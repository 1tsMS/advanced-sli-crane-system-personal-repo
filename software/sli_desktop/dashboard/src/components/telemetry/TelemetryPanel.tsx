import type { TelemetryFrame } from "../../types";

interface TelemetryPanelProps {
  frame: TelemetryFrame | null;
}

interface Row {
  key: string;
  value: string;
  unit?: string;
  alarm?: "warn" | "danger" | "ok" | null;
  wide?: boolean;
}

function TelRow({ row }: { row: Row }) {
  return (
    <div className="tel-row">
      <span className="tel-key">{row.key}</span>
      <span className={`tel-val ${row.alarm ?? ""}`}>
        {row.value}
        {row.unit && <span style={{ fontSize: 9, marginLeft: 2, opacity: 0.55 }}>{row.unit}</span>}
      </span>
    </div>
  );
}

function Section({ title, rows }: { title: string; rows: Row[] }) {
  return (
    <div className="tel-section">
      <div className="tel-section-label">{title}</div>
      <div className="tel-grid" style={{ gridTemplateColumns: "1fr" }}>
        {rows.map(r => <TelRow key={r.key} row={r} />)}
      </div>
    </div>
  );
}

const fmt = (v: number, d = 2) => (Number.isFinite(v) ? v.toFixed(d) : "--");

export function TelemetryPanel({ frame: f }: TelemetryPanelProps) {
  const pct = f?.loadPercent ?? 0;
  const roll = f?.imuRoll ?? 0;
  const pitch = f?.imuPitch ?? 0;

  const loadAlarm = (pct: number): "ok" | "warn" | "danger" => {
    if (pct >= 100) return "danger";
    if (pct >= 80)  return "warn";
    return "ok";
  };

  const imuAlarm = (deg: number): "warn" | "danger" | null => {
    const a = Math.abs(deg);
    if (a > 8) return "danger";
    if (a > 3) return "warn";
    return null;
  };

  const pctFill = Math.min(pct, 100);
  const pctColor = pct >= 100 ? "#FF3B3B" : pct >= 80 ? "#F5A623" : "#00D68F";

  return (
    <div className="panel" style={{ height: "100%" }}>
      <div className="panel-header">
        <span className="ph-icon">◈</span>
        Telemetry
      </div>
      <div className="panel-body">

        {/* GEOMETRY */}
        <Section title="Geometry" rows={[
          { key: "Boom Angle",   value: fmt(f?.boomAngle   ?? 0), unit: "°" },
          { key: "Swing Angle",  value: fmt(f?.swingAngle  ?? 0), unit: "°" },
          { key: "Extension",    value: fmt(f?.extensionMM ?? 0, 1), unit: "mm" },
          { key: "Rope Length",  value: fmt(f?.ropeLength  ?? 0, 1), unit: "mm" },
        ]} />

        {/* LOAD */}
        <div className="tel-section">
          <div className="tel-section-label">Load</div>
          <div className="tel-grid" style={{ gridTemplateColumns: "1fr" }}>
            <TelRow row={{ key: "Measured", value: fmt(f?.measuredLoad ?? 0, 3), unit: "kg" }} />
            <TelRow row={{ key: "Actual (comp.)", value: fmt(f?.actualLoad ?? 0, 3), unit: "kg", alarm: loadAlarm(pct) }} />
            <TelRow row={{ key: "Safe Limit", value: fmt(f?.safeLoadLimit ?? 0, 2), unit: "kg" }} />
            <TelRow row={{ key: "Load %", value: fmt(pct, 1), unit: "%", alarm: loadAlarm(pct) }} />
          </div>
          {/* Load bar */}
          <div className="load-bar-wrap">
            <div className="load-bar-bg" style={{ marginTop: 5 }}>
              <div
                className="load-bar-fill"
                style={{ width: `${pctFill}%`, background: pctColor }}
              />
            </div>
          </div>
        </div>

        {/* IMU */}
        <Section title="IMU / Orientation" rows={[
          { key: "Roll",  value: fmt(roll,  2), unit: "°", alarm: imuAlarm(roll)  ?? undefined },
          { key: "Pitch", value: fmt(pitch, 2), unit: "°", alarm: imuAlarm(pitch) ?? undefined },
        ]} />

      </div>
    </div>
  );
}
