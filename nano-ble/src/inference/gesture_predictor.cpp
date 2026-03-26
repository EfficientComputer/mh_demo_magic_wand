#include "gesture_predictor.h"
#include "../model/model_constants.h"

// REVIEW: Only used in local inference (model_runner.cpp:75); remote inference duplicates this logic
int PredictGesture(const float* scores, int score_count) {
  int best = -1;
  float best_score = 0.0f;
  for (int i = 0; i < score_count && i < kGestureCount; ++i) {
    float s = scores[i];
    if (best < 0 || s > best_score) { best = i; best_score = s; }
  }
  if (best < 0) return kNoGesture;
  if (best == kNoGesture || best_score < kDetectionThreshold) return kNoGesture;
  return best;
}
