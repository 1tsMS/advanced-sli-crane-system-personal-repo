import { useState } from "react";
import {
  LayoutDashboard, Terminal, Database,
  Activity, Settings, Zap, Cpu
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

const NAV: { id: TabId; icon: React.ReactNode; label: string }[] = [
  { id: "dashboard",  icon: <LayoutDashboard size={16} />, label: "Dashboard" },
  { id: "debug",      icon: <Terminal size={16} />,        label: "Debug & Calibrate" },
  { id: "datalogger", icon: <Database size={16} />,        label: "Data Logger" },
  { id: "loadchart",  icon: <Activity size={16} />,        label: "Load Chart" },
  { id: "settings",   icon: <Settings size={16} />,        label: "Settings" },
];

const ALARM_LABEL  = ["ONLINE", "WARNING", "CRITICAL", "E-STOP"];
const ALARM_CLASS  = ["ok",     "warn",    "critical",  "estop" ];

async function estop() {
  try { await fetch(`${API_BASE}/estop`, { method: "POST" }); } catch { /* offline */ }
}

function fsrLevel(val: number): "off" | "low" | "mid" | "high" {
  if (val < 50)  return "off";
  const p = val / 4095;
  if (p < 0.3)   return "low";
  if (p < 0.65)  return "mid";
  return "high";
}

export default function App() {
  const [tab, setTab]     = useState<TabId>("dashboard");
  const [speed, setSpeed] = useState(150);

  const { frame, debugReport, lastAck, wsStatus } = useTelemetry();

  const alarm = frame?.alarmLevel ?? 0;
  const alarmClass = ALARM_CLASS[alarm];
  const alarmLabel = wsStatus === "connected"
    ? ALARM_LABEL[alarm]
    : wsStatus === "connecting" ? "CONNECTING" : "OFFLINE";
  const alarmPillClass = wsStatus !== "connected" ? "ok" : alarmClass;

  const fsr = frame?.fsr ?? [0, 0, 0, 0];

  return (
    <div className="app-shell">

      {/* Icon Sidebar */}
      <aside className="sidebar">
        <div className="sidebar-brand">
          <Cpu size={14} color="#fff" />
        </div>

        {NAV.map(n => (
          <div
            key={n.id}
            className={`nav-btn ${tab === n.id ? "active" : ""}`}
            onClick={() => setTab(n.id)}
            title={n.label}
          >
            {n.icon}
            <span className="tooltip">{n.label}</span>
          </div>
        ))}

        <div className="sidebar-spacer" />
        <div
          className={`sidebar-conn ${wsStatus === "connected" ? "live" : ""}`}
          title={wsStatus}
        />
      </aside>

      {/* Main area */}
      <div className="main-area">

        {/* Header */}
        <div className="header">
          <span className="header-title">
            Advanced SLI
          </span>

          <div className="header-sep" />

          {/* Live stats — only show when connected + have data */}
          {frame && (
            <>
              <div className="header-stat">
                Boom <span className="header-stat-val">{frame.boomAngle.toFixed(1)}°</span>
              </div>
              <div className="header-stat">
                Load <span className="header-stat-val">{frame.actualLoad.toFixed(2)} kg</span>
              </div>
              <div className="header-stat">
                <span className="header-stat-val" style={{
                  color: frame.loadPercent >= 100 ? "var(--red)" : frame.loadPercent >= 80 ? "var(--amber)" : "var(--cyan)"
                }}>
                  {frame.loadPercent.toFixed(1)}%
                </span>
              </div>
            </>
          )}

          <div className="header-fill" />

          {/* Alarm pill */}
          <div className={`alarm-pill ${alarmPillClass}`}>
            <span>●</span>
            {alarmLabel}
          </div>

          <div className="header-sep" />

          <button className="estop-btn" id="estop-button" onClick={estop}>
            <Zap size={12} />
            E-STOP
          </button>
        </div>

        {/* Page content */}
        <div className="page">

          {/* ===== DASHBOARD ===== */}
          {tab === "dashboard" && (
            <div className="dash-grid">

              {/* Left: Telemetry */}
              <TelemetryPanel frame={frame} />

              {/* Center: Visualizer + Motor Controls */}
              <div className="center-col">
                {/* Crane vis */}
                <div className="panel crane-panel">
                  <div className="panel-header">
                    <span className="ph-icon">◈</span>
                    Crane View
                  </div>
                  <div className="crane-canvas-wrap">
                    <CraneVisualizer frame={frame} />
                  </div>
                  {/* Gauges */}
                  <div className="gauges-row">
                    <Gauge value={frame?.loadPercent ?? 0} max={120} label="Load" unit="%" size={80} />
                    <Gauge value={frame?.boomAngle ?? 0} max={80} label="Boom" unit="°" color="#00D68F" size={80} />
                    <Gauge value={Math.abs(frame?.imuRoll ?? 0)} max={15} label="Tilt" unit="°" color="#F5A623" size={80} />
                  </div>
                </div>

                {/* Motor controls below */}
                <div style={{ flexShrink: 0, minHeight: 0 }}>
                  <MotorControls speed={speed} onSpeedChange={setSpeed} />
                </div>
              </div>

              {/* Right: FSR */}
              <div className="right-col">
                <div className="panel fsr-panel">
                  <div className="panel-header">
                    <span className="ph-icon">◈</span>
                    Outrigger Load
                  </div>
                  <div className="fsr-body">
                    <div className="fsr-layout">
                      {/* FL */}
                      <div className="fsr-dot-wrap" style={{ justifySelf: "center" }}>
                        <div className="fsr-dot" data-level={fsrLevel(fsr[0])}>
                          {fsr[0] > 50 ? `${Math.round(fsr[0]/4095*100)}%` : "—"}
                        </div>
                        <div className="fsr-dot-label">FL</div>
                      </div>

                      {/* Top spacer */}
                      <div />

                      {/* FR */}
                      <div className="fsr-dot-wrap" style={{ justifySelf: "center" }}>
                        <div className="fsr-dot" data-level={fsrLevel(fsr[1])}>
                          {fsr[1] > 50 ? `${Math.round(fsr[1]/4095*100)}%` : "—"}
                        </div>
                        <div className="fsr-dot-label">FR</div>
                      </div>

                      {/* Left spacer */}
                      <div />

                      {/* Center label */}
                      <div className="fsr-center-label">
                        <div style={{ fontSize: 10, color: "var(--text-secondary)", fontWeight: 600 }}>BASE</div>
                      </div>

                      {/* Right spacer */}
                      <div />

                      {/* RL */}
                      <div className="fsr-dot-wrap" style={{ justifySelf: "center" }}>
                        <div className="fsr-dot" data-level={fsrLevel(fsr[2])}>
                          {fsr[2] > 50 ? `${Math.round(fsr[2]/4095*100)}%` : "—"}
                        </div>
                        <div className="fsr-dot-label">RL</div>
                      </div>

                      {/* Bottom spacer */}
                      <div />

                      {/* RR */}
                      <div className="fsr-dot-wrap" style={{ justifySelf: "center" }}>
                        <div className="fsr-dot" data-level={fsrLevel(fsr[3])}>
                          {fsr[3] > 50 ? `${Math.round(fsr[3]/4095*100)}%` : "—"}
                        </div>
                        <div className="fsr-dot-label">RR</div>
                      </div>
                    </div>

                    {/* Balance indicator */}
                    {fsr.some(v => v > 50) && (
                      <div style={{ fontSize: 10, color: "var(--text-muted)", textAlign: "center", marginTop: 4 }}>
                        {fsr.every(v => v > 50)
                          ? <span style={{ color: "var(--green)" }}>● All legs grounded</span>
                          : <span style={{ color: "var(--amber)" }}>⚠ Check outriggers</span>
                        }
                      </div>
                    )}
                  </div>
                </div>
              </div>

            </div>
          )}

          {/* ===== DEBUG & CALIBRATE ===== */}
          {tab === "debug" && (
            <DebugTab debugReport={debugReport} lastAck={lastAck} />
          )}

          {/* ===== SETTINGS ===== */}
          {tab === "settings" && (
            <SettingsTab wsConnected={wsStatus === "connected"} />
          )}

          {/* ===== PLACEHOLDERS ===== */}
          {(tab === "datalogger" || tab === "loadchart") && (
            <PlaceholderTab name={tab} />
          )}

        </div>
      </div>
    </div>
  );
}

function PlaceholderTab({ name }: { name: string }) {
  const labels: Record<string, string> = {
    datalogger: "Data Logger",
    loadchart:  "Load Chart Editor",
  };
  return (
    <div style={{
      flex: 1, display: "flex", flexDirection: "column",
      alignItems: "center", justifyContent: "center",
      color: "var(--text-muted)", gap: 8,
    }}>
      <Database size={32} strokeWidth={1} />
      <div style={{ fontSize: 13, fontWeight: 600 }}>{labels[name]}</div>
      <div style={{ fontSize: 11 }}>Coming in Phase 2</div>
    </div>
  );
}
