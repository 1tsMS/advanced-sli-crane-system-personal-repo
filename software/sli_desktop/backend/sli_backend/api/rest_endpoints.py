"""
REST API endpoints — all HTTP routes for the SLI backend.
"""
from __future__ import annotations
import logging
from pathlib import Path
from typing import Optional

from fastapi import APIRouter, HTTPException
from fastapi.responses import FileResponse

from ..api.models import (
    CommandRequest, ConnectionRequest, LoadChartUpload,
    StatusResponse, ApiResponse, ImuCalibrateRequest, TeleCalibrateRequest,
    RecordingState, EncoderCalibrateRequest, RopeCalibrateRequest,
    LoadCalibrateRequest, AxisScaleRequest,
)
from ..api.ws_endpoint import ws_manager

logger = logging.getLogger(__name__)

router = APIRouter(prefix="/api")

# These are set by main.py after all subsystems are initialized
_serial = None
_command_router = None
_session_logger = None


def init_endpoints(serial, command_router, session_logger) -> None:
    """Inject subsystem dependencies (called from main.py)."""
    global _serial, _command_router, _session_logger
    _serial = serial
    _command_router = command_router
    _session_logger = session_logger


# ------------------------------------------------------------------ #
# Connection management
# ------------------------------------------------------------------ #

@router.get("/status", response_model=StatusResponse)
async def get_status():
    """Backend health + serial connection state."""
    return StatusResponse(
        connected=_serial.is_connected if _serial else False,
        port=_serial.port if _serial else None,
        baudRate=_serial.baud_rate if _serial else None,
        rxRate=round(_serial.rx_rate, 1) if _serial else 0.0,
        backendOK=True,
        recording=_recording_state(),
    )


def _recording_state() -> RecordingState:
    return RecordingState(**_session_logger.state()) if _session_logger else RecordingState()


@router.get("/ports")
async def list_ports():
    """List available serial ports on the host machine."""
    if not _serial:
        return {"ports": []}
    return {"ports": _serial.list_ports()}


@router.post("/connect", response_model=ApiResponse)
async def connect(req: ConnectionRequest):
    """Open serial connection to ESP32."""
    if not _serial:
        raise HTTPException(500, "Serial manager not initialized")

    success = await _serial.connect(req.port, req.baudRate)
    if success:
        # Recording is NOT started here: it is off until the REC button is pressed
        return ApiResponse(ok=True, message=f"Connected to {req.port}")
    else:
        raise HTTPException(400, f"Failed to connect to {req.port}")


@router.post("/disconnect", response_model=ApiResponse)
async def disconnect():
    """Close serial connection."""
    if not _serial:
        return ApiResponse(ok=True, message="Not connected")

    if _session_logger and _session_logger.is_active:
        _session_logger.end_session()

    await _serial.disconnect()
    return ApiResponse(ok=True, message="Disconnected")


# ------------------------------------------------------------------ #
# Recording (CSV session log) — off by default
# ------------------------------------------------------------------ #

@router.get("/recording", response_model=RecordingState)
async def get_recording():
    return _recording_state()


@router.post("/recording/start", response_model=RecordingState)
async def start_recording():
    """Start writing telemetry to a new CSV (needs a live ESP32 link)."""
    if not _session_logger:
        raise HTTPException(500, "Session logger not initialized")
    if not (_serial and _serial.is_connected):
        raise HTTPException(409, "Connect to the ESP32 before recording")
    _session_logger.start_session()
    return _recording_state()


@router.post("/recording/stop", response_model=RecordingState)
async def stop_recording():
    """Close the current CSV (no-op if not recording)."""
    if _session_logger:
        _session_logger.end_session()
    return _recording_state()


# ------------------------------------------------------------------ #
# Command sending
# ------------------------------------------------------------------ #

@router.post("/command", response_model=ApiResponse)
async def send_command(req: CommandRequest):
    """Send a raw G-code command to ESP32."""
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")

    ok, msg = _command_router.send(req.command)
    if ok:
        return ApiResponse(ok=True, message=msg)
    else:
        raise HTTPException(400, msg)


@router.post("/estop", response_model=ApiResponse)
async def emergency_stop():
    """Immediate emergency stop — highest priority."""
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")

    ok, msg = _command_router.send_estop()
    return ApiResponse(ok=ok, message=msg)


# ------------------------------------------------------------------ #
# Calibration
# ------------------------------------------------------------------ #

