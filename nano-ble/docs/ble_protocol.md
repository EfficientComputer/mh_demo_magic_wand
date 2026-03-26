# BLE Protocol Documentation

## Overview

The Arduino Nano 33 BLE Sense device acts as a BLE Peripheral that accepts gesture sequence definitions from a Central (Raspberry Pi), performs gesture capture and inference, and returns prediction results.

## BLE Service Configuration

**Device Name:** `NanoGesture`

**Service UUID:** `12345678-1234-5678-1234-56789abcdef0`

### Characteristics

| Characteristic | UUID | Direction | Type | Size | Description |
|----------------|------|-----------|------|------|-------------|
| **Write** | `12345678-1234-5678-1234-56789abcdef1` | Central → Peripheral | Write/WriteWithoutResponse | 20 bytes | Send commands and sequence definitions |
| **ACK** | `12345678-1234-5678-1234-56789abcdef2` | Peripheral → Central | Notify | 1 byte | Acknowledgments, errors, and heartbeat |
| **Data Out** | `12345678-1234-5678-1234-56789abcdef3` | Peripheral → Central | Notify | 161 bytes | Final prediction results |

## Communication Flow

### 1. Connection
1. App scans for BLE peripherals advertising service UUID
2. App connects to device named "NanoGesture"
3. Device begins sending periodic heartbeats (every 5 seconds)

### 2. Starting a Gesture Sequence

**App → Nano (Write Characteristic):**

```
Format: [count][g0][g1]...[g(count-1)][auxSeconds]
```

**Parameters:**
- `count`: Number of gestures in sequence (1-16)
- `g0...g(count-1)`: Gesture indices (0-3)
  - `0` = Wing gesture
  - `1` = Ring gesture
  - `2` = Slope gesture
  - `3` = No gesture (typically not used in sequences)
- `auxSeconds`: Additional delay between gestures (1-10 seconds)

**Example:**
```
[0x03, 0x00, 0x01, 0x02, 0x02]
```
Means: 3 gestures (Wing, Ring, Slope) with 2 seconds between each

**Nano → App (ACK Characteristic):**

The device responds with one of these acknowledgment codes:

| Code | Constant | Description |
|------|----------|-------------|
| `count` | Success | Sequence accepted (returns count value 1-16) |
| `0xFC` | `ACK_ERR_PARSE` | Payload too short (< 3 bytes) |
| `0xFD` | `ACK_ERR_COUNT` | Invalid count (0 or > 16) |
| `0xFE` | `ACK_ERR_LEN` | Length mismatch (expected count + 2 bytes) |
| `0xFA` | `ACK_ERR_VALUE` | Gesture index out of range (>= 4) |
| `0xF9` | `ACK_ERR_AUX` | auxSeconds out of range (< 1 or > 10) |
| `0xF8` | `ACK_ERR_BUSY` | Sequence already in progress |

### 3. Reset Command

If the app needs to abort an in-progress sequence and start fresh:

**App → Nano (Write Characteristic):**
```
[0x00]
```

**Nano → App (ACK Characteristic):**
```
0xFB (ACK_RESET_OK)
```

The device will:
- Clear any in-progress sequence
- Reset gesture capture state
- Allow a new sequence to be started immediately

**Use Case:** If the app restarts or times out while the nano is still waiting for gestures, the reset command allows resynchronization without physically resetting the device.

### 4. Gesture Capture Process

Once a sequence starts, the device:

1. **Waits for Stillness**: User must hold device still
2. **Sends Ready Signal** (ACK Characteristic):
   ```
   0xF7 (ACK_READY_STILLNESS)
   ```
3. **Captures Gesture**: When user moves device, captures 128 samples at 25Hz
4. **Runs Inference**: Performs ML prediction on captured gesture
5. **Repeats**: Waits for stillness again for next gesture in sequence

### 5. Sequence Completion

When all gestures in the sequence are complete:

**Nano → App (Data Out Characteristic):**

```
Format: [count][pred0][pred1]...[pred(count-1)]
```

**Parameters:**
- `count`: Number of predictions (matches input sequence count)
- `pred0...pred(count-1)`: Predicted gesture indices (0-3)
  - `0` = Wing
  - `1` = Ring
  - `2` = Slope
  - `3` = No gesture (low confidence)

**Example:**
```
[0x03, 0x00, 0x01, 0x02]
```
Means: 3 gestures were predicted as Wing, Ring, Slope

## Heartbeat

**Nano → App (ACK Characteristic):**

Every 5 seconds while connected, the device sends:
```
0xFF (ACK_HEARTBEAT)
```

This allows the app to detect connection health and timeouts.

## State Machine

```
┌─────────────┐
│   Idle      │ ← Device advertising, waiting for connection
└──────┬──────┘
       │ Connection established
       ▼
┌─────────────┐
│  Connected  │ ← Sending heartbeats, waiting for sequence
└──────┬──────┘
       │ Sequence received (ACK: count)
       ▼
┌─────────────────┐
│ Capturing       │ ← Waiting for stillness, capturing gestures
│ Sequence        │   Sending ACK_READY_STILLNESS notifications
└──────┬──────────┘
       │ All gestures captured
       ▼
┌─────────────────┐
│ Sending Results │ ← Sends prediction array via Data Out
└──────┬──────────┘
       │
       ▼
┌─────────────┐
│  Connected  │ ← Back to idle, ready for next sequence
└─────────────┘
```

## Error Handling

### Sequence Already in Progress
If app sends a new sequence while one is active:
- Device responds with `0xF8 (ACK_ERR_BUSY)`
- Solution: Send reset command `[0x00]` first

### Invalid Payload
If payload is malformed:
- Device responds with appropriate error code (`0xFC` - `0xFA`)
- App should display error and allow user to retry

### Connection Loss
If connection drops during sequence:
- Device continues waiting for gestures indefinitely
- Upon reconnection, app should send reset command `[0x00]`

### App Timeout
If app times out waiting for gestures:
- Device may still be waiting for user to perform gestures
- App should send reset command `[0x00]` to synchronize state

## Implementation Notes

### Constraints
- Maximum sequence length: 16 gestures
- Write characteristic size: 20 bytes
- Gesture window: 128 samples @ 25Hz (~5 seconds)
- Cooldown between gestures: configurable 1-10 seconds
- Default heartbeat interval: 5000ms

### Gesture Indices
The system supports 4 gesture classes (indices 0-3):
- 0: Wing
- 1: Ring
- 2: Slope
- 3: None (used when no gesture detected or low confidence)

### Timing Considerations
- Device requires ~600ms after stillness before entering "Ready" state
- Gesture capture takes ~5 seconds (128 samples at 25Hz)
- Additional cooldown period between gestures: `auxSeconds` (1-10s)
- Heartbeat sent every 5 seconds during connection
