#include "gesture_state.h"

void GestureCapture::Init(const GestureConfig& cfg) {
  cfg_ = cfg;
  state_ = kPendingStillness;
  moving_ = false;
  stillness_samples_ = 0;
  capture_count_ = 0;
  gesture_complete_ = false;
  state_changed_ = true;
  sample_counter_ = 0;
  in_state_start_tick_ = 0;
  reposition_start_tick_ = 0;
  ready_start_tick_ = 0;
  armed_for_capture_ = false;
}

bool GestureCapture::MovementDecision(float x, float y, float z) {
  float mag = sqrtf(x*x + y*y + z*z);
  float movement = fabsf(mag - cfg_.gravity_mGs);
  if (moving_) {
    if (movement < cfg_.motion_stop_threshold) moving_ = false;
  } else {
    if (movement > cfg_.motion_start_threshold) moving_ = true;
  }
  return moving_;
}

void GestureCapture::CaptureLatestTriplets(const float* ring,
                                           int ring_buffer_size,
                                           int begin_index,
                                           int new_triplets) {
  if (state_ != kCapturing) return;
  // Compute where the newest triplets start.
  int first_new_triplet_base = begin_index - new_triplets * 3;
  while (first_new_triplet_base < 0) first_new_triplet_base += ring_buffer_size;
  for (int t = 0; t < new_triplets; ++t) {
    if (capture_count_ >= cfg_.window_samples) break;
    int triplet_base = (first_new_triplet_base + t * 3) % ring_buffer_size;
    captured_[capture_count_ * 3 + 0] = ring[triplet_base + 0];
    captured_[capture_count_ * 3 + 1] = ring[triplet_base + 1];
    captured_[capture_count_ * 3 + 2] = ring[triplet_base + 2];
    ++capture_count_;
    if (capture_count_ >= cfg_.window_samples) {
      gesture_complete_ = true;
      state_ = kRepositioning;
      state_changed_ = true;
      reposition_start_tick_ = sample_counter_;
      break;
    }
  }
}

bool GestureCapture::Update(int new_triplets,
                            const float* ring,
                            int ring_buffer_size,
                            int begin_index) {
  gesture_complete_ = false;
  state_changed_ = false;
  if (new_triplets <= 0) return false;
  sample_counter_ += new_triplets;

  // Use last new sample for movement decision.
  int last_triplet_base = begin_index - 3;
  while (last_triplet_base < 0) last_triplet_base += ring_buffer_size;
  float x = ring[last_triplet_base + 0];
  float y = ring[last_triplet_base + 1];
  float z = ring[last_triplet_base + 2];
  bool moving_now = MovementDecision(x,y,z);

  switch (state_) {
    case kPendingStillness: {
      if (!moving_now) {
        stillness_samples_++;
        if (stillness_samples_ >= cfg_.stillness_consecutive_samples) {
          state_ = kInStillness;
          state_changed_ = true;
          in_state_start_tick_ = sample_counter_;
        }
      } else {
        stillness_samples_ = 0;
      }
    } break;

    case kInStillness: {
      if (moving_now) {
        state_ = kPendingStillness;
        state_changed_ = true;
        stillness_samples_ = 0;
      } else {
        int held = sample_counter_ - in_state_start_tick_;
        if (held >= cfg_.stillness_hold_ticks) {
          state_ = kReady;
          state_changed_ = true;
          ready_start_tick_ = sample_counter_;
        }
      }
    } break;

    case kReady: {
      const int kReadyMinDwellTicks = 4;  // ~160ms at 25Hz
      int elapsed = sample_counter_ - ready_start_tick_;
      bool dwell_ok = (elapsed >= kReadyMinDwellTicks);

      // If the host has sent a GO command while we are in the ready
      // window, begin capturing immediately once the minimum dwell
      // time has passed, without requiring a fresh motion trigger.
      if (armed_for_capture_ && dwell_ok) {
        armed_for_capture_ = false; // consume GO for this gesture
        state_ = kCapturing;
        state_changed_ = true;
        capture_count_ = 0;
        CaptureLatestTriplets(ring, ring_buffer_size, begin_index, new_triplets);
      }
    } break;


    case kCapturing: {
      CaptureLatestTriplets(ring, ring_buffer_size, begin_index, new_triplets);
    } break;

    case kRepositioning: {
      int elapsed = sample_counter_ - reposition_start_tick_;
      if (elapsed >= cfg_.reposition_ticks) {
        state_ = kPendingStillness;
        state_changed_ = true;
        stillness_samples_ = 0;
        moving_ = false;
        capture_count_ = 0;
      }
    } break;
  }

  return gesture_complete_;
}

void GestureCapture::CopyCaptured(float* out) const {
  int total = cfg_.window_samples * 3;
  for (int i = 0; i < total; ++i) out[i] = captured_[i];
}
