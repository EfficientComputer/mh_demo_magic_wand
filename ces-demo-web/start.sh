#!/bin/bash

# Start the CES Demo Web App - both frontend and backend

echo "Starting CES Demo Web App..."

usage() {
    echo "Usage: $0 (--wand wand1|wand2 | --mac AA:BB:CC:DD:EE:FF)" >&2
    echo "  --wand wand1|wand2   Use predefined MAC address for wand1 or wand2" >&2
    echo "  --mac  <address>     Use a custom Arduino MAC address" >&2
    exit 1
}

WAND_NAME=""
CUSTOM_MAC=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wand)
            shift
            WAND_NAME="$1"
            ;;
        --mac)
            shift
            CUSTOM_MAC="$1"
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            ;;
    esac
    shift
done

if [[ -n "$WAND_NAME" && -n "$CUSTOM_MAC" ]]; then
    echo "Error: specify either --wand or --mac, not both" >&2
    usage
fi

if [[ -z "$WAND_NAME" && -z "$CUSTOM_MAC" ]]; then
    echo "Error: you must specify either --wand or --mac" >&2
    usage
fi

case "$WAND_NAME" in
    wand1)
        ARDUINO_MAC_ADDRESS="0f:d7:84:e3:de:b6"
        echo "Using wand1 (ARDUINO_MAC_ADDRESS=$ARDUINO_MAC_ADDRESS)"
        ;;
    wand2)
        ARDUINO_MAC_ADDRESS="ae:5e:38:fe:1c:da"
        echo "Using wand2 (ARDUINO_MAC_ADDRESS=$ARDUINO_MAC_ADDRESS)"
        ;;
    "")
        ;;
    *)
        echo "Error: --wand must be 'wand1' or 'wand2' (got '$WAND_NAME')" >&2
        usage
        ;;
esac

if [[ -n "$CUSTOM_MAC" ]]; then
    ARDUINO_MAC_ADDRESS="$CUSTOM_MAC"
    echo "Using custom MAC (ARDUINO_MAC_ADDRESS=$ARDUINO_MAC_ADDRESS)"
fi

export ARDUINO_MAC_ADDRESS

# Install frontend dependencies
echo "Installing frontend dependencies..."
cd frontend
npm install
echo "✓ Frontend dependencies installed"
echo ""

# Start frontend in background (suppress output)
echo "Starting frontend..."
npm run dev > /dev/null 2>&1 &
FRONTEND_PID=$!
cd ..

# Start backend in foreground (show logs)
echo "Starting backend..."
echo ""

# Setup backend environment
cd backend
if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
    echo "✓ Virtual environment created"
fi

echo "Installing backend dependencies..."
venv/bin/python -m pip install --upgrade pip
venv/bin/python -m pip install -r requirements.txt || {
    echo "Error: Failed to install backend dependencies"
    kill $FRONTEND_PID 2>/dev/null
    exit 1
}
echo "✓ Backend dependencies installed"
echo ""

echo "✓ Frontend started (PID: $FRONTEND_PID) - http://localhost:5173"
echo "✓ Backend starting - http://localhost:8000"
echo ""
echo "Showing backend logs (Press Ctrl+C to stop both servers):"
echo "================================================================"

# Function to cleanup on exit
cleanup() {
    echo ""
    echo "Stopping servers..."
    kill $FRONTEND_PID 2>/dev/null
    echo "✓ Servers stopped"
}

# Trap Ctrl+C and cleanup
trap cleanup INT TERM

# Run backend in foreground using venv's python directly
venv/bin/python -m uvicorn app:app --reload --log-level debug
cd ..