@router.post("/calibrate/tare", response_model=ApiResponse)
async def calibrate_tare():
    """Send CAL0 (tare load cell) to ESP32."""
    ok, msg = _command_router.send("CAL0")
    return ApiResponse(ok=ok, message=msg)


@router.post("/calibrate/imu", response_model=ApiResponse)
async def calibrate_imu(req: Optional[ImuCalibrateRequest] = None):
    """Send CAL1 (zero IMU) with optional axis remapping to ESP32.

    Firmware CAL1 params:
      B<0|1>  — boom source axis: 0=Roll(X), 1=Pitch(Y)
      I<0|1>  — boom invert flag
      T<0|1>  — tilt source axis: 0=Roll(X), 1=Pitch(Y)  (opposite of boom)
      Q<0|1>  — tilt invert flag
    """
    if req:
        # Map axis string to index: "X"/"Roll" -> 0, "Y"/"Pitch" -> 1
        def axis_idx(s: Optional[str], default: int) -> int:
            return 1 if (s or "").upper() in ("Y", "PITCH") else (0 if (s or "").upper() in ("X", "ROLL") else default)

        b  = axis_idx(req.boomAxis, 0)          # boom source (default Roll=0)
        t  = axis_idx(req.tiltAxis, 1)          # tilt source (default Pitch=1)
        bi = 1 if req.boomInvert else 0
        ti = 1 if (req.tiltInvert or req.swingInvert) else 0
        cmd = f"CAL1 B{b} I{bi} T{t} Q{ti}"
    else:
        # Sensible defaults: boom=Roll(0) inverted, tilt=Pitch(1) not inverted
        cmd = "CAL1 B0 I1 T1 Q0"
    ok, msg = _command_router.send(cmd)
    return ApiResponse(ok=ok, message=msg)


@router.post("/calibrate/tele", response_model=ApiResponse)
async def calibrate_telescope(req: Optional[TeleCalibrateRequest] = None):
    """Send CAL2 to ESP32:
    - Plain CAL2: zeros current position to retracted (0mm)
    - CAL2 S<scale> I<invert>: tunes scale (mm/rev) and direction inversion in NVS flash
    """
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")

    if req and (req.scale is not None or req.invert is not None):
        parts = ["CAL2"]
        if req.scale is not None and req.scale > 0:
            parts.append(f"S{req.scale:.4f}")
        if req.invert is not None:
            parts.append(f"I{1 if req.invert else 0}")
        cmd = " ".join(parts)
    else:
        cmd = "CAL2"

    ok, msg = _command_router.send(cmd)
    return ApiResponse(ok=ok, message=msg)


_ENCODER_AXES = {"swing": 1, "boom": 2, "tele": 3, "rope": 4}


@router.post("/calibrate/encoder", response_model=ApiResponse)
async def calibrate_encoder(req: EncoderCalibrateRequest):
    """Calibrate one AS5600 axis on the ESP32 (stored in its flash).
    - invert -> CAL5 A<n> I<0|1>  (direction, mirrored around the zero)
    - value  -> CAL4 A<n> V<value> ("the axis is at <value> right now":
                degrees for swing/boom, mm for the telescope)
    Direction is sent first so the reference is set in the final direction.
    """
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")
    axis = _ENCODER_AXES.get(req.axis.lower())
    if axis is None:
        raise HTTPException(400, f"Unknown axis '{req.axis}' (use swing, boom, tele or rope)")
    if req.value is None and req.invert is None:
        raise HTTPException(400, "Give a value, an invert flag, or both")

    sent = []
    if req.invert is not None:
        cmd = f"CAL5 A{axis} I{1 if req.invert else 0}"
        ok, msg = _command_router.send(cmd)
        if not ok:
            raise HTTPException(400, msg)
        sent.append(cmd)
    if req.value is not None:
        cmd = f"CAL4 A{axis} V{req.value:.3f}"
        ok, msg = _command_router.send(cmd)
        if not ok:
            raise HTTPException(400, msg)
        sent.append(cmd)
    return ApiResponse(ok=True, message="Sent " + " + ".join(sent))


