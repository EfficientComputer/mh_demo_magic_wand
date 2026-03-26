# main.py
import asyncio
import time
import random
import argparse
from typing import List, Optional
from bleak import BleakClient, BleakScanner

WRITE_UUID = "12345678-1234-5678-1234-56789abcdef1"  # Pi -> Arduino
ACK_UUID   = "12345678-1234-5678-1234-56789abcdef2"  # Arduino -> Pi (notify: ACK + heartbeat)
DATA_UUID  = "12345678-1234-5678-1234-56789abcdef3"  # Arduino -> Pi (notify: 5 bytes)
CHUNK_SIZE = 20

async def _warm_cache(seconds: float = 2.0, adapter: Optional[str] = None):
    """Active scan briefly so BlueZ caches the device/address (Linux stability)."""
    scanner = BleakScanner(adapter=adapter, scanning_mode="active")
    await scanner.start()
    await asyncio.sleep(seconds)
    await scanner.stop()

class BleBlinkLink:
    def __init__(self, address: str, heartbeat_timeout: float = 12.0, adapter: Optional[str] = None):
        print(f"Initializing BleBlinkLink with address {address}, adapter={adapter}")
        self.address = address
        self.adapter = adapter
        self.heartbeat_timeout = heartbeat_timeout

        self.client: Optional[BleakClient] = None
        self._runner: Optional[asyncio.Task] = None
        self._stop = asyncio.Event()

        self._last_liveness = 0.0

        # ACK plumbing
        self._ack_event = asyncio.Event()
        # For sequence payloads, this stores the ACK "length" value.
        # For control commands (e.g. GO), this stores the ACK code.
        self._ack_len = 0

        # Ready/stillness ACK event (0xF7)
        self._ready_event = asyncio.Event()

        # Reset ACK event (0xFB)
        self._reset_event = asyncio.Event()

        # Ensure only one waiter for stillness/ready at a time
        self._ready_lock = asyncio.Lock()

        # DATA_OUT plumbing
        self._data_event = asyncio.Event()
        self._data_bytes: Optional[bytes] = None

        self._conn_lock = asyncio.Lock()

    @property
    def connected(self) -> bool:
        return bool(self.client and self.client.is_connected)

    # ---- Notifications ----
    def _on_ack(self, _handle, data: bytearray):
        if not data:
            return
        b = int(data[0])
        # 0xFF = heartbeat; 0xF7 = ready/stillness marker; 0xFB = reset ACK;
        # all other values are treated as ACK bytes for writes on the WRITE
        # characteristic.
        if b == 0xFF:
            self._last_liveness = time.monotonic()
        elif b == 0xF7:
            # Signal that the Nano has entered stillness/ready state.
            print("[BleBlinkLink] Ready/stillness ACK (0xF7) received")
            self._ready_event.set()
            self._last_liveness = time.monotonic()
        elif b == 0xFB:
            # Signal that the Nano has acknowledged reset.
            print("[BleBlinkLink] Reset ACK (0xFB) received")
            self._reset_event.set()
            self._last_liveness = time.monotonic()
        else:
            self._ack_len = b
            self._ack_event.set()
            self._last_liveness = time.monotonic()

    def _on_data(self, _handle, data: bytearray):
        # Receive variable-length data from Arduino
        if data and len(data) > 0:
            self._data_bytes = bytes(data)
            self._data_event.set()
            self._last_liveness = time.monotonic()
            print(f"[BleBlinkLink] DATA notification received: {self._data_bytes}")

    # ---- Connection mgmt ----
    async def _connect_once(self, timeout: float = 20.0):
        async with self._conn_lock:
            if self.connected:
                return
            await _warm_cache(2.0, self.adapter)
            self.client = BleakClient(self.address, adapter=self.adapter, timeout=timeout)
            await self.client.connect()
            await self.client.start_notify(ACK_UUID, self._on_ack)
            await self.client.start_notify(DATA_UUID, self._on_data)
            self._last_liveness = time.monotonic()

    async def _disconnect_quiet(self):
        try:
            if self.client:
                try:
                    await self.client.stop_notify(ACK_UUID)
                except Exception:
                    pass
                try:
                    await self.client.stop_notify(DATA_UUID)
                except Exception:
                    pass
                await self.client.disconnect()
        finally:
            self.client = None

    async def wait_until_connected(self, timeout: float = 12.0) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.connected:
                return True
            try:
                await self._connect_once()
            except Exception:
                await asyncio.sleep(0.5)
                continue
            await asyncio.sleep(0.1)
        return self.connected

    async def run_forever(self):
        backoff = 1.0
        while not self._stop.is_set():
            try:
                if not self.connected:
                    await self._connect_once()
                    backoff = 1.0

                if time.monotonic() - self._last_liveness > self.heartbeat_timeout:
                    await self._disconnect_quiet()
                    await asyncio.sleep(backoff)
                    backoff = min(backoff * 2, 10.0)
                    continue

                await asyncio.sleep(0.5)
            except Exception:
                await self._disconnect_quiet()
                await asyncio.sleep(backoff)
                backoff = min(backoff * 2, 10.0)
        await self._disconnect_quiet()

    async def start(self):
        if not self._runner:
            self._runner = asyncio.create_task(self.run_forever())

    async def stop(self):
        if self._runner:
            self._stop.set()
            await self._runner
            self._runner = None
            self._stop.clear()

    # ---- IO helpers ----
    async def send_with_ack(self, counts: tuple[int, List[int], int], per_chunk_timeout: float = 60.0):
        if not await self.wait_until_connected(timeout=12.0):
            raise RuntimeError("Not connected")

        # Unpack: first element is length, second is the list of gesture values
        length, gesture_list, delay = counts
        
        # Build payload: [length byte] + [gesture bytes] + [delay byte]
        payload = bytes([length] + gesture_list + [delay])
        
        for i in range(0, len(payload), CHUNK_SIZE):
            chunk = payload[i:i+CHUNK_SIZE]
            self._ack_event.clear()
            await self.client.write_gatt_char(WRITE_UUID, chunk, response=False)
            try:
                await asyncio.wait_for(self._ack_event.wait(), timeout=per_chunk_timeout)
            except asyncio.TimeoutError:
                raise TimeoutError(f"ACK timeout (chunk starting at {i})")
            # For sequence payload chunks, the Arduino returns the number of
            # bytes successfully processed as an ACK. This is defined as
            # (len(chunk) - 2) in the current firmware.
            if self._ack_len != len(chunk) - 2:
                raise RuntimeError(f"ACK mismatch: sent {len(chunk)}, got {self._ack_len}")
            self._last_liveness = time.monotonic()

    async def send_go(self) -> bool:
        """Send a single-byte GO command and wait for ACK_GO_OK (0xF6).

        Returns True on success, False on timeout or unexpected ACK.
        """
        if not await self.wait_until_connected(timeout=12.0):
            raise RuntimeError("Not connected")

        # Clear any previous ACK state and send the GO command byte (0x01).
        self._ack_event.clear()
        self._ack_len = 0
        try:
            await self.client.write_gatt_char(WRITE_UUID, bytes([0x01]), response=False)
        except Exception as e:
            print(f"send_go: write_gatt_char failed: {e}")
            return False
        
        return True


    async def recv_final_results(self, timeout: float = 15.0) -> Optional[tuple[int, List[int]]]:
        """Wait for final game results notification from Arduino.
        
        Current Nano firmware sends final predictions as:
            Byte 0: count (number of gestures in the game)
            Bytes 1..count: gesture IDs for each gesture (indices 0-3)
        
        Note: Power usage is not currently encoded on the wire. This helper
        returns only (count, [gesture_ids...]) or None on timeout.
        """

        def _parse_bytes(data_bytes: bytes) -> Optional[tuple[int, List[int]]]:
            if not data_bytes or len(data_bytes) < 1:
                return None
            print(f"Final results received: {data_bytes}")
            data = list(data_bytes)
            count = data[0]
            # Expect at least [count][pred0]..[pred(count-1)]
            if len(data) < count + 1:
                print(f"Warning: Expected at least {count + 1} bytes, got {len(data)}")
                return None
            gesture_ids = data[1:count + 1]
            # Power usage parsing intentionally omitted for now to match
            # the Nano's current Data Out format.
            print(f"Parsed final results: count={count}, gestures={gesture_ids}")
            return (count, gesture_ids)

        # Fast path: if a DATA notification already arrived before we were
        # called, consume and parse it instead of clearing it.
        if self._data_bytes:
            result = _parse_bytes(self._data_bytes)
            # Clear stored bytes/event so they are not reused on the next call.
            self._data_bytes = None
            self._data_event.clear()
            return result

        # Otherwise, wait for the next DATA notification.
        self._data_event.clear()
        try:
            await asyncio.wait_for(self._data_event.wait(), timeout=timeout)
        except asyncio.TimeoutError:
            return None
        if not self._data_bytes:
            return None

        result = _parse_bytes(self._data_bytes)
        # Clear stored bytes/event so they are not reused on the next call.
        self._data_bytes = None
        self._data_event.clear()
        return result

    async def wait_for_ready(self, timeout: float = 30.0) -> bool:
        """Block until the Nano reports stillness/ready via a 0xF7 ACK.

        Returns True if the signal is observed before the timeout,
        otherwise False.
        """
        async with self._ready_lock:
            self._ready_event.clear()
            start = time.monotonic()
            print(f"[BleBlinkLink] wait_for_ready: waiting up to {timeout}s for stillness/ready")
            try:
                await asyncio.wait_for(self._ready_event.wait(), timeout=timeout)
                elapsed = time.monotonic() - start
                print(f"[BleBlinkLink] wait_for_ready: ready after {elapsed:.2f}s")
                return True
            except asyncio.TimeoutError:
                elapsed = time.monotonic() - start
                print(f"[BleBlinkLink] wait_for_ready: TIMEOUT after {elapsed:.2f}s")
                return False

    async def reset_nano(self, timeout: float = 5.0) -> bool:
        """Send reset command (0x00) to the Nano and wait for ACK (0xFB).
        
        Returns True if reset ACK is received before timeout, False otherwise.
        """
        if not await self.wait_until_connected(timeout=12.0):
            raise RuntimeError("Not connected")
        
        self._reset_event.clear()
        start = time.monotonic()
        print(f"[BleBlinkLink] reset_nano: sending reset command (0x00)")
        
        try:
            await self.client.write_gatt_char(WRITE_UUID, bytes([0x00]), response=False)
        except Exception as e:
            print(f"reset_nano: write_gatt_char failed: {e}")
            return False
        
        try:
            await asyncio.wait_for(self._reset_event.wait(), timeout=timeout)
            elapsed = time.monotonic() - start
            print(f"[BleBlinkLink] reset_nano: ACK received after {elapsed:.2f}s")
            return True
        except asyncio.TimeoutError:
            elapsed = time.monotonic() - start
            print(f"[BleBlinkLink] reset_nano: TIMEOUT after {elapsed:.2f}s")
            return False

 
 # ---------- Periodic main loop ----------

