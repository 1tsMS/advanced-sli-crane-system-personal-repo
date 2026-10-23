import type { DebugReport } from "../../types";

/** Encoder axes in firmware order (CAL4/CAL5 A1..A4; A4 = rope). */
export type EncoderAxisId = "swing" | "boom" | "tele" | "rope";
/** kind: "as5600" has magnet + I2C diagnostics; "n20" is the winch's quadrature encoder (PCNT). */
export const ENCODER_AXES: { id: EncoderAxisId; tag: string; label: string; unit: string; kind: "as5600" | "n20" }[] = [
  { id: "swing", tag: "SWING", label: "Swing",            unit: "°",       kind: "as5600" },
  { id: "boom",  tag: "BOOM",  label: "Boom Lift",        unit: "°",       kind: "as5600" },
  { id: "tele",  tag: "TELE",  label: "Telescope",        unit: "mm",      kind: "as5600" },
  { id: "rope",  tag: "ROPE",  label: "Rope (winch)",     unit: "mm",      kind: "n20" },
];

export interface EncoderDiagView {
  magnet: { verdict: string; agc: number | null; magnitude: number | null } | null;
  link: { reads: number; fails: number; rejected: number; resync: number; status: string } | null;
  axis: { inverted: boolean; reference: number | null; scale: number | null; counts: number | null; source: string | null; status: string } | null;
}

/** Parse "key:value key:value" detail strings from $D lines into numbers/strings. */
function kv(detail: string | null): Record<string, string> {
  const out: Record<string, string> = {};
  for (const part of (detail ?? "").split(/\s+/)) {
    const i = part.indexOf(":");
    if (i > 0) out[part.slice(0, i)] = part.slice(i + 1);
  }
  return out;
}

const num = (v: string | undefined): number | null => {
  if (v === undefined) return null;
  const n = parseFloat(v);
  return Number.isFinite(n) ? n : null;
};

/** Pull one encoder's MAG_/ENC_/AXIS_ lines out of a DBG report. */
export function encoderDiag(report: DebugReport | null, tag: string): EncoderDiagView {
  const find = (bus: string) => report?.entries.find(e => e.bus === bus) ?? null;

  const mag = find(`MAG_${tag}`);
  const enc = find(`ENC_${tag}`);
  const axis = find(`AXIS_${tag}`);

  const m = kv(mag?.address ?? null);
  const e = kv(enc?.address ?? null);
  const a = kv(axis?.address ?? null);

  return {
    magnet: mag ? { verdict: mag.status, agc: num(m.AGC), magnitude: num(m.MAG) } : null,
    link: enc ? {
      reads: num(e.reads) ?? 0, fails: num(e.fails) ?? 0,
      rejected: num(e.rejected) ?? 0, resync: num(e.resync) ?? 0, status: enc.status,
    } : null,
    axis: axis ? {
      inverted: a.inv === "1", reference: num(a.ref), scale: num(a.scale), counts: num(a.counts),
      source: a.src ?? null, status: axis.status,
    } : null,
  };
}

/** Lines shown by the encoder panel instead of the generic hardware table. */
export function isEncoderDetailLine(bus: string): boolean {
  return bus.startsWith("MAG_") || bus.startsWith("ENC_") || bus.startsWith("AXIS_");
}
