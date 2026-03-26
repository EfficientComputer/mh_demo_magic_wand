
# Efficient Computer "Magic Wand" - Arduino Nano 33 BLE

## Overview

This is a gesture recognition demo application for Efficient Computer's tradeshow booth. It showcases machine learning inference on embedded hardware using a TensorFlow Lite model running on an Arduino Nano 33 BLE.

### Main Functionality

The application captures motion gestures using the onboard IMU (accelerometer) and classifies them using a trained TensorFlow Lite Micro model. It supports:

- **Gesture Capture**: State machine-based motion detection that identifies stillness, triggers on movement, and captures 128 samples (3-axis accelerometer data) per gesture
- **On-Device Inference**: Local TensorFlow Lite Micro model inference running directly on the nRF52840 microcontroller
- **Remote Inference**: Optional UART-based offloading to a dedicated inference accelerator MCU for power/performance comparison
- **BLE Protocol**: Wireless control and data exchange via Bluetooth LE, allowing a host device to define gesture sequences and receive predictions
- **Gesture Sequences**: Support for multi-gesture sequences with configurable delays, enabling interactive "Simon Says" style demos

The device advertises as `NanoGesture` and exposes characteristics for sequence definition input, acknowledgment/status output, and prediction results.

## Target Hardware
Arduino Nano 33 BLE Rev 2 (nRF52840)

## Project Layout
```
platformio.ini                               # Environment configuration
src/
  main.cpp                                   # Main application entry point
  gesture/
    gesture_sequence.{cpp,h}                 # Sequence definition and management
    gesture_state.{cpp,h}                    # State machine for gesture capture
  hardware/
    accelerometer_handler.{cpp,h}            # IMU data acquisition
    ble_manager.{cpp,h}                      # BLE service and characteristics
  inference/
    gesture_predictor.{cpp,h}                # High-level prediction orchestration
    inference_runner.h                       # Abstract interface for inference
    local/
      local_inference_runner.{cpp,h}         # On-device TFLite Micro wrapper
      model_runner.{cpp,h}                   # TFLite model execution
    remote/
      remote_inference_runner.{cpp,h}        # Remote inference stub
  model/
    model_constants.h                        # Model hyperparameters
    model_data.{cpp,h}                       # Embedded TFLite model
  protocol/
    ble_payload_parser.{cpp,h}               # BLE protocol handlers
  utils/
    utils.{cpp,h}                            # Shared utilities
lib/
  tensorflow_lite/                           # TensorFlow Lite for Microcontrollers
```

## Setup

### Install PlatformIO

VS Code (GUI):
1. Install VS Code
2. Open Extensions panel, search for 'PlatformIO IDE', install
3. Click the alien head icon (PlatformIO Home) to verify install

CLI (Python):
```bash
pip install -U platformio
pio --version
```

macOS (Homebrew alternative):
```bash
brew install platformio
```
(Homebrew formula can lag behind the latest release.)

### Libraries

PlatformIO auto-installs libraries declared under `lib_deps` in `platformio.ini`:
```ini
lib_deps =
  arduino-libraries/Arduino_BMI270_BMM150@^1.2.1
  arduino-libraries/ArduinoBLE@^1.4.1
```
No manual action needed. To force update/reinstall:
```bash
pio pkg update --environment nano33ble
```
To add another library, append its identifier + version under `lib_deps` and rebuild:
```ini
lib_deps =
  arduino-libraries/Arduino_BMI270_BMM150@^1.2.1
  arduino-libraries/ArduinoBLE@^1.4.1
  vendor/LibraryName@^x.y.z
```

### Common Commands

Build: `pio run`  
Upload: `pio run -t upload`  
Serial monitor: `pio device monitor --baud 115200`

Troubleshooting:
- If board not detected, add `upload_port = /dev/tty.usbmodemXXXX` under `[env:nano33ble]`.
- Double-tap reset if upload fails to enter bootloader.
- Ensure Python 3.7+ if using the CLI.

## Build
```bash
pio run
```

## Upload
Connect the Nano 33 BLE via USB then:
```bash
pio run -t upload
```
If the board fails to enter bootloader, quickly double-tap the reset button to force it, then retry upload.

## Serial Monitor (optional)
If you add serial prints:
```bash
pio device monitor --baud 115200
```
Add `monitor_speed = 115200` under the environment in `platformio.ini` if you use the monitor frequently.


## BLE Gesture Sequence & Results

The device advertises as `NanoGesture` exposing a custom service with:
- Write characteristic (20 bytes) for sequence definition
- ACK characteristic (notify, 1 byte) for parse/status/heartbeat
- DataOut characteristic (notify, variable length) for final predictions

### Sequence Definition Payload
Format (write characteristic): `[count][g0]...[g(count-1)][auxSeconds]`
- `count`: 1..16 (limited by 20-byte write, so practical max is <=17 including auxSeconds)
- `g(i)`: gesture index (0..kGestureCount-1) per `model_constants.h`
- `auxSeconds`: 1..10 additional delay gating capture between gestures

ACK codes (1 byte notification on ACK characteristic):
- `1..count` (echo of count) success
- `0xFC` parse too short
- `0xFD` count invalid
- `0xFE` length mismatch (expected `count+2`)
- `0xFA` gesture index out of range
- `0xF9` auxSeconds out of range
- `0xF8` busy (sequence already in progress)
- `0xF7` ready (entered stillness; begin hold before movement)
- `0xFF` heartbeat (periodic, not a parse result)

### Final Predictions Packet
Sent once after sequence completes on DataOut characteristic.
Format: `[count][p0][p1]...[p(count-1)]`
- `count`: number of gestures in sequence
- `p(i)`: predicted gesture index for step `i`
Length = `count+1` bytes.

### Usage Flow
1. Connect central (phone/host) to device
2. Write sequence definition payload
3. Device begins sampling and captures gestures only after sequence definition; progress via Serial (optional)
4. On completion final predictions packet notifies
5. Heartbeat bytes (`0xFF`) appear intermittently on ACK characteristic while connected

## Inference Runner Selection
This project supports an abstraction for gesture model inference so you can swap between local (on-device TensorFlow Lite Micro) and remote implementations without changing capture logic.

Interface: `IInferenceRunner` in `src/inference/inference_runner.h` with methods `bool Init()` and `int Predict(const float* window, int window_len, float* scores_out)`.

Implemented runners:
- `LocalInferenceRunner`: wraps TFLM `ModelRunner` for on-device inference (default).
- `RemoteInferenceRunner`: transports the 384-float window to a dedicated MCU via UART and receives scores/prediction back.

Selection:
- Define compile flag `USE_REMOTE_INFERENCE` to use remote inference.
  Example (PlatformIO): add under `[env:nano33ble]`:
  `build_flags = -DUSE_REMOTE_INFERENCE`

Usage in code:
- `main.cpp` chooses runner at startup based on the compile flag; if remote init fails it prints a fallback message and uses local inference.
