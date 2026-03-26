#ifndef EXPORT_MODEL_RUNNER_H_
#define EXPORT_MODEL_RUNNER_H_

#include <stdint.h>

// Lightweight wrapper around TFLite Micro interpreter for the gesture model.
class ModelRunner {
public:
  // Initialize interpreter and verify input tensor shape.
  bool Init();
  // Copy 'window' (expected length = kFeatureCount) into model input and invoke.
  // Returns predicted gesture index (kNoGesture on failure). Optionally fills
  // scores_out (length >= kGestureCount) with raw output scores.
  int Predict(const float* window, int window_len, float* scores_out = nullptr);
  // DEAD CODE: Accessor methods never called in codebase
  // const float* Scores() const { return last_scores_; }
  // int ScoreCount() const { return score_count_; }
private:
  bool ready_ = false;
  const float* last_scores_ = nullptr;
  int score_count_ = 0;
};

#endif // EXPORT_MODEL_RUNNER_H_
