#ifndef UTILS_H_
#define UTILS_H_

#include <Arduino.h>
#include "../gesture/gesture_state.h"
#include "../gesture/gesture_sequence.h"
#include "../model/model_constants.h"

// Emits state change messages for gesture capture state machine.
void PrintStateChange(const GestureCapture &capture, Stream &s = Serial);

// DEAD CODE: Debug function never called in codebase
// void DebugDumpRawSamples(const GestureSequence &sequence,
//                          const GestureConfig &cfg,
//                          const float *gesture,
//                          Stream &s = Serial);

void AnnounceNextGesture(GestureSequence &sequence,
                         const GestureCapture &capture,
                         Stream &s = Serial);

// Logs predicted gesture and per-gesture scores.
void LogGesturePrediction(int predicted,
                          const float *scores,
                          int scoreCount = kGestureCount,
                          Stream &s = Serial);

void LogSequenceStep(const GestureSequence &sequence,
                     int completed_step,
                     int expected,
                     int predicted,
                     Stream &s = Serial);

#endif // UTILS_H_
