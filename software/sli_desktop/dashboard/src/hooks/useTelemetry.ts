import { useState, useEffect, useRef, useCallback } from "react";
import type { TelemetryFrame, DebugReport } from "../types";

const WS_URL = "ws://localhost:8000/ws/telemetry";
const RECONNECT_DELAY = 3000;

export type WsStatus = "disconnected" | "connecting" | "connected" | "error";

interface UseTelemetryReturn {
  frame: TelemetryFrame | null;
  debugReport: DebugReport | null;
  lastAck: string | null;
  wsStatus: WsStatus;
}

/**
 * useWebSocket — connects to the Python backend WebSocket and
 * delivers parsed telemetry frames, debug reports, and ACKs.
 *
 * Auto-reconnects on disconnect with a 3s delay.
 */
export function useTelemetry(): UseTelemetryReturn {
  const [frame, setFrame] = useState<TelemetryFrame | null>(null);
  const [debugReport, setDebugReport] = useState<DebugReport | null>(null);
  const [lastAck, setLastAck] = useState<string | null>(null);
  const [wsStatus, setWsStatus] = useState<WsStatus>("disconnected");

  const wsRef = useRef<WebSocket | null>(null);
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const mountedRef = useRef(true);

  const connect = useCallback(() => {
    if (!mountedRef.current) return;

    setWsStatus("connecting");
    const ws = new WebSocket(WS_URL);
    wsRef.current = ws;

    ws.onopen = () => {
      if (!mountedRef.current) return;
      setWsStatus("connected");
    };

    ws.onmessage = (event) => {
      if (!mountedRef.current) return;
      try {
        const msg = JSON.parse(event.data as string);
        if (msg.type === "telemetry") {
          setFrame(msg as TelemetryFrame);
        } else if (msg.type === "debug") {
          setDebugReport({ entries: msg.entries });
        } else if (msg.type === "ack") {
          setLastAck(msg.data as string);
        }
      } catch {
        // Ignore malformed messages
      }
    };

    ws.onerror = () => {
      if (!mountedRef.current) return;
      setWsStatus("error");
    };

    ws.onclose = () => {
      if (!mountedRef.current) return;
      setWsStatus("disconnected");
      // Auto-reconnect
      reconnectTimer.current = setTimeout(connect, RECONNECT_DELAY);
    };
  }, []);

  useEffect(() => {
    mountedRef.current = true;
    connect();

    return () => {
      mountedRef.current = false;
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
      wsRef.current?.close();
    };
  }, [connect]);

  return { frame, debugReport, lastAck, wsStatus };
}
