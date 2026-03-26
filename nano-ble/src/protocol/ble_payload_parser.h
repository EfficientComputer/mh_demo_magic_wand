#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../gesture/gesture_sequence.h"
#include "../model/model_constants.h"

// Incoming BLE sequence definition payload.
// Format (written to BLE write characteristic):
//   [count][g0][g1]...[g(count-1)][auxSeconds]
// Constraints:
//   - count: 1..kMaxSequenceGestures and (count + 2) <= 20 (write char size)
//   - g(i): gesture index 0..(kGestureCount-1)
//   - auxSeconds: 1..10 seconds additional delay between gestures
// Returns ACK codes below on write (sent via ACK characteristic).

static const uint8_t GGI_MAX_ITEMS = kMaxSequenceGestures;
static const uint8_t GGI_MIN_AUX_SEC = 1;
static const uint8_t GGI_MAX_AUX_SEC = 10;

// ACK / error codes
static const uint8_t ACK_ERR_PARSE = 0xFC;   // too short (<3 bytes)
static const uint8_t ACK_ERR_COUNT = 0xFD;   // count invalid
static const uint8_t ACK_ERR_LEN   = 0xFE;   // length mismatch vs count+2
static const uint8_t ACK_ERR_VALUE = 0xFA;   // gesture index out of range
static const uint8_t ACK_ERR_AUX   = 0xF9;   // auxSeconds out of range
static const uint8_t ACK_ERR_BUSY  = 0xF8;   // sequence already in progress
static const uint8_t ACK_READY_STILLNESS = 0xF7;   // entered stillness (user-facing: "Ready")
static const uint8_t ACK_GO_OK    = 0xF6;   // GO command accepted (arm capture)
static const uint8_t ACK_RESET_OK  = 0xFB;   // sequence reset acknowledged
static const uint8_t ACK_HEARTBEAT = 0xFF;   // heartbeat (not a parse result)


// Parse raw buffer into GestureGameInputPayload (declared in gesture_sequence.h).
// Success: returns count (1..GGI_MAX_ITEMS). Failure: returns ACK_ERR_* code.
uint8_t parseGestureGameInputPayload(const uint8_t *buf, int len, GestureGameInputPayload &out);
