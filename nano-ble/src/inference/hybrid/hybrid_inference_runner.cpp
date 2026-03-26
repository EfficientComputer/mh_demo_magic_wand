#include "hybrid_inference_runner.h"
#include <Arduino.h>

bool HybridInferenceRunner::Init() {
  return Init(&Serial1, Config());
}

bool HybridInferenceRunner::Init(Stream* serial, const Config& cfg) {
  local_ready_ = local_runner_.Init();
  remote_ready_ = remote_runner_.Init(serial, cfg.remote_cfg);
  
  if (!local_ready_) {
    Serial.println("WARNING: Local inference init failed in hybrid mode");
  }
  if (!remote_ready_) {
    Serial.println("WARNING: Remote inference init failed in hybrid mode");
  }
  
  return local_ready_;
}

int HybridInferenceRunner::Predict(const float* window, int window_len, float* scores_out) {
  float remote_scores[kGestureCount] = {0};
  float local_scores[kGestureCount] = {0};
  
  int remote_gesture = kNoGesture;
  int local_gesture = kNoGesture;
  
  if (remote_ready_) {
    remote_gesture = remote_runner_.Predict(window, window_len, remote_scores);
  }
  
  if (local_ready_) {
    local_gesture = local_runner_.Predict(window, window_len, local_scores);
  }
  
  Serial.print("Local: ");
  Serial.print(kGestureNames[local_gesture]);
  Serial.print(" | Remote: ");
  Serial.print(kGestureNames[remote_gesture]);
  
  if (remote_gesture == kNoGesture) {
    Serial.println(" -> Using LOCAL");
    if (scores_out) {
      for (int i = 0; i < kGestureCount; ++i) scores_out[i] = local_scores[i];
    }
    return local_gesture;
  }
  
  Serial.println(" -> Using REMOTE");
  if (scores_out) {
    for (int i = 0; i < kGestureCount; ++i) scores_out[i] = remote_scores[i];
  }
  return remote_gesture;
}
