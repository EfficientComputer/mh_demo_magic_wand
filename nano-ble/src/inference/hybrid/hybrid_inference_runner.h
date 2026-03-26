#ifndef HYBRID_INFERENCE_RUNNER_H_
#define HYBRID_INFERENCE_RUNNER_H_

#include "../inference_runner.h"
#include "../local/local_inference_runner.h"
#include "../remote/remote_inference_runner.h"
#include "../../model/model_constants.h"

class HybridInferenceRunner : public IInferenceRunner {
public:
  struct Config {
    RemoteInferenceRunner::Config remote_cfg;
    Config() {}
  };

  bool Init() override;
  bool Init(Stream* serial, const Config& cfg = Config());

  int Predict(const float* window, int window_len, float* scores_out = nullptr) override;

private:
  LocalInferenceRunner local_runner_;
  RemoteInferenceRunner remote_runner_;
  bool local_ready_ = false;
  bool remote_ready_ = false;
};

#endif // HYBRID_INFERENCE_RUNNER_H_
