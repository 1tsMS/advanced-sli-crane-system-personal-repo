import { useRef, useEffect } from "react";
import type { TelemetryFrame } from "../../types";

interface CraneVisualizerProps {
  frame: TelemetryFrame | null;
}

/**
 * 2D canvas crane visualizer.
 *
 * Renders:
 *   - Truck/base body
 *   - Outrigger legs (4x) with FSR-based load coloring
 *   - Boom arm (rotates by boomAngle)
 *   - Telescope extension indicator on the boom
 *   - Rope/hook (length visualized)
 *   - Swing indicator arc (swingAngle)
 *   - IMU tilt indicator
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

    // Defaults when no data
    const boomAngle  = frame?.boomAngle  ?? 45;
    const swingAngle = frame?.swingAngle ?? 0;
    const extMM      = frame?.extensionMM ?? 0;
    const ropeLen    = frame?.ropeLength  ?? 0;
    const fsr        = frame?.fsr ?? [0, 0, 0, 0];
    const alarmLvl   = frame?.alarmLevel  ?? 0;

    ctx.clearRect(0, 0, W, H);

    // ---- Background grid ----
    ctx.strokeStyle = "rgba(255,255,255,0.04)";
    ctx.lineWidth = 0.5;
    for (let x = 0; x < W; x += 20) {
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke();
    }
    for (let y = 0; y < H; y += 20) {
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
    }

    const cx = W * 0.5;
    const baseY = H * 0.72;

    // ---- Outrigger arms (4 corners) ----
    const outriggerPositions = [
      { x: cx - 60, y: baseY + 5,  label: "FL" },
      { x: cx + 60, y: baseY + 5,  label: "FR" },
      { x: cx - 60, y: baseY + 20, label: "RL" },
      { x: cx + 60, y: baseY + 20, label: "RR" },
    ];

    outriggerPositions.forEach((op, i) => {
      const fsrVal = fsr[i] ?? 0;
      const level = fsrVal / 4095;
      const color = level < 0.3
        ? "#00E676"
        : level < 0.65
        ? "#FFB347"
        : "#FF3B3B";

      // Arm line
      ctx.strokeStyle = "rgba(255,255,255,0.25)";
      ctx.lineWidth = 3;
      ctx.setLineDash([]);
      ctx.beginPath();
      ctx.moveTo(cx, baseY + 10);
      ctx.lineTo(op.x, op.y);
      ctx.stroke();

      // Foot pad
      ctx.fillStyle = color;
      ctx.globalAlpha = 0.8;
      ctx.beginPath();
      ctx.ellipse(op.x, op.y + 8, 10, 4, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.globalAlpha = 1.0;
    });

    // ---- Truck body ----
    const truckW = 80, truckH = 28;
    ctx.fillStyle = "rgba(30, 38, 55, 0.95)";
    ctx.strokeStyle = "rgba(0, 212, 255, 0.5)";
    ctx.lineWidth = 1.5;
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.roundRect(cx - truckW / 2, baseY - truckH / 2, truckW, truckH, 5);
    ctx.fill();
    ctx.stroke();

    // Cab
    ctx.fillStyle = "rgba(40, 50, 70, 0.9)";
    ctx.beginPath();
    ctx.roundRect(cx - truckW / 2, baseY - truckH / 2 - 14, 26, 18, 4);
    ctx.fill();
    ctx.stroke();

    // Wheels
    const wheelPositions = [cx - 25, cx, cx + 25];
    wheelPositions.forEach(wx => {
      ctx.fillStyle = "#1A1E28";
      ctx.strokeStyle = "rgba(255,255,255,0.2)";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.ellipse(wx, baseY + truckH / 2, 6, 5, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
    });

    // ---- Turntable base ----
    ctx.fillStyle = "rgba(0, 212, 255, 0.1)";
    ctx.strokeStyle = "rgba(0, 212, 255, 0.4)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.ellipse(cx, baseY - truckH / 2, 22, 8, 0, 0, Math.PI * 2);
    ctx.fill();
    ctx.stroke();

    // ---- Swing angle indicator arc ----
    const swingRad = (swingAngle * Math.PI) / 180;
    ctx.strokeStyle = "rgba(0, 212, 255, 0.3)";
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 3]);
    ctx.beginPath();
    ctx.arc(cx, baseY - truckH / 2, 32, -Math.PI / 2 - 0.3, -Math.PI / 2 + swingRad, false);
    ctx.stroke();
    ctx.setLineDash([]);

    // ---- Boom arm ----
    const boomRad = ((90 - boomAngle) * Math.PI) / 180;  // Convert to canvas angle
    const boomBaseX = cx;
    const boomBaseY = baseY - truckH / 2;

    // Max boom length in pixels
    // Extension percentage (0-1)
    const extPct = Math.min(extMM / 500, 1);  // Assume 500mm max extension
    const boomLength = 70 + extPct * 40;  // 70-110px range

    const boomTipX = boomBaseX + Math.cos(boomRad) * boomLength;
    const boomTipY = boomBaseY - Math.sin(boomRad) * boomLength;  // Y inverted

    // Boom fill color by alarm level
    const boomColor = alarmLvl === 0
      ? "rgba(0, 212, 255, 0.8)"
      : alarmLvl === 1
      ? "rgba(255, 179, 71, 0.8)"
      : "rgba(255, 59, 59, 0.8)";

    // Boom arm (thick trapezoid approximated as lines)
    ctx.strokeStyle = boomColor;
    ctx.lineWidth = 8;
    ctx.lineCap = "round";
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(boomBaseX, boomBaseY);
    ctx.lineTo(boomTipX, boomTipY);
    ctx.stroke();

    // Extension section (lighter, thinner)
    if (extPct > 0.05) {
      const innerLength = 70 * (1 - extPct * 0.3);
      const innerX = boomBaseX + Math.cos(boomRad) * innerLength;
      const innerY = boomBaseY - Math.sin(boomRad) * innerLength;
      ctx.strokeStyle = "rgba(255,255,255,0.4)";
      ctx.lineWidth = 4;
      ctx.setLineDash([4, 4]);
      ctx.beginPath();
      ctx.moveTo(innerX, innerY);
      ctx.lineTo(boomTipX, boomTipY);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // ---- Rope / hook ----
    const ropePx = Math.min((ropeLen / 2000) * 60, 60);  // Max 60px rope
    const hookX = boomTipX;
    const hookY = boomTipY + ropePx;

    ctx.strokeStyle = "rgba(255,255,255,0.5)";
    ctx.lineWidth = 1;
    ctx.setLineDash([]);
    ctx.beginPath();
    ctx.moveTo(boomTipX, boomTipY);
    ctx.lineTo(hookX, hookY);
    ctx.stroke();

    // Hook
    ctx.strokeStyle = "rgba(255,179,71,0.9)";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.arc(hookX, hookY + 4, 4, 0, Math.PI * 1.7);
    ctx.stroke();

    // ---- IMU tilt indicator (bottom center) ----
    const tiltX = cx;
    const tiltY = H - 18;
    const imuRoll = frame?.imuRoll ?? 0;
    const tiltPx = (imuRoll / 30) * 20;  // ±30° maps to ±20px
    ctx.fillStyle = Math.abs(imuRoll) > 5
      ? "rgba(255,179,71,0.8)"
      : "rgba(0,212,255,0.5)";
    ctx.fillRect(tiltX - 20, tiltY - 2, 40, 4);
    ctx.fillStyle = "rgba(0,212,255,1.0)";
    ctx.beginPath();
    ctx.arc(tiltX + tiltPx, tiltY, 5, 0, Math.PI * 2);
    ctx.fill();

    // Angle labels
    ctx.fillStyle = "rgba(255,255,255,0.4)";
    ctx.font = "10px Inter, sans-serif";
    ctx.textAlign = "center";
    ctx.fillText(`Boom: ${boomAngle.toFixed(1)}°`, boomTipX, boomTipY - 8);
    ctx.fillText(`Swing: ${swingAngle.toFixed(1)}°`, cx, baseY - truckH / 2 - 20);

  }, [frame]);

  return (
    <canvas
      ref={canvasRef}
      width={320}
      height={280}
      style={{ borderRadius: "6px", background: "rgba(0,0,0,0.2)" }}
    />
  );
}
