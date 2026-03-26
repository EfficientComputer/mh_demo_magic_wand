#ifndef EXPORT_GESTURE_PREDICTOR_H_
#define EXPORT_GESTURE_PREDICTOR_H_

// REVIEW: Only used in local inference; remote inference duplicates this logic
// Returns gesture index (0..kGestureCount-1). Applies threshold and maps low
// confidence or explicit 'none' class to kNoGesture.
int PredictGesture(const float* scores, int score_count);

#endif // EXPORT_GESTURE_PREDICTOR_H_
