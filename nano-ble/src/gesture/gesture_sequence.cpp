#include "gesture_sequence.h"

int GestureSequence::PredictionAt(int idx) const {
  if (idx < 0 || idx >= total_) return kNoGesture;
  return predicted_[idx];
}

// DEAD CODE: Stub for non-Arduino builds, but project only builds for Arduino
// #ifndef ARDUINO
// class Stream { public: void print(const char*) {} void println(const char*) {} void print(int) {} void println(int) {} }; // NOLINT
// #endif

void GestureSequence::Begin(const GestureGameInputPayload &payload) {
  total_ = (payload.count > kMaxSequenceGestures) ? kMaxSequenceGestures : payload.count;
  for (int i = 0; i < total_; ++i) {
    int v = payload.values[i];
    if (v < 0 || v >= kGestureCount) v = kNoGesture;
    expected_[i] = v;
    predicted_[i] = kNoGesture; // init
    for (int j = 0; j < kGestureCount; ++j) { scores_[i][j] = 0.0f; }
  }
  current_index_ = 0;
  active_ = (total_ > 0);
  completed_ = false;
  announced_current_ = false;
  last_prediction_ms_ = 0;
  aux_delay_ms_ = (unsigned long)payload.auxSeconds * 1000UL;
}

void GestureSequence::Reset() {
  active_ = false;
  completed_ = false;
  total_ = 0;
  current_index_ = 0;
  announced_current_ = false;
  last_prediction_ms_ = 0;
  aux_delay_ms_ = 0;
}

bool GestureSequence::TimeGateSatisfied() const {
  if (!active_ || completed_) return false;
  if (current_index_ == 0 && predicted_[0] == kNoGesture && last_prediction_ms_ == 0) {
    // First gesture: allow immediately.
    return true;
  }
  unsigned long now = millis();
  return (now - last_prediction_ms_) >= aux_delay_ms_;
}

void GestureSequence::RecordPrediction(int predicted, const float* scores, int scoreCount) {
  if (!active_ || completed_ || current_index_ >= total_) return;
  predicted_[current_index_] = predicted;
  // Copy probability scores for this gesture.
  for (int j = 0; j < kGestureCount && j < scoreCount; ++j) {
    scores_[current_index_][j] = scores ? scores[j] : 0.0f;
  }
  last_prediction_ms_ = millis();
  current_index_++;
  announced_current_ = false; // require re-announcement for next gesture
  if (current_index_ >= total_) {
    completed_ = true;
  }
}

void GestureSequence::SummaryToSerial(Stream &s) const {
  if (!active_) {
    s.println("Sequence inactive.");
    return;
  }
  if (!completed_) {
    s.print("Sequence in progress (" );
    s.print(current_index_);
    s.print("/");
    s.print(total_);
    s.println(")");
    return;
  }
  s.println("Sequence complete summary:");
  for (int i = 0; i < total_; ++i) {
    s.print(i); s.print(": expected="); s.print(expected_[i]);
    s.print(" predicted="); s.print(predicted_[i]); s.println("");
  }
}
