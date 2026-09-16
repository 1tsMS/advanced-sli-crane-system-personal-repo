export interface TelemetryFrame {
  boomAngle: number;
  extensionMM: number;
  measuredLoad: number;
  actualLoad: number;
  swingAngle: number;
  ropeLength: number;
  fsr: [number, number, number, number];
  imuRoll: number;
  imuPitch: number;
  safeLoadLimit: number;
  loadPercent: number;
  alarmLevel: 0 | 1 | 2 | 3; // 0=OK, 1=WARN, 2=CRITICAL, 3=ESTOP
  timestamp: number;
}

export interface DebugEntry {
  bus: string;
  address: string | null;
  status: string;
}

export interface DebugReport {
  entries: DebugEntry[];
}

export type AlarmLevel = 0 | 1 | 2 | 3;

export type TabId =
  | "dashboard"
  | "debug"
  | "datalogger"
  | "loadchart"
  | "settings";
