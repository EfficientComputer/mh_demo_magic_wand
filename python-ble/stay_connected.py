import asyncio
import time
from typing import List, Optional
from bleak import BleakClient, BleakScanner

WRITE_UUID = "12345678-1234-5678-1234-56789abcdef1"
ACK_UUID   = "12345678-1234-5678-1234-56789abcdef2"
CHUNK_SIZE = 20

async def _warm_cache(seconds: float = 2.0, adapter: Optional[str] = None):
    """Active scan briefly so BlueZ caches the device/address."""
    scanner = BleakScanner(adapter=adapter, scanning_mode="active")
    await scanner.start()
    await asyncio.sleep(seconds)
    await scanner.stop()

class BleBlinkConnection:
    def __init__(self, address: str, heartbeat_timeout: float = 12.0, adapter: Optional[str] = None):
        self.address = address
        self.adapter = adapter
        self.heartbeat_timeout = heartbeat_timeout
        self.client: Optional[BleakClient] = None

        self._ack_event = asyncio.Event()
        self._ack_len = 0
        self._last_liveness = 0.0

        self._runner: Optional[asyncio.Task] = None
        self._stop_flag = asyncio.Event()
        self._conn_lock = asyncio.Lock()  # serialize connect ops

    @property
    def connected(self) -> bool:
        return bool(self.client and self.client.is_connected)

    def _on_notify(self, _handle, data: bytearray):
        if not data:
            return
        b = int(data[0])
        # 0xFF = heartbeat; 0..20 = per-chunk ACK length
        if b == 0xFF:
            self._last_liveness = time.monotonic()
        else:
            self._ack_len = b
            self._ack_event.set()
            self._last_liveness = time.monotonic()

    async def _connect_once(self, timeout: float = 20.0):
        async with self._conn_lock:
            if self.connected:
                return
            # Warm up BlueZ’s cache (helps stability on some Pis)
            await _warm_cache(2.0, self.adapter)

            self.client = BleakClient(self.address, adapter=self.adapter, timeout=timeout)
            await self.client.connect()
            await self.client.start_notify(ACK_UUID, self._on_notify)
            self._last_liveness = time.monotonic()

    async def _disconnect_quiet(self):
        try:
            if self.client:
                try:
                    await self.client.stop_notify(ACK_UUID)
                except Exception:
                    pass
                await self.client.disconnect()
        finally:
            self.client = None

    async def wait_until_connected(self, timeout: float = 10.0) -> bool:
        """Block until connected (or timeout)."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.connected:
                return True
            try:
                await self._connect_once()
            except Exception:
                # brief backoff before retry
                await asyncio.sleep(0.5)
                continue
            await asyncio.sleep(0.1)
        return self.connected

    async def run_forever(self):
        """Keep the connection alive; reconnect if heartbeats stop."""
        backoff = 1.0
        while not self._stop_flag.is_set():
            try:
                if not self.connected:
                    await self._connect_once()
                    backoff = 1.0  # reset on success

                # If we miss heartbeats > timeout, reconnect
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
            self._stop_flag.set()
            await self._runner
            self._runner = None
            self._stop_flag.clear()

    async def send_with_ack(self, counts: List[int], per_chunk_timeout: float = 60.0):
        """Send any length (chunked at 20 bytes). Wait for ACK per chunk."""
        # Ensure we’re connected before sending
        ok = await self.wait_until_connected(timeout=10.0)
        if not ok:
            raise RuntimeError("Not connected")

        payload = bytes(max(0, min(255, c)) for c in counts)

        for i in range(0, len(payload), CHUNK_SIZE):
            chunk = payload[i:i+CHUNK_SIZE]
            self._ack_event.clear()
            await self.client.write_gatt_char(WRITE_UUID, chunk, response=False)

            try:
                await asyncio.wait_for(self._ack_event.wait(), timeout=per_chunk_timeout)
            except asyncio.TimeoutError:
                raise TimeoutError(f"ACK timeout after {per_chunk_timeout}s (chunk starting @ {i})")

            if self._ack_len != len(chunk):
                raise RuntimeError(f"ACK length mismatch: sent {len(chunk)} but got {self._ack_len}")

            # Any notification is liveness
            self._last_liveness = time.monotonic()

