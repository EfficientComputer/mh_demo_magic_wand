#ifndef GESTURE_SEQUENCE_H_
#define GESTURE_SEQUENCE_H_

#include <Arduino.h>
#include <stdint.h>
#include "../model/model_constants.h"

// Maximum number of gestures supported in a sequence.
constexpr int kMaxSequenceGestures = 16;

// Input payload passed in from higher level (e.g., game logic / host).
// values[] entries use the same indices as model gesture constants (0..kGestureCount-1).
struct GestureGameInputPayload {
  uint8_t count = 0;          // Number of gesture entries present in values[].
  uint8_t auxSeconds = 0;     // Delay between gestures after prediction (additional cooldown).
  uint8_t values[kMaxSequenceGestures]; // Expected gesture indices.
};

class GestureSequence {
public:
  // Begin a new sequence. Copies payload values (clamped to kMaxSequenceGestures).
  void Begin(const GestureGameInputPayload &payload);
  // Reset and deactivate sequence.
  void Reset();

  bool InProgress() const { return active_ && !completed_; }
  bool Completed() const { return completed_; }
  int Total() const { return total_; }
  int CurrentIndex() const { return current_index_; } // Next expected index (0-based)
  int ExpectedGesture() const { return (current_index_ < total_) ? expected_[current_index_] : kNoGesture; }
  int PredictionAt(int idx) const; // Access recorded predictions.

  // Time gating: true when ready for the next gesture (auxSeconds satisfied).
  bool TimeGateSatisfied() const;
  // Announcement flag: helps avoid spamming "perform gesture" prompt.
  bool HasAnnouncedCurrent() const { return announced_current_; }
  void MarkAnnounced() { announced_current_ = true; }

  // Record model prediction and associated probability scores for current expected gesture and advance.
  void RecordPrediction(int predicted, const float* scores, int scoreCount);

  // Access stored probability scores for gesture at index (row of length kGestureCount).
  const float* ScoresAt(int idx) const { return (idx < 0 || idx >= total_) ? nullptr : scores_[idx]; }

  // Emit summary of sequence results.
  void SummaryToSerial(Stream &s) const;

private:
  bool active_ = false;
  bool completed_ = false;
  int total_ = 0;
  int current_index_ = 0; // Points to next expected gesture.
  int expected_[kMaxSequenceGestures];
  int predicted_[kMaxSequenceGestures];
  float scores_[kMaxSequenceGestures][kGestureCount];
  bool announced_current_ = false;
  unsigned long last_prediction_ms_ = 0;
  unsigned long aux_delay_ms_ = 0;
};

#endif // GESTURE_SEQUENCE_H_
