import type { TelemetryFrame } from "../../types";
import { Compass, Scale, ShieldAlert, ArrowUpRight } from "lucide-react";

interface TelemetryPanelProps {
  frame: TelemetryFrame | null;
}

const fmt = (v: number, d = 2) => (Number.isFinite(v) ? v.toFixed(d) : "--");

export function TelemetryPanel({ frame: f }: TelemetryPanelProps) {
  const actualLoad = f?.actualLoad ?? 0;
  const safeLimit = f?.safeLoadLimit ?? 5.0;

  // Sanity check: on a 5kg model crane, anything >10kg is uncalibrated raw ADC or sensor error
  const isLoadError = actualLoad > 10.0 || actualLoad < -0.5 || (f?.loadPercent ?? 0) > 900;
  const pct = isLoadError ? 0 : (f?.loadPercent ?? (safeLimit > 0 ? (actualLoad / safeLimit) * 100 : 0));
  const boomAngle = f?.boomAngle ?? 0;
  const extMM = f?.extensionMM ?? 0;
  const ropeLen = f?.ropeLength ?? 0;
  const swingAngle = f?.swingAngle ?? 0;
  const roll = f?.imuRoll ?? 0;
  const pitch = f?.imuPitch ?? 0;

  // Operating radius calculated from real base boom (22.5cm = 0.225m) + telescope extension
  const nominalBoomM = (225 + Math.max(0, extMM)) / 1000;
  const operatingRadiusM = nominalBoomM * Math.cos((boomAngle * Math.PI) / 180);

  // Status classification
  const isOverload = !isLoadError && pct >= 100;
  const isWarn = !isLoadError && pct >= 80 && pct < 100;
  const statusColor = isLoadError ? "var(--amber)" : isOverload ? "var(--red)" : isWarn ? "var(--amber)" : "var(--green)";
  const statusLabel = isLoadError ? "LOAD SENSOR ERR" : isOverload ? "OVERLOAD" : isWarn ? "WARNING" : "NORMAL";

  const imuWarn = Math.abs(roll) > 5 || Math.abs(pitch) > 5;

  return (
    <div className="panel tel-panel">
      <div className="panel-header">
        <span className="ph-icon">◈</span>
        <span>Crane Telemetry</span>
        <span className="ph-badge" style={{ borderColor: statusColor, color: statusColor }}>
          {statusLabel}
        </span>
      </div>

      <div className="panel-body tel-body">
        {/* HERO LOAD CARD */}
        <div className="tel-hero-card">
          <div className="hero-card-label">
            <Scale size={13} color="var(--cyan)" />
            <span>LOAD & CAPACITY</span>
          </div>

          <div className="hero-load-display">
            {isLoadError ? (
              <>
                <span className="hero-load-value" style={{ color: "var(--amber)", fontSize: "1.4rem" }}>
                  ERR (&gt;10kg)
                </span>
                <span className="hero-load-unit" style={{ color: "var(--text-muted)", fontSize: "0.75rem" }}>
                  [Raw: {fmt(f?.loadCellRaw ?? actualLoad, 0)}]
                </span>
              </>
            ) : (
              <>
                <span className="hero-load-value" style={{ color: statusColor }}>
                  {fmt(actualLoad, 2)}
                </span>
                <span className="hero-load-unit">kg</span>
              </>
            )}
            <span className="hero-load-limit">/ {fmt(safeLimit, 2)} kg MAX</span>
          </div>

          {/* Large Capacity Progress Bar */}
          <div className="hero-progress-track">
            <div
              className="hero-progress-fill"
              style={{
                width: isLoadError ? "100%" : `${Math.min(100, Math.max(0, pct))}%`,
                background: statusColor,
                opacity: isLoadError ? 0.4 : 1,
              }}
            />
            <div className="hero-progress-marker warn" style={{ left: "80%" }} />
            <div className="hero-progress-marker danger" style={{ left: "100%" }} />
          </div>

          <div className="hero-progress-meta">
            <span className="hero-pct-label" style={{ color: statusColor }}>
              {isLoadError ? "UNCALIBRATED / OVERFLOW" : `${fmt(pct, 1)}% SWL`}
            </span>
            <span className="hero-raw-sub">
              {isLoadError ? "Tare/Calibrate in Debug" : `Sens: ${fmt(f?.measuredLoad ?? 0, 2)}kg`}
            </span>
          </div>
        </div>

        {/* SECTION: BOOM & GEOMETRY */}
        <div className="tel-group">
          <div className="tel-group-header">
            <ArrowUpRight size={13} />
            <span>BOOM GEOMETRY</span>
          </div>

          <div className="tel-card-grid">
            <div className="tel-stat-card">
              <span className="stat-label">Angle</span>
              <span className="stat-value highlight">
                {fmt(boomAngle, 1)}<span className="stat-unit">°</span>
              </span>
            </div>

            <div className="tel-stat-card">
              <span className="stat-label">Radius</span>
              <span className="stat-value">
                {fmt(operatingRadiusM, 2)}<span className="stat-unit">m</span>
              </span>
            </div>

            <div className="tel-stat-card">
              <span className="stat-label">Extension</span>
              <span className="stat-value">
                {fmt(extMM, 0)}<span className="stat-unit">mm</span>
              </span>
            </div>

            <div className="tel-stat-card">
              <span className="stat-label">Rope Length</span>
              <span className="stat-value">
                {fmt(ropeLen, 0)}<span className="stat-unit">mm</span>
              </span>
            </div>
          </div>
        </div>

        {/* SECTION: ORIENTATION & STABILITY */}
        <div className="tel-group">
          <div className="tel-group-header">
            <Compass size={13} />
            <span>STABILITY & IMU</span>
          </div>

          <div className="tel-card-grid">
            <div className="tel-stat-card">
              <span className="stat-label">Chassis Roll</span>
              <span className={`stat-value ${Math.abs(roll) > 5 ? "danger" : Math.abs(roll) > 3 ? "warn" : ""}`}>
                {fmt(roll, 1)}<span className="stat-unit">°</span>
              </span>
            </div>

            <div className="tel-stat-card">
              <span className="stat-label">Chassis Pitch</span>
              <span className={`stat-value ${Math.abs(pitch) > 5 ? "danger" : Math.abs(pitch) > 3 ? "warn" : ""}`}>
                {fmt(pitch, 1)}<span className="stat-unit">°</span>
              </span>
            </div>

            <div className="tel-stat-card" style={{ gridColumn: "span 2" }}>
              <span className="stat-label">Slew / Turntable Swing</span>
              <span className="stat-value">
                {fmt(swingAngle, 1)}<span className="stat-unit">°</span>
              </span>
            </div>
          </div>
        </div>

        {/* SECTION: SYSTEM STATUS */}
        <div className="tel-group">
          <div className="tel-group-header">
            <ShieldAlert size={13} />
            <span>SAFETY INTERLOCKS</span>
          </div>

          <div className="tel-status-list">
            <div className="tel-status-item">
              <span className="status-item-name">Sway / Tip Protection</span>
              <span className={`status-item-tag ${imuWarn ? "warn" : "ok"}`}>
                {imuWarn ? "TILT WARNING" : "STABLE"}
              </span>
            </div>

            <div className="tel-status-item">
              <span className="status-item-name">Load Limiter Cutoff</span>
              <span className={`status-item-tag ${isOverload ? "danger" : "ok"}`}>
                {isOverload ? "TRIPPED" : "ARMED"}
              </span>
            </div>
          </div>
        </div>

      </div>
    </div>
  );
}
