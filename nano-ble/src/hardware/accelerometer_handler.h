#ifndef ACCELEROMETER_HANDLER_H_
#define ACCELEROMETER_HANDLER_H_

// Minimal accelerometer handling extracted from Magic Wand example.
// Provides setup and windowed sample retrieval without TensorFlow Lite deps.
// Call AccelerometerSetup() once in setup(), then repeatedly call
// AccelerometerRead(input_buffer, length) where length is window_size * kChannelNumber.
// Recommended window_size for gesture models is 128 samples (length=384).

#include <Arduino.h>
#include <math.h>

// Replace this include with the IMU library for your board if different.
#include "Arduino_BMI270_BMM150.h" // Provides IMU.begin(), IMU.accelerationSampleRate(), IMU.setContinuousMode(), IMU.accelerationAvailable(), IMU.readAcceleration()

// Number of accelerometer channels (x, y, z)
static const int kChannelNumber = 3;
// Internal ring buffer stores 200 samples * 3 channels.
static const int kAccelRingBufferSize = 600; // floats

// Exposed write index into ring buffer (next write position). Advances by 3 per sample.
extern int begin_index;

// Setup the accelerometer. targetHz is the desired effective sample frequency after downsampling.
// Returns true on success.
bool AccelerometerSetup(float targetHz = 25.0f);

// DEAD CODE: Never called in codebase
// bool AccelerometerRead(float* input, int length);

// Poll IMU and append new samples to internal ring buffer.
// Returns number of new triplets (x,y,z) added this call.
int AccelerometerPoll();

// Accessors for ring buffer inspection.
const float* AccelerometerRingBuffer();
int AccelerometerRingBufferSize();
int AccelerometerBeginIndex();

#endif // ACCELEROMETER_HANDLER_H_
