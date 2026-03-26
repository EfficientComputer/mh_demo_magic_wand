#ifndef INFERENCE_RUNNER_H_
#define INFERENCE_RUNNER_H_

// Common inference runner interface allowing interchangeable local vs remote
// implementations without changing capture/inference call sites.
class IInferenceRunner {
public:
  virtual ~IInferenceRunner() {}
  // Prepare underlying resources; returns true if ready for Predict().
  virtual bool Init() = 0;
  // Run inference on a feature window. 'scores_out' optional buffer length >= gesture count.
  // Returns predicted gesture index (kNoGesture on failure).
  virtual int Predict(const float* window, int window_len, float* scores_out = nullptr) = 0;
};

#endif // INFERENCE_RUNNER_H_
