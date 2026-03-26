#ifndef LOCAL_INFERENCE_RUNNER_H_
#define LOCAL_INFERENCE_RUNNER_H_

#include "../inference_runner.h"
#include "model_runner.h"

// Thin adapter exposing existing ModelRunner via the generic interface.
class LocalInferenceRunner : public IInferenceRunner {
public:
  bool Init() override { return impl_.Init(); }
  int Predict(const float* window, int window_len, float* scores_out = nullptr) override {
    return impl_.Predict(window, window_len, scores_out);
  }
private:
  ModelRunner impl_;
};

#endif // LOCAL_INFERENCE_RUNNER_H_