@router.post("/calibrate/load", response_model=ApiResponse)
async def calibrate_load(req: LoadCalibrateRequest):
    """Load cell multi-weight calibration (CAL7). The ESP32 averages ~2 s of
    readings per weight, refits counts-per-kg over all points and saves it;
    the result arrives as a $ACK,CAL7 line."""
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")
    action = req.action.lower()
    if action == "point":
        if req.weight is None or not (0 < req.weight <= 10):
            raise HTTPException(400, "Weight must be between 0 and 10 kg")
        cmd = f"CAL7 W{req.weight:.4f}"
    elif action == "clear":
        cmd = "CAL7 CLEAR"
    elif action == "scale":
        if req.scale is None or req.scale == 0:
            raise HTTPException(400, "Scale must be non-zero")
        cmd = f"CAL7 S{req.scale:.2f}"
    else:
        raise HTTPException(400, "action must be point, clear or scale")
    ok, msg = _command_router.send(cmd)
    if not ok:
        raise HTTPException(400, msg)
    return ApiResponse(ok=True, message=f"Sent {cmd}")


@router.post("/calibrate/axis-scale", response_model=ApiResponse)
async def calibrate_axis_scale(req: AxisScaleRequest):
    """Encoder ratio (CAL8): degrees (swing/boom) or mm (telescope) per encoder revolution."""
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")
    axis = {"swing": 1, "boom": 2, "tele": 3}.get(req.axis.lower())
    if axis is None:
        raise HTTPException(400, "axis must be swing, boom or tele")
    if not req.scale > 0:
        raise HTTPException(400, "Scale must be greater than 0")
    cmd = f"CAL8 A{axis} S{req.scale:.4f}"
    if req.invert is not None:
        cmd += f" I{1 if req.invert else 0}"
    ok, msg = _command_router.send(cmd)
    if not ok:
        raise HTTPException(400, msg)
    return ApiResponse(ok=True, message=f"Sent {cmd}")


@router.post("/calibrate/rope", response_model=ApiResponse)
async def calibrate_rope(req: RopeCalibrateRequest):
    """Rope (winch encoder) scale in mm per N20 count -> CAL6, and optionally direction -> CAL5 A4."""
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")
    if not (0 < req.scale < 10):
        raise HTTPException(400, "Scale must be between 0 and 10 mm per count")
    sent = []
    if req.invert is not None:
        cmd = f"CAL5 A4 I{1 if req.invert else 0}"
        ok, msg = _command_router.send(cmd)
        if not ok:
            raise HTTPException(400, msg)
        sent.append(cmd)
    cmd = f"CAL6 S{req.scale:.6f}"
    ok, msg = _command_router.send(cmd)
    if not ok:
        raise HTTPException(400, msg)
    sent.append(cmd)
    return ApiResponse(ok=True, message="Sent " + " + ".join(sent))


# ------------------------------------------------------------------ #
# Debug
# ------------------------------------------------------------------ #

@router.post("/debug/scan", response_model=ApiResponse)
async def request_debug_scan():
    """Request I2C scan + sensor status report from ESP32."""
    ok, msg = _command_router.send("DBG")
    return ApiResponse(ok=ok, message=msg)


# ------------------------------------------------------------------ #
# Load chart
# ------------------------------------------------------------------ #

@router.post("/loadchart/upload", response_model=ApiResponse)
async def upload_load_chart(chart: LoadChartUpload):
    """Upload a new load chart to ESP32 and store in NVS."""
    if not _command_router:
        raise HTTPException(500, "Command router not initialized")

    entries = chart.entries
    if not entries:
        raise HTTPException(400, "Empty load chart")

    # Send upload header
    _command_router.send(f"LC UPLOAD {len(entries)}")

    # Send each entry
    for entry in entries:
        cmd = f"LC {entry.angle},{entry.extensionMM},{entry.limitKg}"
        ok, msg = _command_router.send(cmd)
        if not ok:
            raise HTTPException(500, f"Failed to send entry: {msg}")

    # Confirm save
    ok, msg = _command_router.send("LC SAVE")
    if ok:
        return ApiResponse(
            ok=True,
            message=f"Load chart uploaded ({len(entries)} entries)"
        )
    else:
        raise HTTPException(500, f"LC SAVE failed: {msg}")


# ------------------------------------------------------------------ #
# Session logs
# ------------------------------------------------------------------ #

@router.get("/logs")
async def list_logs():
    """List all saved session CSV files."""
    if not _session_logger:
        return {"sessions": []}
    return {"sessions": _session_logger.list_sessions()}


@router.get("/logs/{filename}")
async def download_log(filename: str):
    """Download a specific session log CSV (only files inside the log folder)."""
    file_path = _session_logger.resolve_log(filename) if _session_logger else None
    if file_path is None:
        raise HTTPException(404, f"Log file '{filename}' not found")
    return FileResponse(str(file_path), media_type="text/csv", filename=file_path.name)
