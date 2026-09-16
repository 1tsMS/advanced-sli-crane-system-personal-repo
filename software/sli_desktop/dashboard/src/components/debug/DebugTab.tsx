import { useState, useEffect, useRef } from "react";
import { RefreshCw } from "lucide-react";

const API_BASE = "http://localhost:8000/api";

interface DebugEntry { bus: string; address: string | null; status: string; }
interface DebugReport { entries: DebugEntry[]; }

interface DebugTabProps {
  debugReport: DebugReport | null;
  lastAck: string | null;
}

// ---- Human-readable device names + GPIO info ----
// Map raw bus tag from firmware → { display name, address, GPIO note }
const DEVICE_INFO: Record<string, { name: string; gpio: string }> = {
  "I2C0_SWING":   { name: "AS5600 Swing Encoder",    gpio: "SDA=21 SCL=22" },
  "I2C1_BOOM":    { name: "AS5600 Boom Encoder",     gpio: "SDA=25 SCL=26" },
  "I2C2_TELE":    { name: "AS5600 Telescope Enc.",   gpio: "SDA=32 SCL=33" },
  "MPU6050":      { name: "MPU6050 (IMU)",           gpio: "SDA=21 SCL=22" },
  "HX711":        { name: "HX711 (Load Cell)",       gpio: "DT=17 SCK=16"  },
  "FSR1":         { name: "FSR Front-Left",          gpio: "ADC=34" },
  "FSR2":         { name: "FSR Front-Right",         gpio: "ADC=35" },
  "FSR3":         { name: "FSR Rear-Left",           gpio: "ADC=32" },
  "FSR4":         { name: "FSR Rear-Right",          gpio: "ADC=33" },
  "N20_WINCH":    { name: "N20 Winch Encoder",       gpio: "A=18 B=19" },
};

function friendlyName(bus: string): string {
  return DEVICE_INFO[bus]?.name ?? bus;
}

function gpioNote(bus: string): string {
  return DEVICE_INFO[bus]?.gpio ?? "—";
}

// Calibration options
type AxisChoice = "X" | "Y" | "Z";
const CAL_ITEMS = [
  { key: "tare",  label: "Tare Load Cell",         sub: "Zero with no load attached",           path: "/calibrate/tare", hasAxis: false },
  { key: "imu",   label: "Zero IMU / Gyro",         sub: "Place system still. Select which axis maps to each angle.", path: "/calibrate/imu",  hasAxis: true  },
  { key: "tele",  label: "Reset Telescope Pos.",   sub: "Set current position as zero (retracted)", path: "/calibrate/tele", hasAxis: false },
];

interface CalState { status: string; boomAxis: AxisChoice; swingAxis: AxisChoice; boomInv: boolean; swingInv: boolean; }

