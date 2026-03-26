#include "remote_inference_runner.h"
#include <math.h>

bool RemoteInferenceRunner::Init() {
  return Init(&Serial1, Config());
}

bool RemoteInferenceRunner::Init(Stream* serial, const Config& cfg) {
  serial_ = serial;
  cfg_ = cfg;
  if (!serial_) {
    ready_ = false;
    return false;
  }
  ready_ = true;
  if (cfg_.verbose) Log("RemoteInferenceRunner ready (serial attached)");
  return true;
}

void RemoteInferenceRunner::Log(const char* msg) const {
  if (cfg_.verbose) Serial.println(msg);
}

void RemoteInferenceRunner::quantize_sample(const float in[3], int8_t out[3]) const {
  const float inv_scale = 1.0f / kAccScale;
  for (int axis = 0; axis < 3; ++axis) {
    float v = in[axis];
    float qf = v * inv_scale + kAccZeroPoint;
    if (qf < -128.0f) qf = -128.0f;
    if (qf > 127.0f) qf = 127.0f;
    out[axis] = (int8_t)roundf(qf);
  }
}

bool RemoteInferenceRunner::BuildPayload(const float* window, int window_len) {
  if (window_len != kFeatureCount) {
    Log("Window length mismatch");
    return false;
  }
  for (int i = 0; i < kWindowSamples; ++i) {
    const float* sample = &window[i * kChannelCount];
    int8_t q[3];
    quantize_sample(sample, q);
    int base = i * kChannelCount;
    payload_[base + 0] = q[0];
    payload_[base + 1] = q[1];
    payload_[base + 2] = q[2];
  }
  return true;
}

bool RemoteInferenceRunner::SendFrame() {
  if (!serial_) return false;
  serial_->write(kSyncByte);
  serial_->write((const uint8_t*)payload_, kFeatureCount);
  serial_->flush();
  return true;
}

bool RemoteInferenceRunner::ReceiveResponse(uint8_t out[kResponseBytes]) {
  if (!serial_) return false;
  unsigned long start = millis();
  while ((serial_->available() < kResponseBytes) && (millis() - start < cfg_.timeout_ms)) {
    // busy wait
  }
  if (serial_->available() < kResponseBytes) {
    while (serial_->available()) (void)serial_->read();
    timeout_errors_++;
    Log("Timeout waiting for response");
    return false;
  }
  for (int i = 0; i < kResponseBytes; ++i) {
    int v = serial_->read();
    if (v < 0) {
      format_errors_++;
      Log("Negative read byte");
      return false;
    }
    out[i] = (uint8_t)v;
  }
  return true;
}

bool RemoteInferenceRunner::ParseResponse(const uint8_t in[kResponseBytes], int& gesture, float scores[kGestureCount]) {
  gesture = (int)in[0];
  if (gesture < 0 || gesture >= kGestureCount) {
    format_errors_++;
    Log("Gesture ID out of range");
    return false;
  }
  for (int i = 0; i < kGestureCount; ++i) {
    uint8_t c = in[i + 1];
    if (c > 100) {
      format_errors_++;
      Log("Confidence >100");
      return false;
    }
    scores[i] = static_cast<float>(c) / 100.0f;
  }
  return true;
}

int RemoteInferenceRunner::Predict(const float* window, int window_len, float* scores_out) {
  if (!ready_) return kNoGesture;
  if (!BuildPayload(window, window_len)) return kNoGesture;

  int final_gesture = kNoGesture;
  for (uint8_t attempt = 0; attempt <= cfg_.retries; ++attempt) {
    if (!SendFrame()) continue;
    uint8_t raw[kResponseBytes];
    if (!ReceiveResponse(raw)) continue;
    float scores[kGestureCount];
    int gesture;
    if (!ParseResponse(raw, gesture, scores)) continue;

    for (int i = 0; i < kGestureCount; ++i) last_scores_[i] = scores[i];
    if (scores_out) {
      for (int i = 0; i < kGestureCount; ++i) scores_out[i] = scores[i];
    }

    if (gesture != kNoGesture && scores[gesture] < kDetectionThreshold) {
      final_gesture = kNoGesture;
    } else {
      final_gesture = gesture;
    }
    break;
  }

  if (final_gesture == kNoGesture && (timeout_errors_ + format_errors_) > 5) {
    ready_ = false;
    Log("Disabling remote due to repeated errors");
  }
  return final_gesture;
}