async def periodic_roundtrip(address: str, interval_s: int):
    link = BleBlinkLink(address, heartbeat_timeout=12.0)
    await link.start()

    print("Waiting for connection…")
    if not await link.wait_until_connected(timeout=15.0):
        raise SystemExit("Could not establish BLE connection.")

    print("Connected. Round-trip every", interval_s, "seconds.\n")

    try:
        while True:
            # 1) Generate random 5 numbers in [1..5]
            outgoing = [random.randint(1, 5) for _ in range(5)]
            print("Pi → Arduino:", outgoing)

            # 2) Send to Arduino (with per-chunk ACK)
            try:
                await link.send_with_ack(outgoing)
            except Exception as e:
                print("Send failed:", e)
                await asyncio.sleep(interval_s)
                continue

            # 3) Arduino blinks (done already during send)
            # 4) Receive Arduino's random 5 numbers
            incoming = await link.recv_random5(timeout=30.0)
            if incoming is None:
                print("Pi ← Arduino: (timeout waiting for DATA_OUT)")
            else:
                # 5) Print the random numbers
                print("Pi ← Arduino:", incoming)

            # Wait N seconds
            await asyncio.sleep(interval_s)
    finally:
        await link.stop()


def main():
    parser = argparse.ArgumentParser(description="Pi↔Arduino BLE round-trip every N seconds")
    parser.add_argument("--address", "-a", required=True, help="Arduino MAC (e.g., 0F:D7:84:E3:DE:B6)")
    parser.add_argument("--interval", "-i", type=int, default=10, help="Loop interval seconds (default 10)")
    args = parser.parse_args()

    asyncio.run(periodic_roundtrip(args.address, args.interval))

if __name__ == "__main__":
    main()

