#ifndef GESTURE_STATE_H_
#define GESTURE_STATE_H_

#include <Arduino.h>
#include <math.h>

// Gesture capture states.
enum GestureState {
  kPendingStillness,
  kInStillness,
  kReady,
  kCapturing,
  kRepositioning
};

// Configuration parameters (tunable).
struct GestureConfig {
  float gravity_mGs = 1000.0f;            // Expected resting magnitude
  float motion_start_threshold = 45.0f;   // Must exceed to start movement
  float motion_stop_threshold  = 35.0f;   // Must go below to stop movement
  int stillness_consecutive_samples = 5;  // Required consecutive still samples
  int stillness_hold_ticks = 15;          // Stabilization ticks after stillness
  int window_samples = 128;               // Samples per gesture window
  int reposition_ticks = 50;              // Cooldown ticks after capture (~2s at 25Hz)
};

class GestureCapture {
public:
  void Init(const GestureConfig& cfg);
  // Update state machine with number of newly appended triplets and ring buffer info.
  // Returns true if a gesture window completed on this call (also query ReadyForInference()).
  bool Update(int new_triplets,
              const float* ring,
              int ring_buffer_size,
              int begin_index);
  // DEAD CODE: Deprecated method never called - use ReadyForInference()
  // bool GestureComplete() const { return gesture_complete_; }
  bool ReadyForInference() const { return gesture_complete_; }
  GestureState State() const { return state_; }
  bool StateChanged() const { return state_changed_; }
  void CopyCaptured(float* out) const; // out length = window_samples * 3

private:
  GestureConfig cfg_;
  GestureState state_;
  bool moving_;
  int stillness_samples_;
  int capture_count_;
  bool gesture_complete_;
  bool state_changed_;
  int sample_counter_;
  int in_state_start_tick_;
  int reposition_start_tick_;
  int ready_start_tick_;
  bool armed_for_capture_;
  float captured_[128 * 3]; // Supports max window_samples=128
 
   bool MovementDecision(float x, float y, float z);
   void CaptureLatestTriplets(const float* ring,
                              int ring_buffer_size,
                              int begin_index,
                              int new_triplets);
 public:
   void SetArmedForCapture(bool armed) { armed_for_capture_ = armed; }
 };


#endif // GESTURE_STATE_H_
