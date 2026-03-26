#include "utils.h"
#include "../model/model_constants.h"

void PrintStateChange(const GestureCapture &capture, Stream &s)
{
  if (!capture.StateChanged()) return;
  s.print("State: ");
  switch (capture.State())
  {
    case kPendingStillness:   s.println("PendingStillness"); break;
    case kInStillness:        s.println("InStillness"); break;
    case kReady:              s.println("Ready"); break;
    case kCapturing:          s.println("Capturing"); break;
    case kRepositioning:      s.println("Repositioning"); break;
  }
}

// DEAD CODE: Debug function never called in codebase
// void DebugDumpRawSamples(const GestureSequence &sequence,
//                          const GestureConfig &cfg,
//                          const float *gesture,
//                          Stream &s)
// {
//   if (!sequence.InProgress())
//   {
//     for (int i = 0; i < cfg.window_samples; ++i)
//     {
//       int base = i * 3;
//       s.print(gesture[base]);
//       s.print(',');
//       s.print(gesture[base + 1]);
//       s.print(',');
//       s.println(gesture[base + 2]);
//     }
//     s.println("-- End of gesture --");
//   }
// }

void AnnounceNextGesture(GestureSequence &sequence,
                         const GestureCapture &capture,
                         Stream &s)
{
  if (sequence.InProgress() && capture.State() == kReady &&
      sequence.TimeGateSatisfied() && !sequence.HasAnnouncedCurrent())
  {
    int expected = sequence.ExpectedGesture();
    s.print("Sequence perform gesture ");
    s.print(sequence.CurrentIndex() + 1);
    s.print("/");
    s.print(sequence.Total());
    s.print(": ");
    s.print(kGestureNames[expected]);
    s.print(" (index=");
    s.print(expected);
    s.println(") - begin movement.");
    sequence.MarkAnnounced();
  }
}

void LogGesturePrediction(int predicted,
                          const float *scores,
                          int scoreCount,
                          Stream &s)
{
  s.print("Predicted: ");
  if (predicted >= 0 && predicted < kGestureCount) {
    s.print(kGestureNames[predicted]);
  } else {
    s.print("<invalid>");
  }
  s.print(" (index=");
  s.print(predicted);
  s.print(") scores: [");
  for (int i = 0; i < scoreCount; ++i) {
    s.print(scores[i], 3);
    if (i < scoreCount - 1) s.print(',');
  }
  s.println("]");
}

void LogSequenceStep(const GestureSequence &sequence,
                     int completed_step,
                     int expected,
                     int predicted,
                     Stream &s)
{
  s.print("Sequence step ");
  s.print(completed_step);
  s.print("/");
  s.print(sequence.Total());
  s.print(" expected=");
  s.print(kGestureNames[expected]);
  s.print(" predicted=");
  s.println(kGestureNames[predicted]);
}
