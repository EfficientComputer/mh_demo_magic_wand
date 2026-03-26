#ifndef REMOTE_INFERENCE_RUNNER_H_
#define REMOTE_INFERENCE_RUNNER_H_

#include <stdint.h>
#include <Arduino.h>
#include "../inference_runner.h"
#include "../../model/model_constants.h" // kFeatureCount, kGestureCount, kNoGesture, kDetectionThreshold

// Remote inference runner: sends a quantized accelerometer window over a serial
// link to a companion MCU that performs inference and returns 5 bytes:
//   [gestureId, conf0, conf1, conf2, conf3]
// gestureId: 0..(kGestureCount-1), confidences: 0..100 (percentage).
// Gesture ordering must match model_constants gesture indices.
// Falls back to local inference if init/predict repeatedly fails (ready_ becomes false).
class RemoteInferenceRunner : public IInferenceRunner {
public:
  struct Config {
    unsigned long timeout_ms; // wait for 5-byte response
    uint8_t retries;          // additional attempts after first send
    bool verbose;             // enable Serial logging
    Config(): timeout_ms(1000), retries(1), verbose(false) {}
  };

  // Default Init(): attaches to Serial1 with default config.
  bool Init() override;
  // Explicit Init allowing caller to supply a Stream (e.g. &Serial1) and config.
  bool Init(Stream* serial, const Config& cfg = Config());

  // Predict: builds + sends frame, waits for response, optionally fills scores_out.
  // Returns gesture index or kNoGesture on failure or below detection threshold.
  int Predict(const float* window, int window_len, float* scores_out = nullptr) override;

  // DEAD CODE: Status accessor methods never called in codebase
  // bool IsReady() const { return ready_; }
  // uint32_t timeoutErrors() const { return timeout_errors_; }
  // uint32_t formatErrors() const { return format_errors_; }
  // const float* LastScores() const { return last_scores_; }

private:
  Stream* serial_ = nullptr; // UART to remote inference MCU
  Config cfg_;
  bool ready_ = false;
  int8_t payload_[kFeatureCount];          // Quantized 128x3 window (384 bytes)
  float last_scores_[kGestureCount] = {0}; // Last received confidences 0.0..1.0
  uint32_t timeout_errors_ = 0;
  uint32_t format_errors_ = 0;

  // Quantization constants (mirrors matrix_transformation.cpp) kept local to avoid cross-dir include.
  static constexpr float kAccScale = 12.452904f;
  static constexpr float kAccZeroPoint = -37.0f;
  static constexpr uint8_t kSyncByte = 0x80;
  static constexpr int kResponseBytes = 5; // gesture + 4 confidences

  void Log(const char* msg) const;
  // Shared-style quantization helper matching signature from evk_integration/matrix_transformation.cpp
  void quantize_sample(const float in[3], int8_t out[3]) const;
  bool BuildPayload(const float* window, int window_len);
  bool SendFrame();
  bool ReceiveResponse(uint8_t out[kResponseBytes]);
  bool ParseResponse(const uint8_t in[kResponseBytes], int& gesture, float scores[kGestureCount]);
};

#endif // REMOTE_INFERENCE_RUNNER_H_

