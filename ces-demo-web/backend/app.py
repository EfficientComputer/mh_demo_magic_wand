from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Dict
import logging
import random
import asyncio
import uuid
import os
import time
from contextlib import asynccontextmanager
from dotenv import load_dotenv
from pathlib import Path
import sys
# Resolve repo root and add BLE client dir to sys.path
ROOT = Path(__file__).resolve().parents[2]  # /home/ec1/ces_demo
BLE_CLIENT_DIR = ROOT / "efficient-computer-python-ble"
if str(BLE_CLIENT_DIR) not in sys.path:
    sys.path.insert(0, str(BLE_CLIENT_DIR))
import main as ble_main
from enum import Enum

load_dotenv()


class SendRequest(BaseModel):
    game_id: str


class Gestures(Enum):
    SLASH = 0
    W = 1
    CIRCLE = 2
    UNKNOWN = 3


ARDUINO_MAC_ADDRESS = os.getenv("ARDUINO_MAC_ADDRESS", "9c:d9:e7:e4:6b:36")

FRONTEND_URL = os.getenv("FRONTEND_URL", "http://localhost:5173")
GESTURE_DELAY = int(os.getenv("GESTURE_DELAY_S", "3"))

# Initialize BLE connection
connection = ble_main.BleBlinkLink(ARDUINO_MAC_ADDRESS)
game_sessions: Dict[str, dict] = {}  # game_id -> {sent: [...], progress: [...], received: {...}|None, error: str|None, created_at: float}
SESSION_TIMEOUT_SECONDS = 300  # 5 minutes

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


