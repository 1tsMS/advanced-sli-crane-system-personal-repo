import { useCallback, useState, useEffect } from "react";
import {
  ArrowLeft, ArrowRight,
  ChevronsUp, ChevronsDown, Gamepad2, Square
} from "lucide-react";

const API_BASE = "http://localhost:8000/api";

type AxisId = 1 | 2 | 3 | 4;

const AXES = [
  { id: 1 as AxisId, name: "Swing",     icon: "↔", units: "°" },
  { id: 2 as AxisId, name: "Boom Lift", icon: "↕", units: "°" },
  { id: 3 as AxisId, name: "Telescope", icon: "↔", units: "mm" },
  { id: 4 as AxisId, name: "Winch",     icon: "⇅", units: "mm" },
];

interface MotorControlsProps {
  speed: number;
  onSpeedChange: (s: number) => void;
}

async function apiPost(path: string, body: object) {
  try {
    const res = await fetch(`${API_BASE}${path}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    return res.ok;
  } catch {
    return false;
  }
}

export function MotorControls({ speed, onSpeedChange }: MotorControlsProps) {
  const [pressed, setPressed] = useState<Record<string, boolean>>({});
  const [gamepadConnected, setGamepadConnected] = useState(false);

  // Gamepad detection
  useEffect(() => {
    const onConnect    = () => setGamepadConnected(true);
    const onDisconnect = () => setGamepadConnected(false);
    window.addEventListener("gamepadconnected", onConnect);
    window.addEventListener("gamepaddisconnected", onDisconnect);
    return () => {
      window.removeEventListener("gamepadconnected", onConnect);
      window.removeEventListener("gamepaddisconnected", onDisconnect);
    };
  }, []);

  const startMotor = useCallback(
    async (axis: AxisId, direction: 0 | 1) => {
      const key = `${axis}-${direction}`;
      if (pressed[key]) return;
      setPressed(p => ({ ...p, [key]: true }));
      await apiPost("/command", { command: `M${axis} S${speed} D${direction}` });
    },
    [speed, pressed]
  );

  const stopAxis = useCallback(async (axis: AxisId) => {
    setPressed(p => {
      const next = { ...p };
      delete next[`${axis}-0`];
      delete next[`${axis}-1`];
      return next;
    });
    await apiPost("/command", { command: `M0 A${axis}` });
  }, []);

  const stopAll = useCallback(async () => {
    setPressed({});
    await apiPost("/estop", {});
  }, []);

  return (
    <div className="controls-panel scroll-y">
      <h3>Motor Controls</h3>

      {AXES.map(axis => (
        <div className="axis-control glass-card" key={axis.id} style={{ padding: "10px" }}>
          <div className="axis-label">
            <span style={{ color: "var(--cyan)", fontSize: "10px", fontFamily: "var(--font-mono)" }}>
              M{axis.id}
            </span>
            {axis.name}
          </div>
          <div className="axis-buttons">
            <button
              className={`motor-btn ${pressed[`${axis.id}-0`] ? "pressed" : ""}`}
              onMouseDown={() => startMotor(axis.id, 0)}
              onMouseUp={() => stopAxis(axis.id)}
              onMouseLeave={() => stopAxis(axis.id)}
              onTouchStart={() => startMotor(axis.id, 0)}
              onTouchEnd={() => stopAxis(axis.id)}
              title={`${axis.name} Direction 0`}
            >
              {axis.id === 4 ? <ChevronsUp size={16} /> : <ArrowLeft size={16} />}
            </button>
            <button
              className="motor-btn"
              style={{ flex: "0 0 32px", color: "var(--text-muted)" }}
              onClick={() => stopAxis(axis.id)}
              title="Stop axis"
            >
              <Square size={12} />
            </button>
            <button
              className={`motor-btn ${pressed[`${axis.id}-1`] ? "pressed" : ""}`}
              onMouseDown={() => startMotor(axis.id, 1)}
              onMouseUp={() => stopAxis(axis.id)}
              onMouseLeave={() => stopAxis(axis.id)}
              onTouchStart={() => startMotor(axis.id, 1)}
              onTouchEnd={() => stopAxis(axis.id)}
              title={`${axis.name} Direction 1`}
            >
              {axis.id === 4 ? <ChevronsDown size={16} /> : <ArrowRight size={16} />}
            </button>
          </div>
        </div>
      ))}

      {/* Speed slider */}
      <div className="speed-control glass-card" style={{ padding: "10px" }}>
        <div className="axis-label" style={{ marginBottom: 6 }}>
          Speed: <span style={{ fontFamily: "var(--font-mono)", color: "var(--cyan)" }}>{speed}</span>
          <span style={{ color: "var(--text-muted)", fontSize: 10 }}>/255</span>
        </div>
        <input
          type="range"
          min={20}
          max={255}
          step={5}
          value={speed}
          onChange={e => onSpeedChange(Number(e.target.value))}
          className="speed-slider"
          style={{ width: "100%" }}
        />
        <div style={{ display: "flex", justifyContent: "space-between", fontSize: 9, color: "var(--text-muted)", marginTop: 2 }}>
          <span>Slow</span>
          <span>Fast</span>
        </div>
      </div>

      {/* Stop All */}
      <button className="btn-danger" style={{ width: "100%" }} onClick={stopAll}>
        Stop All Axes
      </button>

      {/* Gamepad status */}
      <div className="xbox-status">
        <Gamepad2 size={14} style={{ color: gamepadConnected ? "var(--green)" : "var(--text-muted)" }} />
        <span>{gamepadConnected ? "Controller connected" : "No controller detected"}</span>
      </div>
    </div>
  );
}
