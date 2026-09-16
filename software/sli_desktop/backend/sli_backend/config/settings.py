"""
Application-wide configuration and constants.
"""
from pathlib import Path

# Serial communication
DEFAULT_BAUD_RATE = 115200
SERIAL_TIMEOUT = 0.1  # seconds

# WebSocket
WS_HOST = "0.0.0.0"
WS_PORT = 8765

# REST API
API_HOST = "0.0.0.0"
API_PORT = 8000

# Telemetry
TELEMETRY_FIELDS = [
    "boomAngle", "extensionMM", "measuredLoad", "actualLoad",
    "swingAngle", "ropeLength",
    "fsr1", "fsr2", "fsr3", "fsr4",
    "imuRoll", "imuPitch",
    "safeLoadLimit", "loadPercent", "alarmLevel"
]

# Session logging
LOG_DIR = Path(__file__).parent.parent.parent / "logs"
LOG_DIR.mkdir(parents=True, exist_ok=True)

# CORS (allow Electron frontend)
CORS_ORIGINS = [
    "http://localhost:5173",   # Vite dev server
    "http://localhost:3000",
    "http://127.0.0.1:5173",
    "app://.",                  # Electron
]
