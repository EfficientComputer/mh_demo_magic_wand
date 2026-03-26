// Gesture capture + inference with BLE-managed sequence input and final results packet.
// Sequence definition arrives over BLE; device captures gestures, performs inference,
// and sends final predictions array back when sequence completes.
// Final BLE result format: [count][pred0][pred1]...[pred(count-1)] (indices 0..kGestureCount-1).

#include <Arduino.h>
#include "hardware/accelerometer_handler.h"
#include "gesture/gesture_state.h"
#include "model/model_constants.h"
#include "gesture/gesture_sequence.h"
#include "utils/utils.h"
#include "hardware/ble_manager.h"
#include "protocol/ble_payload_parser.h"

#ifdef USE_HYBRID_INFERENCE
#include "inference/hybrid/hybrid_inference_runner.h"
static HybridInferenceRunner g_hybrid_runner;
static IInferenceRunner *g_runner = &g_hybrid_runner;
#elif defined(USE_REMOTE_INFERENCE)
#include "inference/remote/remote_inference_runner.h"
// Remote inference enabled via USE_REMOTE_INFERENCE
static RemoteInferenceRunner g_remote_runner;
static IInferenceRunner *g_runner = &g_remote_runner;
#else
#include "inference/local/local_inference_runner.h"
static LocalInferenceRunner g_local_runner;
static IInferenceRunner *g_runner = &g_local_runner;
#endif

static GestureConfig g_cfg; // defaults
static GestureCapture g_capture;
static float g_gesture[kFeatureCount]; // 128 * 3 window
static GestureSequence g_sequence;     // sequence controller
static BLEManager g_ble;               // BLE abstraction
static bool g_final_sent = false;      // ensures final packet sent once
static unsigned long g_last_ready_ack_ms = 0; // throttle ACK_READY_STILLNESS while in still/ready window

static void SendFinalPredictions(GestureSequence &sequence, BLEManager &ble, bool &final_sent)
{
  if (final_sent)
    return;

  uint8_t sequence_count = (uint8_t)sequence.Total();
  if (sequence_count + 1 > BLEManager::DATA_OUT_CAPACITY)
  {
    sequence_count = BLEManager::DATA_OUT_CAPACITY - 1;
    Serial.println("WARNING: sequence truncated in BLE predictions packet due to capacity.");
  }
  if (sequence_count > 0)
  {
    uint8_t buf[BLEManager::DATA_OUT_CAPACITY];
    buf[0] = sequence_count;
    for (uint8_t i = 0; i < sequence_count; ++i)
    {
      int pred = sequence.PredictionAt(i);
      if (pred < 0 || pred >= kGestureCount)
        pred = kNoGesture;
      buf[i + 1] = (uint8_t)pred;
    }
    ble.sendData(buf, sequence_count + 1);
    Serial.print("Final predictions sent over BLE (count=");
    Serial.print(sequence_count);
    Serial.println(")");
  }
  final_sent = true;
}

// BLE write handler: parses sequence payload and begins sequence if idle.
static void HandleBleWrite(const uint8_t *buf, int len)
{
  GestureGameInputPayload payload;
  uint8_t ack = parseGestureGameInputPayload(buf, len, payload);
  
  if (ack == ACK_RESET_OK)
  {
    g_sequence.Reset();
    g_capture.Init(g_cfg);
    g_final_sent = false;
    g_ble.sendAck(ACK_RESET_OK);
    Serial.println("Sequence reset by app.");
    return;
  }

  // GO command: arm capture for the next gesture without altering sequence.
  if (ack == ACK_GO_OK)
  {
    // Arm capture now; the web stack no longer depends on a specific
    // GO acknowledgement byte, so we simply arm the capture logic and
    // rely on the final results packet as confirmation.
    g_capture.SetArmedForCapture(true);
    Serial.println("GO command received: capture armed (no GO ACK emitted).");
    return;
  }
  
  if (g_sequence.InProgress())
  {
    g_ble.sendAck(ACK_ERR_BUSY);
    return;
  }
  
  g_ble.sendAck(ack);
  if (ack == payload.count && payload.count > 0)
  {
    g_sequence.Begin(payload);
    g_capture.Init(g_cfg);
    g_final_sent = false;
    Serial.print("Sequence started with ");
    Serial.print(payload.count);
    Serial.println(" gestures.");
  }
}

