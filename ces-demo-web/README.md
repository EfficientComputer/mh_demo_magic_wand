# My Web App

## Environment Variables
The mac address of the Nano should be set in the `/backend/.env` file before running the app. 

```ARDUINO_MAC_ADDRESS=XX:XX:XX:XX:XX:XX```

## Setup

This app references the `nano-ble` project, which lives in the `nano-ble/` directory at the root of this repository.


## Backend Setup
```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
uvicorn app:app --reload
```

## Frontend Setup
```bash
cd frontend
npm install
npm run dev
```


Or if the app is already setup, the full stack can be run with `./start.sh` in the root directory.

## Access
- Backend API: http://localhost:8000
- Frontend: http://localhost:5173
