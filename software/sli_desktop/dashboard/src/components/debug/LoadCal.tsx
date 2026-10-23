import { useState } from "react";
import { Scale, Plus, Trash2, Save } from "lucide-react";
import type { DebugReport, TelemetryFrame } from "../../types";

const API_BASE = "http://localhost:8000/api";

interface LoadCalProps {
  frame: TelemetryFrame | null | undefined;
  debugReport: DebugReport | null;
  lastAck: string | null;
  onScan: () => void;
  onLog: (text: string, type: "ack" | "info" | "err") => void;
}

async function apiPost(path: string, body: object) {
  try {
    const res = await fetch(`${API_BASE}${path}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const data = await res.json().catch(() => ({}));
    return { ok: res.ok, msg: data.message ?? data.detail ?? (res.ok ? "Done" : "Failed") };
  } catch {
    return { ok: false, msg: "Backend offline" };
  }
}

/** "key:value key:value" -> record */
function kv(s: string | null | undefined): Record<string, string> {
  const out: Record<string, string> = {};
  for (const part of (s ?? "").split(/[\s,]+/)) {
    const i = part.indexOf(":");
    if (i > 0) out[part.slice(0, i)] = part.slice(i + 1);
  }
  return out;
}

/**
 * Load cell calibration with known weights.
 * 1. Tare with nothing hanging. 2. Hang a weighed object, enter its kg, Record:
 * the ESP32 averages ~2 s of readings and refits counts-per-kg over every
 * weight recorded so far (least squares through zero), then saves it.
 * 3. Repeat with 1-2 more weights; the worst error tells you how good the fit is.
 */
export function LoadCal({ frame, debugReport, lastAck, onScan, onLog }: LoadCalProps) {
  const [weightKg, setWeightKg] = useState("");
  const [manualScale, setManualScale] = useState("");
  const [busy, setBusy] = useState(false);

  const line = debugReport?.entries.find(e => e.bus === "LOAD_CAL");
  const cal = kv(line?.address);
  const espScale = cal.scale ? parseFloat(cal.scale) : null;
  const isDefault = line?.status === "DEFAULT";

  // Latest CAL7 result straight from the ESP32's ACK
  const ack = lastAck && lastAck.startsWith("$ACK,CAL7") ? lastAck : null;
  const ackKv = kv(ack);
  const measuring = !!ack && ack.includes("MEASURING");

  const live = frame?.measuredLoad ?? 0;

  const send = async (body: object, what: string) => {
    setBusy(true);
    onLog(`→ ${what}`, "info");
    const r = await apiPost("/calibrate/load", body);
    onLog(`← ${r.msg}`, r.ok ? "ack" : "err");
    setBusy(false);
    setTimeout(onScan, 2600);   // read the new scale back after the ~2 s average
  };

  const tare = async () => {
    onLog("→ Tare (nothing hanging)", "info");
    const r = await apiPost("/calibrate/tare", {});
    onLog(`← ${r.msg}`, r.ok ? "ack" : "err");
  };

  const record = () => {
    const w = parseFloat(weightKg);
    if (!Number.isFinite(w) || w <= 0 || w > 10) { onLog("⚠ Enter the weight in kg (0–10)", "err"); return; }
    send({ action: "point", weight: w }, `Record ${w} kg (hold still ~2 s)`);
  };

  const saveManual = () => {
    const s = parseFloat(manualScale);
    if (!Number.isFinite(s) || s === 0) { onLog("⚠ Scale must be a non-zero number", "err"); return; }
    send({ action: "scale", scale: s }, `Set scale ${s} counts/kg`);
  };

  return (
    <div className="cal-item" style={{ border: "1px solid var(--border-accent)" }}>
      <div className="cal-item-top">
        <div>
          <div className="cal-item-name" style={{ color: "var(--cyan)", display: "flex", alignItems: "center", gap: 6 }}>
            <Scale size={13} />
            Load Cell (2–3 known weights)
          </div>
          <div className="cal-item-sub">
            1. Empty hook → Tare. 2. Hang a weighed object, enter its kg → Record (hold still ~2 s).
            3. Repeat with 1–2 heavier weights. Each Record refits and saves the scale.
          </div>
        </div>
        <button className="btn-primary" style={{ padding: "4px 10px", fontSize: 10 }} onClick={tare}
                title="Zero the load cell with nothing hanging on the hook">
          Tare
        </button>
      </div>

      <div className="tele-cal-box">
        <div className="tele-readout-strip">
          <div>
            <div className="tele-k">Live load</div>
            <div className="tele-val-lg" style={{ color: "var(--cyan)" }}>
              {live.toFixed(3)} <span style={{ fontSize: 12, fontWeight: 500 }}>kg</span>
            </div>
          </div>
          <div style={{ textAlign: "right" }}>
            <div className="tele-k">Scale on the ESP32</div>
            <div style={{ fontFamily: "var(--font-mono)", fontSize: 11,
                          color: espScale === null ? "var(--amber)" : isDefault ? "var(--amber)" : "var(--text-secondary)" }}>
              {espScale === null ? "not loaded: Scan" : `${espScale.toFixed(1)} counts/kg${isDefault ? " (default guess)" : ""}`}
            </div>
            {cal.points && cal.points !== "0" && (
              <div style={{ fontSize: 10, color: "var(--text-muted)" }}>{cal.points} weight(s) this session</div>
            )}
          </div>
        </div>

        <div className="tele-section-card">
          <div className="tele-section-title">
            <span>Known weight</span>
            <button className="btn-ghost" style={{ padding: "1px 6px", fontSize: 9 }} disabled={busy}
                    onClick={() => send({ action: "clear" }, "Clear recorded weights")}>
              <Trash2 size={9} /> Clear points
            </button>
          </div>
          <div className="tele-input-row">
            <input type="number" step="0.001" className="tele-input" value={weightKg} placeholder="kg"
                   aria-label="Known weight in kg"
                   onChange={e => setWeightKg(e.target.value)}
                   onKeyDown={e => { if (e.key === "Enter") record(); }} />
            <span style={{ fontSize: 11, color: "var(--text-muted)" }}>kg</span>
            <button className="btn-primary" onClick={record} disabled={busy || measuring}
                    style={{ marginLeft: "auto", padding: "5px 12px" }}>
              <Plus size={10} /> {measuring ? "Measuring…" : "Record weight"}
            </button>
          </div>
          {ack && (
            <div className="cal-item-status" style={{ color: ack.includes(",ERR") ? "var(--amber)" : "var(--cyan)" }}>
              {ack.includes(",OK,")
                ? `Fitted ${ackKv.SCALE} counts/kg from ${ackKv.POINTS} weight(s) · worst error ${ackKv.WORST_ERR_G} g`
                : ack.replace("$ACK,CAL7,", "")}
            </div>
          )}
        </div>

        <div className="tele-section-card">
          <div className="tele-section-title"><span>Manual scale</span></div>
          <div className="tele-input-row">
            <input type="number" step="any" className="tele-input" value={manualScale}
                   placeholder={espScale !== null ? espScale.toFixed(1) : "counts/kg"}
                   onChange={e => setManualScale(e.target.value)} />
            <span style={{ fontSize: 11, color: "var(--text-muted)" }}>counts/kg</span>
            <button className="btn-ghost" onClick={saveManual} disabled={busy}
                    style={{ marginLeft: "auto", padding: "5px 12px" }}>
              <Save size={10} /> Save
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
