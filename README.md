# Magic Wand Demo

A gesture recognition demo using an Arduino Nano 33 BLE and Efficient Computer's EVK hardware. The system captures accelerometer-based gestures on the Nano, classifies them using TensorFlow Lite Micro (locally or via the EVK), and displays results through a web dashboard.

## Projects

### `nano-ble/`
Arduino Nano 33 BLE firmware (PlatformIO/C++). Captures IMU gestures, runs on-device inference with TFLite Micro, and communicates results over BLE. Supports local inference, remote inference via UART to the EVK, and a hybrid mode.

### `ces-demo-evk/`
EVK firmware (C/CMake) targeting Efficient Computer's SDK. Runs compiled MLIR models on the EVK hardware and communicates with the Nano over UART.

### `ces-demo-web/`
Web dashboard with a Python/FastAPI backend and React/Vite/Tailwind frontend. Connects to the Nano via BLE and displays gesture recognition results in real time.

### `python-ble/`
Python BLE utility client using the `bleak` library. Used for testing and development of the BLE connection to the Nano.
