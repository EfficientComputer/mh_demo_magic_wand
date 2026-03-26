#ifndef EXPORT_MODEL_CONSTANTS_H_
#define EXPORT_MODEL_CONSTANTS_H_

// Gesture meta constants (mirrors original example but kept self-contained).
constexpr int kGestureCount = 4;
constexpr int kWingGesture = 0;
constexpr int kRingGesture = 1;
constexpr int kSlopeGesture = 2;
constexpr int kNoGesture = 3;

// Input window configuration.
constexpr int kWindowSamples = 128;                           // samples per gesture window
constexpr int kChannelCount = 3;                              // accelerometer channels
constexpr int kFeatureCount = kWindowSamples * kChannelCount; // 384 floats

// Detection threshold (score must exceed unless index == kNoGesture).
constexpr float kDetectionThreshold = 0.8f;

// Human-readable labels.
inline const char *kGestureNames[kGestureCount] = {
    "wing",  // 0
    "ring",  // 1
    "slope", // 2
    "none"   // 3
};

#endif // EXPORT_MODEL_CONSTANTS_H_
