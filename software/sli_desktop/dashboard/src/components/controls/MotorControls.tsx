import { useCallback, useState, useEffect } from "react";
import { Square, Gamepad2 } from "lucide-react";

const API_BASE = "http://localhost:8000/api";

type AxisId = 1 | 2 | 3 | 4;

const AXES: { id: AxisId; name: string; negLabel: string; posLabel: string }[] = [
  { id: 1, name: "Swing",    negLabel: "◄", posLabel: "►" },
  { id: 2, name: "Boom",     negLabel: "▼", posLabel: "▲" },
  { id: 3, name: "Extend",   negLabel: "◄", posLabel: "►" },
  { id: 4, name: "Winch",    negLabel: "▼", posLabel: "▲" },
];

async function apiPost(path: string, body: object) {
  try {
    const res = await fetch(`${API_BASE}${path}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    return res.ok;
  } catch { return false; }
}

interface MotorControlsProps {
  speed: number;
  onSpeedChange: (s: number) => void;
}

export function MotorControls({ speed, onSpeedChange }: MotorControlsProps) {
  const [pressed, setPressed] = useState<Record<string, boolean>>({});
  const [gpConnected, setGpConnected] = useState(false);

  useEffect(() => {
    const on  = () => setGpConnected(true);
    const off = () => setGpConnected(false);
    window.addEventListener("gamepadconnected", on);
    window.addEventListener("gamepaddisconnected", off);
    return () => {
      window.removeEventListener("gamepadconnected", on);
      window.removeEventListener("gamepaddisconnected", off);
    };
  }, []);

  const startMotor = useCallback(async (axis: AxisId, dir: 0 | 1) => {
    const key = `${axis}-${dir}`;
    if (pressed[key]) return;
    setPressed(p => ({ ...p, [key]: true }));
    await apiPost("/command", { command: `M${axis} S${speed} D${dir}` });
  }, [speed, pressed]);

  const stopAxis = useCallback(async (axis: AxisId) => {
    setPressed(p => {
      const n = { ...p };
      delete n[`${axis}-0`]; delete n[`${axis}-1`];
      return n;
    });
    await apiPost("/command", { command: `M0 A${axis}` });
  }, []);

  const stopAll = useCallback(async () => {
    setPressed({});
    await apiPost("/estop", {});
  }, []);

  return (
    <div className="panel" style={{ height: "100%" }}>
      <div className="panel-header">
        <span className="ph-icon">⊕</span>
        Motor Controls
      </div>

      {/* 2×2 axis grid */}
      <div className="axes-grid">
        {AXES.map(ax => (
          <div className="axis-card" key={ax.id}>
            <div className="axis-name">
              {ax.name}
              <span className="axis-id">M{ax.id}</span>
            </div>
            <div className="axis-btns">
              <button
                className={`mtr-btn ${pressed[`${ax.id}-0`] ? "pressed" : ""}`}
                onMouseDown={() => startMotor(ax.id, 0)}
                onMouseUp={() => stopAxis(ax.id)}
                onMouseLeave={() => { if (pressed[`${ax.id}-0`]) stopAxis(ax.id); }}
                onTouchStart={e => { e.preventDefault(); startMotor(ax.id, 0); }}
                onTouchEnd={() => stopAxis(ax.id)}
              >
                {ax.negLabel}
              </button>
              <button
                className="mtr-btn stop"
                title="Stop"
                onClick={() => stopAxis(ax.id)}
              >
                <Square size={9} />
              </button>
              <button
                className={`mtr-btn ${pressed[`${ax.id}-1`] ? "pressed" : ""}`}
                onMouseDown={() => startMotor(ax.id, 1)}
                onMouseUp={() => stopAxis(ax.id)}
                onMouseLeave={() => { if (pressed[`${ax.id}-1`]) stopAxis(ax.id); }}
                onTouchStart={e => { e.preventDefault(); startMotor(ax.id, 1); }}
                onTouchEnd={() => stopAxis(ax.id)}
              >
                {ax.posLabel}
              </button>
            </div>
          </div>
        ))}
      </div>

      {/* Footer: speed + stop all + gamepad */}
      <div className="controls-footer" style={{ flexWrap: "wrap", gap: 6 }}>
        <span className="speed-label">
          Speed: <span className="speed-val">{speed}</span>
        </span>
        <input
          type="range" min={250} max={2500} step={10}
          value={speed}
          onChange={e => onSpeedChange(Number(e.target.value))}
          className="speed-slider"
        />
        <button className="stop-all-btn" onClick={stopAll}>
          ■ Stop All
        </button>
      </div>

      {/* Gamepad indicator */}
      <div style={{ padding: "0 8px 8px" }}>
        <div className={`gp-badge ${gpConnected ? "live" : ""}`}>
          <Gamepad2 size={12} />
          {gpConnected ? "Controller active" : "No controller"}
        </div>
      </div>
    </div>
  );
}
