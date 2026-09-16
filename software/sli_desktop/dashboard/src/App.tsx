import { useState } from "react";
import {
  LayoutDashboard, Terminal, SlidersHorizontal,
  Database, Wrench, Settings, Zap, Activity
} from "lucide-react";
import type { TabId } from "./types";
import { useTelemetry } from "./hooks/useTelemetry";
import { TelemetryPanel } from "./components/telemetry/TelemetryPanel";
import { MotorControls } from "./components/controls/MotorControls";
import { CraneVisualizer } from "./components/visualizer/CraneVisualizer";
import { Gauge } from "./components/shared/Gauge";
import { SettingsTab } from "./components/settings/SettingsTab";
import { DebugTab } from "./components/debug/DebugTab";

const API_BASE = "http://localhost:8000/api";

const NAV_ITEMS: { id: TabId; label: string; icon: React.ReactNode }[] = [
  { id: "dashboard",   label: "Dashboard",    icon: <LayoutDashboard size={15} /> },
  { id: "debug",       label: "Debug",        icon: <Terminal size={15} /> },
  { id: "calibration", label: "Calibration",  icon: <SlidersHorizontal size={15} /> },
  { id: "datalogger",  label: "Data Logger",  icon: <Database size={15} /> },
  { id: "loadchart",   label: "Load Chart",   icon: <Activity size={15} /> },
  { id: "controller",  label: "Controller",   icon: <Wrench size={15} /> },
  { id: "settings",    label: "Settings",     icon: <Settings size={15} /> },
];

const ALARM_LABELS = ["SYSTEM OK", "WARNING", "CRITICAL", "E-STOP"];
const ALARM_CLASSES = ["ok", "warn", "critical", "estop"];

