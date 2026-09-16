import { useEffect, useRef } from "react";

interface GaugeProps {
  value: number;
  min?: number;
  max: number;
  label: string;
  unit?: string;
  color?: string;
  size?: number;
}

/**
 * Circular arc gauge using Canvas 2D.
 * Renders a 240° arc from min to max, colored by value level.
 */
export function Gauge({
  value,
  min = 0,
  max,
  label,
  unit = "",
  color = "#00D4FF",
  size = 90,
}: GaugeProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const dpr = window.devicePixelRatio || 1;
    canvas.width  = size * dpr;
    canvas.height = size * dpr;
    canvas.style.width  = `${size}px`;
    canvas.style.height = `${size}px`;
    ctx.scale(dpr, dpr);

    const cx = size / 2, cy = size / 2;
    const radius = size * 0.38;
    const startAngle = (135 * Math.PI) / 180;   // 225° on clock → bottom-left
    const totalAngle = (270 * Math.PI) / 180;    // Full sweep = 270°

    const pct = Math.max(0, Math.min((value - min) / (max - min), 1));
    const endAngle = startAngle + totalAngle * pct;

    ctx.clearRect(0, 0, size, size);

    // Track background
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, startAngle + totalAngle);
    ctx.strokeStyle = "rgba(255,255,255,0.07)";
    ctx.lineWidth = size * 0.1;
    ctx.lineCap = "round";
    ctx.stroke();

    // Value arc — gradient based on level
    const fillColor = pct < 0.6
      ? color
      : pct < 0.85
      ? "#FFB347"
      : "#FF3B3B";

    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, endAngle);
    ctx.strokeStyle = fillColor;
    ctx.lineWidth = size * 0.1;
    ctx.lineCap = "round";
    ctx.stroke();

    // Glow effect
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, endAngle);
    ctx.strokeStyle = fillColor.replace(")", ", 0.3)").replace("rgb(", "rgba(");
    ctx.lineWidth = size * 0.18;
    ctx.lineCap = "round";
    ctx.globalAlpha = 0.25;
    ctx.stroke();
    ctx.globalAlpha = 1;

    // Center value text
    ctx.fillStyle = "#E8EAF0";
    ctx.font = `bold ${size * 0.18}px JetBrains Mono, monospace`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    const displayVal = Number.isFinite(value)
      ? Math.abs(value) >= 100
        ? value.toFixed(0)
        : value.toFixed(1)
      : "--";
    ctx.fillText(displayVal, cx, cy - size * 0.03);

    // Unit
    if (unit) {
      ctx.fillStyle = "rgba(255,255,255,0.3)";
      ctx.font = `${size * 0.1}px Inter, sans-serif`;
      ctx.fillText(unit, cx, cy + size * 0.14);
    }

    // Label below
    ctx.fillStyle = "rgba(255,255,255,0.35)";
    ctx.font = `600 ${size * 0.1}px Inter, sans-serif`;
    ctx.fillText(label.toUpperCase(), cx, cy + size * 0.38);

  }, [value, min, max, label, unit, color, size]);

  return (
    <canvas
      ref={canvasRef}
      style={{ display: "block" }}
    />
  );
}
