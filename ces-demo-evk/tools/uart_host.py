#!/usr/bin/env python3

import sys
import time
from pathlib import Path

import serial

PORT = "/dev/ttyACM2"
BAUD = 115200

SENSOR_SAMPLES = 128
AXES = 3
FRAME_BYTES = SENSOR_SAMPLES * AXES  # 384
SYNC_BYTE = 0x80


def load_gesture_txt(path: Path) -> bytes:
    """Load gesture from a txt file into 384-byte sequence.

    Expects 128 lines with "x, y, z" int values. Ignores comments/blank lines.
    """

    values = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) != 3:
                continue
            try:
                x = int(parts[0])
                y = int(parts[1])
                z = int(parts[2])
            except ValueError:
                continue
            values.extend([x, y, z])

    if len(values) != FRAME_BYTES:
        raise ValueError(f"Expected {FRAME_BYTES} values, got {len(values)} from {path}")

    # Clamp to int8 range just in case (and avoid -128 per spec)
    clamped = []
    for v in values:
        if v < -128:
            v = -128
        if v > 127:
            v = 127
        if v == -128:
            v = -127
        clamped.append(v & 0xFF)

    return bytes(clamped)


def send_frame(ser: serial.Serial, payload: bytes) -> tuple[int, int]:
    """Send one gesture frame and read 2-byte response.

    Returns (gesture_id, confidence).
    """

    if len(payload) != FRAME_BYTES:
        raise ValueError(f"payload must be {FRAME_BYTES} bytes, got {len(payload)}")

    frame = bytes([SYNC_BYTE]) + payload
    ser.write(frame)
    ser.flush()

    # Read 2-byte response: [gesture_id][confidence]
    resp = ser.read(2)
    if len(resp) != 2:
        raise RuntimeError(f"Expected 2-byte response, got {len(resp)} bytes: {resp!r}")

    gesture_id = resp[0]
    confidence = resp[1]
    return gesture_id, confidence


def decode_gesture(gesture_id: int) -> str:
    gestures = {
        0: "Wing",
        1: "Ring",
        2: "Slope",
        3: "None",
    }
    return gestures.get(gesture_id, f"Unknown({gesture_id})")


MENU = """\nMagic Wand UART Host (Python virtual Nano)\n========================================\nPort: {port}\nBaud: {baud}\n\nCommands:\n  w - send WING gesture\n  r - send RING gesture\n  s - send SLOPE gesture\n  q - quit\n\nEnter choice: """


def main() -> int:
    base = Path(__file__).resolve().parent
    wing_path = base / "wing_gesture.txt"
    ring_path = base / "ring_gesture.txt"
    slope_path = base / "slope_gesture.txt"

    try:
        wing_payload = load_gesture_txt(wing_path)
        ring_payload = load_gesture_txt(ring_path)
        slope_payload = load_gesture_txt(slope_path)
    except Exception as e:
        print(f"Error loading gesture data: {e}", file=sys.stderr)
        return 1

    print(f"Opening serial port {PORT} @ {BAUD}...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=5)
    except Exception as e:
        print(f"Failed to open serial port: {e}", file=sys.stderr)
        return 1

    with ser:
        # Small delay to let the board reset if needed
        time.sleep(0.5)

        while True:
            choice = input(MENU.format(port=PORT, baud=BAUD)).strip().lower()

            if choice == "q":
                print("Exiting.")
                break
            elif choice == "w":
                payload = wing_payload
                label = "Wing"
            elif choice == "r":
                payload = ring_payload
                label = "Ring"
            elif choice == "s":
                payload = slope_payload
                label = "Slope (synthetic)"
            else:
                print("Unknown command. Use w, r, s, or q.")
                continue

            print(f"Sending {label} gesture frame...")
            try:
                gesture_id, confidence = send_frame(ser, payload)
            except Exception as e:
                print(f"Error during transaction: {e}")
                continue

            print(f"Response: gesture_id={gesture_id} ({decode_gesture(gesture_id)}), "
                  f"confidence={confidence}%")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
