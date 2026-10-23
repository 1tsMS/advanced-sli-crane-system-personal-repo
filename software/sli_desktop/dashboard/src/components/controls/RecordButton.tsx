import { useEffect, useState } from "react";
import type { RecordingState } from "../../types";

const API_BASE = "http://localhost:8000/api";

interface RecordButtonProps {
  recording: RecordingState & { receivedAt: number };
  espConnected: boolean;
  /** Ask the parent to re-read /api/status after a toggle. */
  onChanged: () => void;
}

function mmss(totalSec: number): string {
  const s = Math.max(0, Math.floor(totalSec));
  return `${String(Math.floor(s / 60)).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
}

/**
 * CSV recording toggle. Off by default: the backend only writes a session
 * file between Start and Stop (or until the ESP32 disconnects).
 */
export function RecordButton({ recording, espConnected, onChanged }: RecordButtonProps) {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [now, setNow] = useState(() => Date.now());

  // Tick the elapsed time every second between the 2 s status polls
  useEffect(() => {
    if (!recording.recording) return;
    const id = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(id);
  }, [recording.recording]);

  // Errors fade out on their own
  useEffect(() => {
    if (!error) return;
    const id = setTimeout(() => setError(null), 4000);
    return () => clearTimeout(id);
  }, [error]);

  const elapsed = recording.recording
    ? recording.elapsedSec + Math.max(0, now - recording.receivedAt) / 1000
    : 0;

  const toggle = async () => {
    setBusy(true);
    setError(null);
    try {
      const action = recording.recording ? "stop" : "start";
      const res = await fetch(`${API_BASE}/recording/${action}`, { method: "POST" });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        setError(typeof data.detail === "string" ? data.detail : `Could not ${action} recording`);
      }
    } catch {
      setError("Backend offline");
    } finally {
      setBusy(false);
      onChanged();
    }
  };

  const disabled = busy || (!recording.recording && !espConnected);
  const title = recording.recording
    ? `Recording to ${recording.file ?? "CSV"} (${recording.rows.toLocaleString()} rows). Click to stop.`
    : espConnected
      ? "Start recording telemetry to a CSV file"
      : "Connect to the ESP32 to record";

  return (
    <div className="rec-wrap">
      {error && <span className="rec-err" role="alert">{error}</span>}
      <button
        className={`rec-btn ${recording.recording ? "on" : ""}`}
        onClick={toggle}
        disabled={disabled}
        title={title}
        aria-pressed={recording.recording}
      >
        <span className="rec-dot" aria-hidden="true" />
        REC
        {recording.recording && <span className="rec-time">{mmss(elapsed)}</span>}
      </button>
    </div>
  );
}