async function apiPost(path: string, body: object = {}) {
  try {
    await fetch(`${API_BASE}${path}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
  } catch { /* offline */ }
}

export default function App() {
  const [activeTab, setActiveTab] = useState<TabId>("dashboard");
  const [speed, setSpeed] = useState(150);
  const { frame, debugReport, lastAck, wsStatus } = useTelemetry();

  const alarmLvl = frame?.alarmLevel ?? 0;
  const alarmClass = ALARM_CLASSES[alarmLvl] ?? "ok";

  const fsrLevel = (val: number): "low" | "mid" | "high" => {
    const pct = val / 4095;
    return pct < 0.3 ? "low" : pct < 0.65 ? "mid" : "high";
  };

  const FSR_LABELS = ["FL", "FR", "RL", "RR"];

  return (
    <div className="app-shell">
      {/* Sidebar */}
      <aside className="sidebar">
        <div className="sidebar-logo">
          <div className="sidebar-logo-icon">
            <Zap size={15} color="#fff" />
          </div>
          <div>
            <div className="sidebar-logo-text">SLI</div>
            <div className="sidebar-logo-sub">Advanced v1.0</div>
          </div>
        </div>

        {NAV_ITEMS.map(item => (
          <div
            key={item.id}
            className={`nav-item ${activeTab === item.id ? "active" : ""}`}
            onClick={() => setActiveTab(item.id)}
          >
            {item.icon}
            {item.label}
          </div>
        ))}

        {/* Connection status in sidebar footer */}
        <div className="sidebar-footer">
          <div className="conn-badge">
            <div className={`conn-dot ${wsStatus === "connected" ? "connected" : wsStatus === "error" ? "error" : ""}`} />
            <span style={{ fontSize: 10 }}>
              {wsStatus === "connected" ? "Live" : wsStatus === "connecting" ? "Connecting..." : "Offline"}
            </span>
          </div>
          {frame && (
            <div style={{ fontSize: 9, color: "var(--text-muted)", marginTop: 3, fontFamily: "var(--font-mono)" }}>
              {new Date(frame.timestamp * 1000).toLocaleTimeString()}
            </div>
          )}
        </div>
      </aside>

      {/* Main content */}
      <div className="main-content">
        {/* Header bar */}
        <div className="header-bar">
          <button
            className="estop-btn"
            onClick={() => apiPost("/estop")}
            id="estop-button"
          >
            <Zap size={13} />
            E-STOP
          </button>

          <div className="header-divider" />

          <div className="conn-status-chip">
            <div className={`conn-dot ${wsStatus === "connected" ? "connected" : ""}`} />
            WS: {wsStatus}
          </div>

          {frame && (
            <>
              <div className="header-divider" />
              <div style={{ fontSize: 11, color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>
                Load: <span style={{ color: "var(--cyan)" }}>{frame.actualLoad.toFixed(2)} kg</span>
              </div>
              <div style={{ fontSize: 11, color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>
                Boom: <span style={{ color: "var(--cyan)" }}>{frame.boomAngle.toFixed(1)}°</span>
              </div>
            </>
          )}

          {/* Alarm chip — right side */}
          <div className={`alarm-chip ${alarmClass}`}>
            <span>●</span>
            {ALARM_LABELS[alarmLvl]}
          </div>
        </div>

        {/* Page content */}
        {activeTab === "dashboard" && (
          <div className="page-content" style={{ overflow: "hidden" }}>
            <div className="dashboard-grid">

              {/* LEFT: Telemetry */}
              <TelemetryPanel frame={frame} />

              {/* CENTER: Motor controls */}
              <div className="glass-card" style={{ overflow: "hidden" }}>
                <MotorControls speed={speed} onSpeedChange={setSpeed} />
              </div>

              {/* RIGHT: Visualizer + Outrigger FSR */}
              <div className="viz-panel">
                {/* Crane canvas */}
                <div className="glass-card crane-canvas-wrapper">
                  <CraneVisualizer frame={frame} />

                  {/* 3 Gauges */}
                  <div className="gauges-row">
                    <Gauge
                      value={frame?.loadPercent ?? 0}
                      max={120}
                      label="Load"
                      unit="%"
                      size={82}
                    />
                    <Gauge
                      value={frame?.boomAngle ?? 0}
                      max={80}
                      label="Boom"
                      unit="°"
                      color="#00E676"
                      size={82}
                    />
                    <Gauge
                      value={Math.abs(frame?.imuRoll ?? 0)}
                      max={15}
                      label="Tilt"
                      unit="°"
                      color="#FFB347"
                      size={82}
                    />
                  </div>
                </div>

                {/* FSR Outrigger view */}
                <div className="glass-card fsr-panel">
                  <div className="section-title">Outrigger Load Distribution</div>
                  <div className="fsr-grid">
                    {(frame?.fsr ?? [0, 0, 0, 0]).map((val, i) => (
                      <div className="fsr-corner" key={i}>
                        <div className="fsr-circle" data-level={fsrLevel(val)}>
                          {Math.round((val / 4095) * 100)}%
                        </div>
                        <div className="fsr-label">{FSR_LABELS[i]}</div>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {activeTab === "debug" && (
          <DebugTab debugReport={debugReport} lastAck={lastAck} />
        )}

        {activeTab === "calibration" && (
          <CalibrationTab />
        )}

        {activeTab === "settings" && (
          <SettingsTab wsConnected={wsStatus === "connected"} />
        )}

        {(activeTab === "datalogger" || activeTab === "loadchart" || activeTab === "controller") && (
          <PlaceholderTab name={activeTab} />
        )}
      </div>
    </div>
  );
}

/* ====================== CALIBRATION TAB ====================== */
function CalibrationTab() {
  const [status, setStatus] = useState<Record<string, string>>({});

  const run = async (key: string, path: string) => {
    setStatus(s => ({ ...s, [key]: "Running..." }));
    try {
      const res = await fetch(`${API_BASE}${path}`, { method: "POST" });
      const data = await res.json();
      setStatus(s => ({ ...s, [key]: data.message ?? (res.ok ? "Done ✓" : "Failed") }));
    } catch {
      setStatus(s => ({ ...s, [key]: "Backend offline" }));
    }
  };

  const cals = [
    { key: "tare",   label: "Tare Load Cell",          sub: "Zero the load cell with no load attached", path: "/calibrate/tare" },
    { key: "imu",    label: "Zero IMU",                 sub: "Set current orientation as flat/zero — keep system still", path: "/calibrate/imu" },
    { key: "tele",   label: "Reset Telescope Position", sub: "Reset telescope encoder to zero (retracted position)", path: "/calibrate/tele" },
  ];

  return (
    <div className="page-content scroll-y">
      <div style={{ maxWidth: 480, display: "flex", flexDirection: "column", gap: 12 }}>
        {cals.map(cal => (
          <div className="glass-card" key={cal.key} style={{ padding: "14px 16px" }}>
            <div style={{ marginBottom: 6 }}>
              <div style={{ fontSize: 13, fontWeight: 600, color: "var(--text-primary)" }}>{cal.label}</div>
              <div style={{ fontSize: 11, color: "var(--text-muted)", marginTop: 2 }}>{cal.sub}</div>
            </div>
            <div style={{ display: "flex", gap: 10, alignItems: "center" }}>
              <button className="btn-primary" onClick={() => run(cal.key, cal.path)}>
                Run
              </button>
              {status[cal.key] && (
                <span style={{ fontSize: 11, color: "var(--text-secondary)", fontFamily: "var(--font-mono)" }}>
                  {status[cal.key]}
                </span>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

/* ====================== PLACEHOLDER TAB ====================== */
function PlaceholderTab({ name }: { name: string }) {
  const label = name.replace(/([A-Z])/g, " $1").replace(/^./, s => s.toUpperCase());
  return (
    <div className="page-content" style={{ display: "flex", alignItems: "center", justifyContent: "center" }}>
      <div style={{ textAlign: "center", color: "var(--text-muted)" }}>
        <div style={{ fontSize: 32, marginBottom: 8 }}>🚧</div>
        <div style={{ fontSize: 14, fontWeight: 600 }}>{label}</div>
        <div style={{ fontSize: 12, marginTop: 4 }}>Coming in Phase 2</div>
      </div>
    </div>
  );
}
