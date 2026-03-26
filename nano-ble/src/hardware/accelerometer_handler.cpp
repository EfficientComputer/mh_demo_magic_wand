#include "accelerometer_handler.h"

// Internal buffer and sampling state.
float g_accel_ring[kAccelRingBufferSize] = {0.0f};
int begin_index = 0;                    // next write position (advances by 3 per sample)
static bool pending_initial_data = true; // true until at least ~200 floats stored
static int sample_every_n = 1;           // keep every Nth raw sample
static int sample_skip_counter = 1;      // counts samples between writes

bool AccelerometerSetup(float targetHz) {
  if (!IMU.begin()) {
    return false;
  }
  IMU.setContinuousMode();
  float sample_rate = IMU.accelerationSampleRate();
  if (sample_rate <= 0.0f) sample_rate = targetHz;
  sample_every_n = (int)roundf(sample_rate / targetHz);
  if (sample_every_n < 1) sample_every_n = 1;
  return true;
}

// Poll IMU for new samples; returns number of new triplets appended.
int AccelerometerPoll() {
  int added = 0;
  while (IMU.accelerationAvailable()) {
    float x,y,z;
    if (!IMU.readAcceleration(x,y,z)) break;
    if (sample_skip_counter != sample_every_n) {
      sample_skip_counter++;
      continue;
    }
    // Axis remapping (device orientation). Adjust if needed.
    const float norm_x = -z;
    const float norm_y = y;
    const float norm_z = x;
    g_accel_ring[begin_index++] = norm_x * 1000.0f;
    g_accel_ring[begin_index++] = norm_y * 1000.0f;
    g_accel_ring[begin_index++] = norm_z * 1000.0f;
    sample_skip_counter = 1;
    if (begin_index >= kAccelRingBufferSize) begin_index = 0;
    added++;
  }
  if (added > 0 && pending_initial_data && begin_index >= 200) {
    pending_initial_data = false;
  }
  return added;
}

// DEAD CODE: Backward-compatible window copy never called in codebase
// bool AccelerometerRead(float* input, int length) {
//   int new_triplets = AccelerometerPoll();
//   if (new_triplets <= 0) return false; // nothing new
//   if (pending_initial_data) return false; // not enough initial data yet
//   if (length > kAccelRingBufferSize) length = kAccelRingBufferSize;
//   for (int i = 0; i < length; ++i) {
//     int ring_index = begin_index + i - length;
//     if (ring_index < 0) ring_index += kAccelRingBufferSize;
//     input[i] = g_accel_ring[ring_index];
//   }
//   return true;
// }

const float* AccelerometerRingBuffer() { return g_accel_ring; }
int AccelerometerRingBufferSize() { return kAccelRingBufferSize; }
int AccelerometerBeginIndex() { return begin_index; }
