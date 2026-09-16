import { useRef, useEffect } from "react";
import type { TelemetryFrame } from "../../types";

interface CraneVisualizerProps {
  frame: TelemetryFrame | null;
}

/**
 * Clean side-view crane boom visualizer.
 *
 * Renders:
 *   - Slotted turret base (simple rect)
 *   - Main boom arm (rotates by boomAngle, expands with extensionMM)
 *   - Telescope section inner boom (lighter overlay)
 *   - Rope + hook hanging from boom tip
 *   - Angle arc + label
 *   - Swing indicator (arc at base)
 *   - Safe zone envelope (arc at max radius) — green/amber/red
 */
export function CraneVisualizer({ frame }: CraneVisualizerProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const W = canvas.width;
    const H = canvas.height;

    const boomAngle   = frame?.boomAngle   ?? 50;   // degrees from horizontal
    const extMM       = frame?.extensionMM ?? 0;
    const ropeLength  = frame?.ropeLength  ?? 500;
    const swingAngle  = frame?.swingAngle  ?? 0;
    const alarmLevel  = frame?.alarmLevel  ?? 0;

    ctx.clearRect(0, 0, W, H);

    // ---- Background ----
    ctx.fillStyle = "#080A0E";
    ctx.fillRect(0, 0, W, H);

    // Subtle grid
    ctx.strokeStyle = "rgba(255,255,255,0.025)";
    ctx.lineWidth = 0.5;
    for (let x = 0; x <= W; x += 24) { ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,H); ctx.stroke(); }
    for (let y = 0; y <= H; y += 24) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(W,y); ctx.stroke(); }

    // ---- Pivot point ----
    const pivX = W * 0.28;
    const pivY = H * 0.75;

    // ---- Boom dimensions ----
    const MAX_BOOM_PX = Math.min(W, H) * 0.58;   // Base boom length in pixels
    const extFraction = Math.min(extMM / 600, 1); // Assume 600mm max extension
    const boomPx = MAX_BOOM_PX * (1 + extFraction * 0.4); // 1.0x – 1.4x based on extension

    // Convert boom angle to canvas angle (0° = right, boom goes up-left)
    const angleRad = (boomAngle * Math.PI) / 180;
    const boomEndX = pivX + Math.cos(Math.PI - angleRad) * boomPx;
    const boomEndY = pivY - Math.sin(angleRad) * boomPx;

    // ---- Safe load zone arc ----
    const safeStroke = alarmLevel === 0 ? "#00D68F" : alarmLevel === 1 ? "#F5A623" : "#FF3B3B";

    ctx.strokeStyle = safeStroke;
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 4]);
    ctx.globalAlpha = 0.4;
    ctx.beginPath();
    ctx.arc(pivX, pivY, boomPx + 8, -Math.PI, 0);
    ctx.stroke();
    ctx.globalAlpha = 1;
    ctx.setLineDash([]);

    // ---- Swing arc at base ----
    if (Math.abs(swingAngle) > 0.5) {
      ctx.strokeStyle = "rgba(0,212,255,0.3)";
      ctx.lineWidth = 1;
      ctx.setLineDash([2, 3]);
      ctx.beginPath();
      ctx.arc(pivX, pivY, 28,
        -Math.PI / 2 - (swingAngle * Math.PI) / 180,
        -Math.PI / 2
      );
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // ---- Angle arc from horizontal ----
    ctx.strokeStyle = "rgba(0,212,255,0.2)";
    ctx.lineWidth = 1;
    ctx.setLineDash([2, 3]);
    ctx.beginPath();
    ctx.arc(pivX, pivY, 42, Math.PI, Math.PI + angleRad, false);
    ctx.stroke();
    ctx.setLineDash([]);

    // ---- Ground line ----
    ctx.strokeStyle = "rgba(255,255,255,0.07)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, pivY + 18);
    ctx.lineTo(W, pivY + 18);
    ctx.stroke();

    // ---- Turret base ----
    // Trapezoid base
    ctx.fillStyle = "#1A1F2C";
    ctx.strokeStyle = "rgba(0,212,255,0.25)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(pivX - 22, pivY + 18);
    ctx.lineTo(pivX + 22, pivY + 18);
    ctx.lineTo(pivX + 14, pivY);
    ctx.lineTo(pivX - 14, pivY);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    // Turret circle
    ctx.fillStyle = "#232938";
    ctx.strokeStyle = "rgba(0,212,255,0.35)";
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(pivX, pivY, 10, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // ---- Main boom arm ----
    const boomColor = alarmLevel === 0 ? "#00D4FF" : alarmLevel === 1 ? "#F5A623" : "#FF3B3B";
    const boomW = 9;

    // Shadow / glow
    ctx.save();
    ctx.shadowColor = boomColor;
    ctx.shadowBlur = 8;
    ctx.strokeStyle = boomColor;
    ctx.lineWidth = boomW;
    ctx.lineCap = "round";
    ctx.beginPath();
    ctx.moveTo(pivX, pivY);
    ctx.lineTo(boomEndX, boomEndY);
    ctx.stroke();
    ctx.restore();

    // Boom body
    ctx.strokeStyle = boomColor;
    ctx.lineWidth = boomW;
    ctx.lineCap = "round";
    ctx.globalAlpha = 0.85;
    ctx.beginPath();
    ctx.moveTo(pivX, pivY);
    ctx.lineTo(boomEndX, boomEndY);
    ctx.stroke();
    ctx.globalAlpha = 1;

    // Lattice pattern on boom
    const latticeStep = 14;
    const boomDx = boomEndX - pivX;
    const boomDy = boomEndY - pivY;
    const boomLen = Math.sqrt(boomDx*boomDx + boomDy*boomDy);
    const boomAngleActual = Math.atan2(boomDy, boomDx);
    const perp = boomAngleActual + Math.PI / 2;

    ctx.strokeStyle = "rgba(0,0,0,0.4)";
    ctx.lineWidth = 1;
    for (let d = latticeStep; d < boomLen - 6; d += latticeStep) {
      const t = d / boomLen;
      const mx = pivX + boomDx * t;
      const my = pivY + boomDy * t;
      const w = (boomW / 2) * 0.7;
      ctx.beginPath();
      ctx.moveTo(mx + Math.cos(perp) * w, my + Math.sin(perp) * w);
      ctx.lineTo(mx - Math.cos(perp) * w, my - Math.sin(perp) * w);
      ctx.stroke();
    }

    // ---- Extension inner boom ----
    if (extFraction > 0.02) {
      const innerStart = 0.35; // starts 35% from base
      const innerX0 = pivX + boomDx * innerStart;
      const innerY0 = pivY + boomDy * innerStart;

      ctx.strokeStyle = "rgba(255,255,255,0.5)";
      ctx.lineWidth = 4;
      ctx.lineCap = "round";
      ctx.setLineDash([4, 3]);
      ctx.beginPath();
      ctx.moveTo(innerX0, innerY0);
      ctx.lineTo(boomEndX, boomEndY);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // ---- Jib head ----
    ctx.fillStyle = boomColor;
    ctx.globalAlpha = 0.9;
    ctx.beginPath();
    ctx.arc(boomEndX, boomEndY, 4, 0, Math.PI * 2);
    ctx.fill();
    ctx.globalAlpha = 1;

    // ---- Rope ----
    const maxRopePx = H * 0.25;
    const ropePx = Math.min((ropeLength / 2000) * maxRopePx, maxRopePx);

    ctx.strokeStyle = "rgba(200,210,230,0.5)";
    ctx.lineWidth = 1;
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(boomEndX, boomEndY);
    ctx.lineTo(boomEndX, boomEndY + ropePx);
    ctx.stroke();

    // Hook
    const hookX = boomEndX;
    const hookY = boomEndY + ropePx;
    ctx.strokeStyle = "#F5A623";
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.arc(hookX, hookY + 4, 4, 0, Math.PI * 1.7);
    ctx.stroke();

    // ---- Labels ----
    ctx.fillStyle = "rgba(0,212,255,0.7)";
    ctx.font = "500 10px Inter, sans-serif";
    ctx.textAlign = "center";

    // Angle label on arc
    const arcLabelR = 55;
    const arcLabelAngle = Math.PI + angleRad * 0.5;
    ctx.fillText(
      `${boomAngle.toFixed(1)}°`,
      pivX + Math.cos(arcLabelAngle) * arcLabelR,
      pivY + Math.sin(arcLabelAngle) * arcLabelR + 3
    );

    // Extension label near tip
    if (extFraction > 0.05) {
      ctx.fillStyle = "rgba(255,255,255,0.35)";
      ctx.font = "9px JetBrains Mono, monospace";
      const midBoomX = pivX + boomDx * 0.65;
      const midBoomY = pivY + boomDy * 0.65 - 10;
      ctx.fillText(`+${extMM.toFixed(0)}mm`, midBoomX, midBoomY);
    }

    // Swing label
    if (Math.abs(swingAngle) > 0.5) {
      ctx.fillStyle = "rgba(0,212,255,0.5)";
      ctx.font = "9px Inter, sans-serif";
      ctx.fillText(`⟳ ${swingAngle.toFixed(1)}°`, pivX, pivY - 32);
    }

  }, [frame]);

  return (
    <canvas
      ref={canvasRef}
      width={320}
      height={240}
      className="crane-canvas"
    />
  );
}
