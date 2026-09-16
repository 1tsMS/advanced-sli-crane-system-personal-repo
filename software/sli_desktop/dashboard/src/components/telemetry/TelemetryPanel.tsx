import type { TelemetryFrame } from "../../types";

interface TelemetryPanelProps {
  frame: TelemetryFrame | null;
}

interface TelItem {
  label: string;
  value: string;
  unit?: string;
  color?: string;
}

function TelRow({ label, value, unit, color }: TelItem) {
  return (
    <div className="tel-item glass-card">
      <div className="tel-label">{label}</div>
      <div className="tel-value" style={color ? { color } : undefined}>
        {value}
        {unit && <span className="tel-unit">{unit}</span>}
      </div>
    </div>
  );
}

/**
 * Left-side telemetry panel — all sensor readings in a scrollable list.
 */
export function TelemetryPanel({ frame }: TelemetryPanelProps) {
  const f = frame;

  const alarmColor = (lvl: number) => {
    if (lvl === 0) return "var(--green)";
    if (lvl === 1) return "var(--amber)";
    return "var(--red)";
  };

  const alarmLabel = (lvl: number) => {
    return ["OK", "WARNING", "CRITICAL", "E-STOP"][lvl] ?? "--";
  };

  const rows: TelItem[] = [
    {
      label: "Boom Angle",
      value: f ? f.boomAngle.toFixed(2) : "--",
      unit: "°",
    },
    {
      label: "Swing Angle",
      value: f ? f.swingAngle.toFixed(2) : "--",
      unit: "°",
    },
    {
      label: "Extension",
      value: f ? f.extensionMM.toFixed(1) : "--",
      unit: "mm",
    },
    {
      label: "Rope Length",
      value: f ? f.ropeLength.toFixed(1) : "--",
      unit: "mm",
    },
    {
      label: "Measured Load",
      value: f ? f.measuredLoad.toFixed(2) : "--",
      unit: "kg",
    },
    {
      label: "Actual Load",
      value: f ? f.actualLoad.toFixed(2) : "--",
      unit: "kg",
      color: f && f.loadPercent > 85 ? "var(--red)" : undefined,
    },
    {
      label: "Safe Limit",
      value: f ? f.safeLoadLimit.toFixed(2) : "--",
      unit: "kg",
    },
    {
      label: "Load %",
      value: f ? f.loadPercent.toFixed(1) : "--",
      unit: "%",
      color: f
        ? f.loadPercent >= 100
          ? "var(--red)"
          : f.loadPercent >= 80
          ? "var(--amber)"
          : "var(--green)"
        : undefined,
    },
    {
      label: "IMU Roll",
      value: f ? f.imuRoll.toFixed(2) : "--",
      unit: "°",
      color: f && Math.abs(f.imuRoll) > 5 ? "var(--amber)" : undefined,
    },
    {
      label: "IMU Pitch",
      value: f ? f.imuPitch.toFixed(2) : "--",
      unit: "°",
      color: f && Math.abs(f.imuPitch) > 5 ? "var(--amber)" : undefined,
    },
    {
      label: "FSR FL",
      value: f ? String(f.fsr[0]) : "--",
    },
    {
      label: "FSR FR",
      value: f ? String(f.fsr[1]) : "--",
    },
    {
      label: "FSR RL",
      value: f ? String(f.fsr[2]) : "--",
    },
    {
      label: "FSR RR",
      value: f ? String(f.fsr[3]) : "--",
    },
    {
      label: "Alarm",
      value: f ? alarmLabel(f.alarmLevel) : "--",
      color: f ? alarmColor(f.alarmLevel) : undefined,
    },
  ];

  return (
    <div className="telemetry-panel scroll-y">
      {rows.map(row => (
        <TelRow key={row.label} {...row} />
      ))}
    </div>
  );
}
