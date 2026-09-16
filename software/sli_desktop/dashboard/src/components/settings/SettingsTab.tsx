import { useState, useEffect } from "react";
import { Wifi, WifiOff, PlugZap, Unplug } from "lucide-react";

const API_BASE = "http://localhost:8000/api";

interface SettingsTabProps {
  wsConnected: boolean;
}

interface PortEntry { port: string; description: string; }

export function SettingsTab({ wsConnected }: SettingsTabProps) {
  const [ports, setPorts] = useState<PortEntry[]>([]);
  const [selectedPort, setSelectedPort] = useState("");
  const [baud, setBaud] = useState(115200);
  const [status, setStatus] = useState<{ connected: boolean; port?: string; rxRate?: number } | null>(null);
  const [message, setMessage] = useState("");
  const [loading, setLoading] = useState(false);

  const loadPorts = async () => {
    try {
      const res = await fetch(`${API_BASE}/ports`);
      const data = await res.json();
      setPorts(data.ports ?? []);
    } catch {
      setPorts([]);
    }
  };

  const loadStatus = async () => {
    try {
      const res = await fetch(`${API_BASE}/status`);
      const data = await res.json();
      setStatus(data);
    } catch {
      setStatus(null);
    }
  };

  useEffect(() => {
    loadPorts();
    loadStatus();
    const interval = setInterval(loadStatus, 2000);
    return () => clearInterval(interval);
  }, []);

  const connect = async () => {
    if (!selectedPort) { setMessage("Select a port first"); return; }
    setLoading(true);
    setMessage("");
    try {
      const res = await fetch(`${API_BASE}/connect`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ port: selectedPort, baudRate: baud }),
      });
      const data = await res.json();
      setMessage(data.message ?? (res.ok ? "Connected!" : "Failed"));
      await loadStatus();
    } catch (e) {
      setMessage("Backend unreachable");
    } finally {
      setLoading(false);
    }
  };

  const disconnect = async () => {
    setLoading(true);
    try {
      await fetch(`${API_BASE}/disconnect`, { method: "POST" });
      setMessage("Disconnected");
      await loadStatus();
    } catch {
      setMessage("Error");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="page-content scroll-y">
      <div style={{ maxWidth: 500, display: "flex", flexDirection: "column", gap: 16 }}>

        {/* Backend + WS Status */}
        <div className="glass-card" style={{ padding: "14px 16px" }}>
          <div className="section-title">Connection Status</div>
          <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
            <StatusRow
              icon={wsConnected ? <Wifi size={13} /> : <WifiOff size={13} />}
              label="WebSocket"
              value={wsConnected ? "Connected" : "Disconnected"}
              ok={wsConnected}
            />
            <StatusRow
              icon={status?.connected ? <PlugZap size={13} /> : <Unplug size={13} />}
              label="ESP32 Serial"
              value={
                status?.connected
                  ? `${status.port} @ ${status.rxRate?.toFixed(1)} pkt/s`
                  : "Not connected"
              }
              ok={!!status?.connected}
            />
          </div>
        </div>

        {/* Port selection */}
        <div className="glass-card" style={{ padding: "14px 16px" }}>
          <div className="section-title">Serial Connection</div>
          <div className="settings-form">
            <div className="form-group">
              <label className="form-label">Port</label>
              <div style={{ display: "flex", gap: 8 }}>
                <select
                  className="form-select"
                  style={{ flex: 1 }}
                  value={selectedPort}
                  onChange={e => setSelectedPort(e.target.value)}
                >
                  <option value="">-- Select port --</option>
                  {ports.map(p => (
                    <option key={p.port} value={p.port}>
                      {p.port} — {p.description}
                    </option>
                  ))}
                </select>
                <button
                  className="btn-primary"
                  style={{ padding: "8px 12px", fontSize: 11 }}
                  onClick={loadPorts}
                >
                  Refresh
                </button>
              </div>
            </div>

            <div className="form-group">
              <label className="form-label">Baud Rate</label>
              <select className="form-select" value={baud} onChange={e => setBaud(Number(e.target.value))}>
                <option value={9600}>9600</option>
                <option value={115200}>115200</option>
                <option value={230400}>230400</option>
              </select>
            </div>

            <div style={{ display: "flex", gap: 8 }}>
              <button className="btn-primary" style={{ flex: 1 }} onClick={connect} disabled={loading}>
                {loading ? "..." : "Connect"}
              </button>
              <button className="btn-danger" style={{ flex: 1 }} onClick={disconnect} disabled={loading}>
                Disconnect
              </button>
            </div>

            {message && (
              <div style={{ fontSize: 11, color: "var(--text-secondary)", paddingTop: 4 }}>
                {message}
              </div>
            )}
          </div>
        </div>

        {/* Backend info */}
        <div className="glass-card" style={{ padding: "14px 16px" }}>
          <div className="section-title">Backend</div>
          <div style={{ fontSize: 11, color: "var(--text-secondary)", lineHeight: 1.8, fontFamily: "var(--font-mono)" }}>
            <div>REST API: http://localhost:8000/api</div>
            <div>WebSocket: ws://localhost:8000/ws/telemetry</div>
            <div>Docs: http://localhost:8000/docs</div>
          </div>
        </div>

      </div>
    </div>
  );
}

function StatusRow({
  icon, label, value, ok,
}: {
  icon: React.ReactNode; label: string; value: string; ok: boolean;
}) {
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, fontSize: 12 }}>
      <span style={{ color: ok ? "var(--green)" : "var(--text-muted)" }}>{icon}</span>
      <span style={{ color: "var(--text-secondary)", minWidth: 90 }}>{label}</span>
      <span style={{
        color: ok ? "var(--green)" : "var(--text-secondary)",
        fontFamily: "var(--font-mono)",
        fontSize: 11,
      }}>
        {value}
      </span>
    </div>
  );
}