async def _cleanup_old_sessions():
    """Background task to periodically remove abandoned game sessions."""
    while True:
        try:
            await asyncio.sleep(60)  # Check every minute
            current_time = time.time()
            expired_sessions = [
                game_id for game_id, session in game_sessions.items()
                if current_time - session.get("created_at", 0) > SESSION_TIMEOUT_SECONDS
            ]
            for game_id in expired_sessions:
                del game_sessions[game_id]
                logger.info(f"Cleaned up expired session: {game_id}")
        except Exception as e:
            logger.error(f"Error during session cleanup: {e}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    logger.info("Starting up CES Demo Web App API...")
    await connection.start()
    cleanup_task = asyncio.create_task(_cleanup_old_sessions())
    yield
    # Shutdown
    logger.info("Shutting down, disconnecting BLE device...")
    cleanup_task.cancel()
    if connection.connected:
        await connection.stop()


app = FastAPI(title="CES Demo Web App API", lifespan=lifespan)

# CORS middleware for React frontend
app.add_middleware(
    CORSMiddleware,
    allow_origins=[FRONTEND_URL],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/")
async def root():
    return {"message": "BLE Web App API", "status": "running"}


@app.get("/api/game/connection-status")
async def connection_status():
    return connection.connected


@app.post("/api/game/start")
async def start_game():
    """Check connection and create a new game session with unique ID."""
    if not connection.connected:
        raise HTTPException(status_code=503, detail="BLE device not connected")

    # Reset the Nano to ensure clean state before starting new game
    logger.info("Resetting nano state for new game")
    reset_ok = await connection.reset_nano(timeout=5.0)
    if not reset_ok:
        logger.error("Nano reset failed or timed out")
        raise HTTPException(status_code=500, detail="Failed to reset device state")

    game_id = str(uuid.uuid4())
    game_sessions[game_id] = {"sent": None, "progress": [], "received": None, "error": None, "created_at": time.time()}
    logger.info(f"New game started: {game_id}")
    return {"game_id": game_id}


@app.post("/api/game/ready")
async def send_to_arduino(request: SendRequest):
    """Send data to Arduino and start listening for response in background."""
    game_id = request.game_id

    if not connection.connected:
        raise HTTPException(status_code=503, detail="BLE device not connected")

    if game_id not in game_sessions:
        raise HTTPException(
            status_code=404, detail="Game ID not found. Call /api/game/start first."
        )

    # Generate random gestures (0-2) with fixed length 4
    length = 4
    gestures = [random.randint(0, 2) for _ in range(length)]
    game_sessions[game_id]["sent"] = gestures
    game_sessions[game_id]["progress"] = []  # reset progress for this game
    logger.info(f"Gestures sent to nano (game {game_id}): {gestures}")

    try:
        # Send to Arduino - format: (length, gesture_list, delay)
        await connection.send_with_ack((length, gestures, GESTURE_DELAY))

        # Start background task to listen for response.
        #
        # NOTE: This timeout starts when we first send the gesture
        # sequence, so it needs to cover the entire UI flow
        # (countdown + gesture + hold-still) for *all* gestures.
        # Our UI currently spends ~14s per gesture, so use a
        # generous per-gesture budget here.
        timeout = length * 60.0  # seconds
        asyncio.create_task(_listen_for_response(game_id, timeout=timeout))

        return {
            "sent": gestures,
            "delay": int(GESTURE_DELAY) + 2,
            "status": "listening",
            "game_id": game_id,
        }
    except Exception as e:
        logger.error(f"Send failed for game {game_id}: {e}")
        raise HTTPException(status_code=500, detail=f"Send error: {str(e)}")


@app.get("/api/game/results/{game_id}")
async def get_results(game_id: str):
    print(f"Fetching results for game ID: {game_id}, connected={connection.connected}")
    """Poll for Arduino response for a specific game. Returns waiting status if not ready yet."""
    if not connection.connected:
        raise HTTPException(status_code=503, detail="BLE device not connected")

    # Verify game exists
    if game_id not in game_sessions:
        raise HTTPException(status_code=404, detail="Game ID not found")

    session = game_sessions[game_id]

    # Check if there was an error
    if session["error"]:
        return {
            "status": "error",
            "game_id": game_id,
            "sent": session["sent"],
            "received": None,
            "error": session["error"],
        }

    # Check if response has been received
    if session["received"] is None:
        # Provide progress updates if any gestures completed
        status = "waiting"
        if session.get("progress"):
            status = "in-progress"
        return {
            "status": status,
            "game_id": game_id,
            "sent": session["sent"],
            "progress": session.get("progress", []),
            "received": None,
            "next_index": len(session.get("progress", [])),
        }

    # Response is ready - return it and clean up the session
    result = {
        "status": "complete",
        "game_id": game_id,
        "sent": session["sent"],
        "progress": session.get("progress", []),
        "received": session["received"],
    }
    logger.info(f"Sending game results for {game_id}: {result}")
    
    # Delete the session after returning complete results
    del game_sessions[game_id]
    
    return result


@app.get("/api/game/wait-ready/{game_id}")
async def wait_ready(game_id: str):
    """Block until the Nano reports stillness/ready via 0xF7 ACK.

    Returns {"status": "ready"} on success or {"status": "timeout"}
    if the ready signal is not observed within the timeout window.
    """
    if not connection.connected:
        raise HTTPException(status_code=503, detail="BLE device not connected")

    if game_id not in game_sessions:
        raise HTTPException(status_code=404, detail="Game ID not found")

    start = time.monotonic()
    logger.info(f"wait_ready: starting wait for game %s", game_id)
    ok = await connection.wait_for_ready(timeout=30.0)
    elapsed = time.monotonic() - start
    if ok:
        logger.info("wait_ready: ready for game %s after %.2fs", game_id, elapsed)
    else:
        logger.warning("wait_ready: TIMEOUT for game %s after %.2fs", game_id, elapsed)
    return {"game_id": game_id, "status": "ready" if ok else "timeout"}


@app.post("/api/game/go/{game_id}")
async def send_go(game_id: str):
    """Send a GO command to arm the next gesture capture.

    Returns {"status": "ok"} on success or 500 if the GO
    acknowledgement is not received within the timeout window.
    """
    if not connection.connected:
        raise HTTPException(status_code=503, detail="BLE device not connected")

    if game_id not in game_sessions:
        raise HTTPException(status_code=404, detail="Game ID not found")

    start = time.monotonic()
    logger.info("send_go: sending GO for game %s", game_id)
    ok = await connection.send_go()
    elapsed = time.monotonic() - start
    if not ok:
        logger.error("send_go: GO ACK failed or timed out for game %s after %.2fs", game_id, elapsed)
        raise HTTPException(status_code=500, detail="GO message failed")

    logger.info("send_go: GO acknowledged for game %s after %.2fs", game_id, elapsed)
    return {"game_id": game_id, "status": "ok"}
 
 
async def _listen_for_response(game_id: str, timeout: float):

    """Background task to receive Arduino response for a specific game.

    Matches the Nano's current BLE Data Out format, which sends only
    final predictions as:
        [count][pred0][pred1]...[pred(count-1)]
    """
    try:
        logger.info("_listen_for_response: waiting for final results for game %s with timeout %.1fs", game_id, timeout)
        incoming = await connection.recv_final_results(timeout=timeout)
        if game_id in game_sessions:
            if incoming:
                # incoming is (count, [pred0, pred1, ...])
                count, gestures = incoming
                # Power usage is not currently provided by the Nano; store
                # only predictions for now and use a placeholder power value.
                game_sessions[game_id]["received"] = {
                    "power_usage": 0.0,
                    "gestures": gestures,
                }
                logger.info(
                    f"Response received from nano (game {game_id}): count={count}, gestures={gestures}"
                )
            else:
                # Timeout occurred
                game_sessions[game_id]["error"] = "timeout"
                logger.warning(f"Timeout waiting for Arduino response (game {game_id})")
    except Exception as e:
        logger.error(f"Progress collection failed for game {game_id}: {e}")
        if game_id in game_sessions:
            game_sessions[game_id]["error"] = str(e)


async def _await_final_results(game_id: str, expected_len: int, timeout: float):
    """Background task to receive final game results with power usage.
    
    Called after all gesture progress has been collected.
    
    Protocol expectation:
    - Final results packet format:
      [count, gesture_id1, gesture_id2, ..., power_byte1, power_byte2, power_byte3, power_byte4]
    - If no final packet arrives, mark game complete with zero power usage (graceful degradation)
    """
    try:
        result = await connection.recv_final_results(timeout=timeout)
        
        if game_id not in game_sessions:
            return
        
        session = game_sessions[game_id]
        
        if result is None:
            # No final results packet received - this is an error condition
            logger.error(f"No final results packet received for game {game_id}")
            session["error"] = "timeout_waiting_for_final_results"
        else:
            count, gesture_ids, power_usage = result
            
            # Validate count matches expected
            if count != expected_len:
                logger.warning(f"Final results count mismatch for game {game_id}: expected {expected_len}, got {count}")
            
            # Use the gestures from final results (more authoritative than progress)
            session["received"] = {
                "power_usage": power_usage,
                "gestures": gesture_ids
            }
            # Update progress to match final results for consistency
            session["progress"] = gesture_ids
            logger.info(f"Final results stored for game {game_id}: power_usage={power_usage}, gestures={gesture_ids}")
    
    except Exception as e:
        logger.error(f"Final results reception failed for game {game_id}: {e}")
        if game_id in game_sessions:
            # Fallback: mark complete with what we have
            session = game_sessions[game_id]
            if session["received"] is None:
                session["received"] = {
                    "power_usage": 0.0,
                    "gestures": session["progress"]
                }
                logger.info(f"Using fallback completion for game {game_id} due to error: {e}")


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8000)