async function apiPost(path: string, body: object = {}) {
  try {
    const res = await fetch(`${API_BASE}${path}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const data = await res.json();
    return { ok: res.ok, msg: data.message ?? (res.ok ? "Done ✓" : "Failed") };
  } catch {
    return { ok: false, msg: "Backend offline" };
  }
}

export function DebugTab({ debugReport, lastAck }: DebugTabProps) {
  const [scanning, setScanning] = useState(false);
  const [logs, setLogs] = useState<{ text: string; type: "ack" | "info" | "err" }[]>([]);
  const [calState, setCalState] = useState<Record<string, CalState>>({});
  const termRef = useRef<HTMLDivElement>(null);

  // Collect ACKs into the log
  useEffect(() => {
    if (!lastAck) return;
    setLogs(prev => [{ text: lastAck, type: "ack" as const }, ...prev].slice(0, 200));
  }, [lastAck]);

  const scan = async () => {
    setScanning(true);
    addLog("→ DBG (I2C scan requested)", "info");
    await apiPost("/debug/scan");
    setTimeout(() => setScanning(false), 2500);
  };

  const runCal = async (item: typeof CAL_ITEMS[0]) => {
    const s = calState[item.key];
    const body = item.hasAxis ? {
      boomAxis: s?.boomAxis ?? "Y",
      swingAxis: s?.swingAxis ?? "X",
      boomInvert: s?.boomInv ?? false,
      swingInvert: s?.swingInv ?? false,
    } : {};

    addLog(`→ ${item.label} (${item.path})`, "info");
    const result = await apiPost(item.path, body);
    addLog(`← ${result.msg}`, result.ok ? "ack" : "err");
    setCalState(prev => ({
      ...prev,
      [item.key]: { ...prev[item.key] as CalState, status: result.msg },
    }));
  };

  const addLog = (text: string, type: "ack" | "info" | "err") => {
    const ts = new Date().toLocaleTimeString("en", { hour12: false, hour: "2-digit", minute: "2-digit", second: "2-digit" });
    setLogs(prev => [{ text: `[${ts}] ${text}`, type }, ...prev].slice(0, 200));
  };

  const setCalAxis = (key: string, field: string, val: string | boolean) => {
    setCalState(prev => ({
      ...prev,
      [key]: {
        boomAxis: (prev[key]?.boomAxis ?? "Y") as AxisChoice,
        swingAxis: (prev[key]?.swingAxis ?? "X") as AxisChoice,
        boomInv: prev[key]?.boomInv ?? false,
        swingInv: prev[key]?.swingInv ?? false,
        status: prev[key]?.status ?? "",
        [field]: val,
      },
    }));
  };

  return (
    <div className="debug-page">
      {/* Top two columns */}
      <div className="debug-main">

        {/* I2C / Sensor Scan */}
        <div className="debug-card">
          <div className="debug-card-header">
            <span className="debug-card-title">Hardware Status</span>
            <button
              className="btn-ghost"
              onClick={scan}
              disabled={scanning}
              style={{ display: "flex", alignItems: "center", gap: 5, padding: "3px 8px", fontSize: 10 }}
            >
              <RefreshCw size={10} style={{ animation: scanning ? "spin 1s linear infinite" : "none" }} />
              {scanning ? "Scanning..." : "Scan"}
            </button>
          </div>
          <div className="debug-card-body">
            {debugReport && debugReport.entries.length > 0 ? (
              <table className="scan-table">
                <thead>
                  <tr>
                    <th>Device</th>
                    <th>GPIO</th>
                    <th>Address</th>
                    <th>Status</th>
                  </tr>
                </thead>
                <tbody>
                  {debugReport.entries.map((e, i) => (
                    <tr key={i}>
                      <td>{friendlyName(e.bus)}</td>
                      <td className="dim-text">{gpioNote(e.bus)}</td>
                      <td className="dim-text">{e.address ?? "—"}</td>
                      <td className={e.status === "OK" ? "ok-text" : "fail-text"}>
                        {e.status}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            ) : (
              <div style={{ fontSize: 11, color: "var(--text-muted)", paddingTop: 4 }}>
                Click "Scan" to request hardware status from ESP32.
              </div>
            )}
          </div>
        </div>

        {/* Calibration */}
        <div className="debug-card">
          <div className="debug-card-header">
            <span className="debug-card-title">Calibration</span>
          </div>
          <div className="debug-card-body">
            <div className="cal-list">
              {CAL_ITEMS.map(item => (
                <div className="cal-item" key={item.key}>
                  <div className="cal-item-top">
                    <div>
                      <div className="cal-item-name">{item.label}</div>
                      <div className="cal-item-sub">{item.sub}</div>
                    </div>
                    <button
                      className="btn-primary"
                      style={{ padding: "4px 10px", fontSize: 10 }}
                      onClick={() => runCal(item)}
                    >
                      Run
                    </button>
                  </div>

                  {/* IMU axis mapping UI */}
                  {item.hasAxis && (
                    <div style={{ marginTop: 6, display: "flex", flexDirection: "column", gap: 5 }}>
                      <div style={{ fontSize: 9, color: "var(--text-muted)", marginBottom: 2 }}>
                        Axis Mapping (select which physical axis is used for each angle):
                      </div>
                      <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
                        {/* Boom angle axis */}
                        <div>
                          <div style={{ fontSize: 9, color: "var(--text-secondary)", marginBottom: 3 }}>Boom Lift</div>
                          <div className="cal-axis-row">
                            {(["X", "Y", "Z"] as AxisChoice[]).map(a => (
                              <button
                                key={a}
                                className={`axis-badge ${(calState[item.key]?.boomAxis ?? "Y") === a ? "sel" : ""}`}
                                onClick={() => setCalAxis(item.key, "boomAxis", a)}
                              >
                                {a}
                              </button>
                            ))}
                            <button
                              className={`axis-badge ${(calState[item.key]?.boomInv ?? false) ? "sel" : ""}`}
                              onClick={() => setCalAxis(item.key, "boomInv", !(calState[item.key]?.boomInv ?? false))}
                              style={{ marginLeft: 4 }}
                            >
                              INV
                            </button>
                          </div>
                        </div>
                        {/* Swing angle axis */}
                        <div>
                          <div style={{ fontSize: 9, color: "var(--text-secondary)", marginBottom: 3 }}>Swing</div>
                          <div className="cal-axis-row">
                            {(["X", "Y", "Z"] as AxisChoice[]).map(a => (
                              <button
                                key={a}
                                className={`axis-badge ${(calState[item.key]?.swingAxis ?? "X") === a ? "sel" : ""}`}
                                onClick={() => setCalAxis(item.key, "swingAxis", a)}
                              >
                                {a}
                              </button>
                            ))}
                            <button
                              className={`axis-badge ${(calState[item.key]?.swingInv ?? false) ? "sel" : ""}`}
                              onClick={() => setCalAxis(item.key, "swingInv", !(calState[item.key]?.swingInv ?? false))}
                              style={{ marginLeft: 4 }}
                            >
                              INV
                            </button>
                          </div>
                        </div>
                      </div>
                    </div>
                  )}

                  {calState[item.key]?.status && (
                    <div className="cal-item-status" style={{ marginTop: 4 }}>
                      ← {calState[item.key].status}
                    </div>
                  )}
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* Terminal log bar at bottom */}
      <div className="terminal-bar">
        <div className="terminal-header">
          <div className="terminal-dot" />
          ACK / Command Log
        </div>
        <div className="terminal-body" ref={termRef}>
          {logs.length === 0 && (
            <div className="terminal-line dim-text">Waiting for commands...</div>
          )}
          {logs.map((l, i) => (
            <div key={i} className={`terminal-line ${l.type}`}>
              {l.text}
            </div>
          ))}
        </div>
      </div>

      <style>{`@keyframes spin { from{transform:rotate(0deg)} to{transform:rotate(360deg)} }`}</style>
    </div>
  );
}
