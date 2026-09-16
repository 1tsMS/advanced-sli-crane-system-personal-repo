import { useState } from "react";
import { Cpu, RefreshCw } from "lucide-react";

const API_BASE = "http://localhost:8000/api";

interface DebugEntry { bus: string; address: string | null; status: string; }
interface DebugReport { entries: DebugEntry[]; }

interface DebugTabProps {
  debugReport: DebugReport | null;
  lastAck: string | null;
}

export function DebugTab({ debugReport, lastAck }: DebugTabProps) {
  const [loading, setLoading] = useState(false);
  const [ackLog, setAckLog] = useState<string[]>([]);

  const requestScan = async () => {
    setLoading(true);
    try {
      await fetch(`${API_BASE}/debug/scan`, { method: "POST" });
    } catch {/* offline */}
    setTimeout(() => setLoading(false), 2000);
  };

  if (lastAck && !ackLog.includes(lastAck)) {
    setAckLog(prev => [lastAck, ...prev].slice(0, 50));
  }

  return (
    <div className="page-content scroll-y">
      <div style={{ display: "flex", flexDirection: "column", gap: 12, height: "100%" }}>

        {/* Scan button */}
        <div style={{ display: "flex", gap: 10, alignItems: "center" }}>
          <button className="btn-primary" onClick={requestScan} disabled={loading}
            style={{ display: "flex", alignItems: "center", gap: 8 }}>
            <RefreshCw size={13} className={loading ? "spin" : ""} />
            {loading ? "Scanning..." : "Request I2C Scan"}
          </button>
          <span style={{ fontSize: 11, color: "var(--text-muted)" }}>
            Sends DBG command to ESP32 — reports all sensor bus status
          </span>
        </div>

        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 12, flex: 1 }}>

          {/* I2C scan results */}
          <div className="glass-card" style={{ padding: "14px 16px", overflow: "auto" }}>
            <div className="section-title">
              <Cpu size={11} style={{ marginRight: 6, verticalAlign: "middle" }} />
              I2C / Sensor Scan
            </div>
            {debugReport && debugReport.entries.length > 0 ? (
              <table className="i2c-scan-table">
                <thead>
                  <tr>
                    <th>Device</th>
                    <th>Address</th>
                    <th>Status</th>
                  </tr>
                </thead>
                <tbody>
                  {debugReport.entries.map((e, i) => (
                    <tr key={i}>
                      <td style={{ color: "var(--text-secondary)" }}>{e.bus}</td>
                      <td>{e.address ?? "—"}</td>
                      <td className={e.status === "OK" ? "status-ok" : "status-fail"}>
                        {e.status}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            ) : (
              <div style={{ fontSize: 12, color: "var(--text-muted)", paddingTop: 8 }}>
                No scan data yet. Click "Request I2C Scan".
              </div>
            )}
          </div>

          {/* ACK log */}
          <div className="glass-card" style={{ padding: "14px 16px", overflow: "auto" }}>
            <div className="section-title">ACK Log</div>
            {ackLog.length > 0 ? (
              <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
                {ackLog.map((ack, i) => (
                  <div key={i} style={{
                    fontFamily: "var(--font-mono)",
                    fontSize: 11,
                    color: "var(--text-secondary)",
                    padding: "3px 0",
                    borderBottom: "1px solid var(--border)",
                  }}>
                    {ack}
                  </div>
                ))}
              </div>
            ) : (
              <div style={{ fontSize: 12, color: "var(--text-muted)" }}>
                No ACKs received yet.
              </div>
            )}
          </div>
        </div>
      </div>

      <style>{`.spin { animation: spin 1s linear infinite; } @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
    </div>
  );
}