// One-time initialization: configure hardware (accelerometer, BLE) and inference engine.
// This function runs once when the Arduino powers on or resets.
// If any critical component fails to initialize, the device halts in an infinite loop.
void setup()
{
  // Initialize serial communication for debugging output at 115200 baud.
  Serial.begin(115200);
  Serial.println("Booting gesture device (BLE sequence mode)...");

  // Initialize the IMU (accelerometer) at 25Hz sampling rate.
  // If setup fails, halt execution - the device cannot function without accelerometer data.
  if (!AccelerometerSetup(25.0f))
  {
    Serial.println("IMU setup failed. Check wiring or library.");
    while (true)
    {
      delay(1000);
    }
  }

  // Configure gesture capture parameters: window size and cooldown period between gestures.
  g_cfg.window_samples = kWindowSamples;
  g_cfg.reposition_ticks = 50; // ~2s cooldown at 25Hz
  g_capture.Init(g_cfg);
  Serial.println("Accelerometer initialized. Awaiting BLE sequence definition before capture...");

  // Initialize the inference engine based on compile-time configuration.
  // USE_HYBRID_INFERENCE: run both local and remote inference with fallback logic.
  // USE_REMOTE_INFERENCE: offload ML inference to external device via Serial1.
  // Otherwise: run TensorFlow Lite model locally on this device.
#ifdef USE_HYBRID_INFERENCE
  Serial1.begin(9600);
  HybridInferenceRunner::Config hybrid_cfg;
  hybrid_cfg.remote_cfg.timeout_ms = 1000;
  hybrid_cfg.remote_cfg.retries = 2;
  hybrid_cfg.remote_cfg.verbose = true;
  if (!g_hybrid_runner.Init(&Serial1, hybrid_cfg))
  {
    Serial.println("Hybrid inference initialization FAILED. Aborting.");
    while (true)
    {
      delay(1000);
    }
  }
#elif defined(USE_REMOTE_INFERENCE)
  Serial1.begin(9600);
  RemoteInferenceRunner::Config remote_cfg;
  remote_cfg.timeout_ms = 1000;
  remote_cfg.retries = 2;
  remote_cfg.verbose = true;
  if (!g_remote_runner.Init(&Serial1, remote_cfg))
  {
    Serial.println("Remote inference initialization FAILED. Aborting.");
    while (true)
    {
      delay(1000);
    }
  }
#else
  if (!g_runner->Init())
  {
    Serial.println("Inference initialization FAILED. Aborting.");
    while (true)
    {
      delay(1000);
    }
  }
#endif
  Serial.println("Inference initialized.");

  // Start BLE peripheral with service/device name "NanoGesture".
  // Register the write handler to receive gesture sequence definitions from connected clients.
  if (!g_ble.begin("NanoGesture", "NanoGesture"))
  {
    Serial.println("BLE init failed.");
    while (true)
    {
      delay(1000);
    }
  }
  g_ble.setWriteHandler(HandleBleWrite);
  Serial.println("BLE advertising; awaiting sequence payload...");
}

// Main event loop: continuously poll BLE, sample accelerometer, run state machine,
// perform inference, and manage gesture sequence progression.
// Each "return" statement exits the current loop iteration early, allowing the loop
// to restart quickly and check for new events (BLE writes, accelerometer samples, etc.)
// without blocking or waiting. This keeps the system responsive.
void loop()
{
  // Always poll BLE first to handle incoming sequence writes / heartbeats.
  // This ensures we can receive new sequences and maintain connection.
  g_ble.poll();

  // Gate capture/inference until a sequence is active.
  // If no sequence is in progress, exit early (return) to continue polling BLE
  // and wait for a sequence definition to arrive. This return prevents unnecessary
  // accelerometer processing when the device is idle.
  if (!g_sequence.InProgress())
  {
    // BLE heartbeats + writes still processed above via g_ble.poll().
    return; // No sampling or inference without defined gesture sequence.
  }

  // Poll accelerometer for any newly available samples.
  // If no new data is available, exit early (return) to avoid wasting CPU cycles.
  // This keeps the loop tight and responsive to new accelerometer readings.
  int new_triplets = AccelerometerPoll();
  if (new_triplets <= 0)
    return; // keep loop tight

  // Advance capture state machine with the new accelerometer data.
  // The state machine tracks motion detection, gesture windows, and stillness periods.
  g_capture.Update(new_triplets,
                   AccelerometerRingBuffer(),
                   AccelerometerRingBufferSize(),
                   AccelerometerBeginIndex());

  PrintStateChange(g_capture);

  // While in the "ready for next gesture" state (kReady only),
  // periodically emit ACK_READY_STILLNESS so the host can reliably
  // synchronize even if it starts waiting late or misses an earlier
  // notification. We intentionally do NOT emit this while in kInStillness,
  // since that represents an earlier pre-ready stillness phase.
  static GestureState prev_state = kPendingStillness;
  GestureState s = g_capture.State();
 
  bool in_ready_state = (s == kReady);


  if (g_sequence.InProgress() && in_ready_state)
  {
    unsigned long now = millis();
    // Emit ACK_READY_STILLNESS frequently while in the ready window so
    // the host can reliably observe it even if the window is brief or the
    // host starts waiting slightly late.
    if (g_last_ready_ack_ms == 0 || (now - g_last_ready_ack_ms) >= 50UL)
    {
      g_ble.sendAck(ACK_READY_STILLNESS);
      g_last_ready_ack_ms = now;
    }
  }
  else
  {
    g_last_ready_ack_ms = 0;
  }

  AnnounceNextGesture(g_sequence, g_capture);


  // Wait until a complete gesture window has been captured and validated.
  // ReadyForInference() returns true when the state machine has captured
  // a full window of accelerometer data following detected motion.
  if (!g_capture.ReadyForInference())
    return;

  // Copy the captured accelerometer window (kFeatureCount = 128 samples * 3 axes)
  // into the inference input buffer and run the ML model to predict the gesture type.
  g_capture.CopyCaptured(g_gesture);
  int predicted = kNoGesture;
  float scores[kGestureCount] = {0};
  predicted = g_runner->Predict(g_gesture, kFeatureCount, scores);
  LogGesturePrediction(predicted, scores);

  // If we're tracking a multi-gesture sequence, record this prediction and
  // advance the sequence state. Check if the entire sequence is now complete.
  if (g_sequence.InProgress())
  {
    // Store the expected gesture before recording (for logging purposes).
    int expected_before = g_sequence.ExpectedGesture();

    // Record the prediction and confidence scores, advancing to the next expected gesture.
    g_sequence.RecordPrediction(predicted, scores, kGestureCount);
    int completed_step = g_sequence.CurrentIndex();

    // Log the step result showing expected vs. actual prediction.
    LogSequenceStep(g_sequence, completed_step, expected_before, predicted);

    // When the full sequence completes, print summary statistics and send
    // the final predictions array over BLE to the connected client.
    if (g_sequence.Completed())
    {
      g_sequence.SummaryToSerial(Serial);
      SendFinalPredictions(g_sequence, g_ble, g_final_sent);
    }
  }
}
