#include "model_runner.h"

#include <stddef.h>
#include "../../model/model_constants.h"
#include "../../model/model_data.h"
#include "../gesture_predictor.h"

// TFLM includes
#include "TensorFlowLite.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"

namespace {
  tflite::ErrorReporter* g_error_reporter = nullptr;
  const tflite::Model* g_model = nullptr;
  tflite::MicroInterpreter* g_interpreter = nullptr;
  TfLiteTensor* g_input = nullptr;
  constexpr int kTensorArenaSize = 60 * 1024;
  static uint8_t g_tensor_arena[kTensorArenaSize];
}

bool ModelRunner::Init() {
  static tflite::MicroErrorReporter micro_error_reporter; // safe static
  g_error_reporter = &micro_error_reporter;
  g_model = tflite::GetModel(g_magic_wand_model_data);
  if (g_model == nullptr) return false;
  if (g_model->version() != 3) return false;
  // Only required ops (same as original example).
  // Increased capacity to accommodate versioned registrations.
  static tflite::MicroOpResolver<15> micro_op_resolver;
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
                               tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
                               tflite::ops::micro::Register_MAX_POOL_2D(),
                               /*min_version=*/1,
                               /*max_version=*/2);
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D(),
                               /*min_version=*/1,
                               /*max_version=*/3);
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                               tflite::ops::micro::Register_FULLY_CONNECTED(),
                               /*min_version=*/1,
                               /*max_version=*/4);
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX(),
                               /*min_version=*/1,
                               /*max_version=*/2);
  static tflite::MicroInterpreter static_interpreter(
      g_model, micro_op_resolver, g_tensor_arena, kTensorArenaSize, g_error_reporter);
  g_interpreter = &static_interpreter;
  if (g_interpreter->AllocateTensors() != kTfLiteOk) return false;
  g_input = g_interpreter->input(0);
  if (!g_input) return false;
  // Expect shape [1,128,3,1] float32.
  if (g_input->dims->size != 4 ||
      g_input->dims->data[0] != 1 ||
      g_input->dims->data[1] != kWindowSamples ||
      g_input->dims->data[2] != kChannelCount ||
      g_input->type != kTfLiteFloat32) {
    return false;
  }
  ready_ = true;
  return true;
}

int ModelRunner::Predict(const float* window, int window_len, float* scores_out) {
  if (!ready_) return kNoGesture;
  const int expected = kFeatureCount;
  if (window_len != expected) return kNoGesture;
  float* input_data = g_input->data.f;
  for (int i = 0; i < expected; ++i) input_data[i] = window[i];
  if (g_interpreter->Invoke() != kTfLiteOk) return kNoGesture;
  TfLiteTensor* output = g_interpreter->output(0);
  last_scores_ = output->data.f;
  score_count_ = output->bytes / sizeof(float);
  if (scores_out) {
    for (int i = 0; i < score_count_ && i < kGestureCount; ++i) scores_out[i] = last_scores_[i];
  }
  return PredictGesture(last_scores_, score_count_);
}
