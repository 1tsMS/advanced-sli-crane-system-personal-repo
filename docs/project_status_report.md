# Advanced SLI — Project Status Report
**Date:** September 2026 | **Ref:** [software_design_plan.md](./software_design_plan.md)

---

## Build Phase Status

| Phase | Description | Status |
|---|---|---|
| **Phase 1** | It Talks — end-to-end data pipeline | ✅ Complete |
| **Phase 2** | It Looks Good — full UI + controls | ✅ Complete |
| **Phase 3** | It's Smart — load calculation & safety | 🔴 Pending |
| **Phase 4** | It Prevents Disaster — PID anti-sway | 🔴 Not Started |

---

## What's Done

### Layer 1 — ESP32 Firmware (FreeRTOS)
- **3-task FreeRTOS skeleton** running: `SensorTask` (100Hz, Core 1), `TelemetryTask` (50Hz, Core 0), `CommandTask` (event-driven, Core 0)
- **`$T` telemetry packet** streaming at 50Hz over USB serial — all 16 fields present and parsed
- **Custom G-code protocol** implemented — `M1–M4`, `M0 Ax`, `T0` E-stop, `CAL0–2`, `DBG`, `LC`
- **UART2 bridge** to Arduino Mega working (`GPIO23/27`)
- **All sensor drivers written and functional:**
  - `AS5600Driver` — supports all 3 buses (Wire, Wire1, SoftI2C)
  - `MPU6050Driver` — backed by `Adafruit_MPU6050`, complementary filter for roll/pitch
  - `HX711Driver` — load cell with tare and calibration factor
  - `FSRReader` — 4-channel ADC for outrigger sensors
  - `N20Encoder` — quadrature interrupt-based encoder + DRV8833 motor control
- **NVS Flash persistence** — telescope scale (`tele_scale`) and invert (`tele_inv`) survive power cycles via `Preferences`
- **Calibration commands extended** — `CAL2 S<scale> I<invert>` to tune telescope parameters at runtime without reflashing

### Layer 2 — Python Backend
- **FastAPI + pyserial** running at `localhost:8000`
- **WebSocket** telemetry broadcast at 50Hz to frontend
- **All REST endpoints** live: `/api/connect`, `/api/command`, `/api/status`, `/api/calibrate/tare|imu|tele`, `/api/estop`, `/api/debug/scan`, `/api/loadchart/*`
- **CSV session logger** — auto-logs each session with timestamp
- **Command router** with regex validation — rejects invalid commands before they hit serial
- **`TeleCalibrateRequest` model** — `CAL2 S... I...` built and sent from `/api/calibrate/tele`

### Layer 3 — Frontend Dashboard
- **Dark industrial UI** — Inter + JetBrains Mono, electric cyan accents (`#00D4FF`)
- **Sidebar navigation** — Dashboard, Debug & Calibrate, Settings, Data Logger, Load Chart (placeholder)
- **Dashboard tab** — Live telemetry panels, 3× circular arc gauges (Load%, Boom Angle, Tilt), 2D crane visualizer, outrigger FSR corner view
- **Motor Controls** — Hold-to-move buttons for all 4 axes (M1–M4), global speed slider (250–2500 steps/s, center=1000), E-stop header button
- **Xbox Gamepad API** — full axis mapping (Left Stick=Swing/Boom, Right Stick=Extend, Triggers=Winch)
- **Debug & Calibrate tab:**
  - Hardware status (I2C scan)
  - Load cell tare, IMU zero with axis mapping UI
  - **Telescope calibration panel** — live encoder readout, M3 motor jog (press-and-hold), ruler auto-calibration tool (computes scale from measured travel), manual scale/invert inputs, Save to Flash button
- **Settings tab** — COM port selection, baud, connect/disconnect
- **2D Crane Visualizer** — base boom (225mm constant) + telescoping extension stage with graduation marks + `+XX mm` badge

### Arduino Mega Executor
- Motor commands received on `Serial1` from ESP32
- 3 axes on RAMPS 1.4: **Swing → X**, **Boom Lift → Z**, **Telescope Extend → Y**
- Default speed: 1000 steps/s, range 250–2500 steps/s
- E-stop (`T0`) disables all drivers

---

## Libraries: Custom vs Standard

### Why Custom Drivers

| Component | Why not standard |
|---|---|
| `AS5600Driver` | No library supports 3 AS5600s (all fixed at `0x36`) on 3 separate buses. Custom driver takes `Wire`, `Wire1`, or `SoftI2C` as constructor arg. Essential for multi-encoder setup. |
| `SoftI2C` | ESP32 has only 2 hardware I2C peripherals. 3rd bus (telescope AS5600, GPIO32/33) needs bit-bang. AVR-targeted `SoftwareWire` won't work. Custom impl mirrors `TwoWire` API, tuned at 8µs bit delay (~60kHz) for capacitive wire lengths. |
| `HX711Driver` | Standard `HX711` library uses blocking `delay()`, which starves FreeRTOS tasks. Custom driver is non-blocking — returns cached value if data not ready, keeping `SensorTask` on its 100Hz deadline. |

### Standard Libraries Used

| Library | Layer | Role |
|---|---|---|
| `Adafruit_MPU6050` + `Adafruit_Sensor` | ESP32 | IMU hardware abstraction |
| `Preferences` (ESP-IDF built-in) | ESP32 | NVS flash key-value for calibration |
| `FreeRTOS` (bundled with ESP32 Arduino) | ESP32 | Task scheduling, mutexes, queues |
| `FastAPI` + `uvicorn` | Python | ASGI REST + WebSocket server |
| `pyserial` | Python | USB serial to ESP32 |
| `React 19` + `Vite 8` | Frontend | UI framework and build tooling |
| `Lucide React` | Frontend | SVG icon library |
| `Recharts` | Frontend | Live rolling charts |
| `Gamepad API` (W3C browser API) | Frontend | Xbox controller input |

---

## Current Issues

### 🔴 Telescope Extension — Encoder Jumpiness
- AS5600 on SoftI2C (GPIO32/33) produces spike deltas at the 0°/360° wrap boundary — 30mm physical travel was reading as 110mm.
- **Software mitigations applied:** 45° glitch gatekeeper (single-frame delta > 45° discarded), shortest-path angular accumulation, switched to `REG_ANGLE` (0x0E) for AS5600 internal hardware filter, SoftI2C bit delay increased to 8µs.
- **Current state:** Improved but still jumpy under vibration. Runtime calibration (ruler tool in Debug tab) works but is sensitive to slippage.
- **Root causes:** Long SoftI2C wires (capacitance), inconsistent magnet air-gap, AS5600 has no quadrature redundancy unlike hall-effect encoders.
- **Mechanical fix needed:** Tighten motor-to-stage coupling, reduce magnet-to-IC gap, shorten SoftI2C wiring harness.

### 🔴 N20 Winch — One Direction Not Working
- Motor only runs in reverse. Forward direction (via DRV8833 AIN1/AIN2 on GPIO13/14) produces no motion.
- **Suspected cause:** DRV8833 IC fault — one H-bridge half likely dead.
- **Action:** Replace DRV8833 module. Reverify AIN1/AIN2 polarity against `n20_test` sketch after replacement.
- **Rope length measurement:** Encoder interrupt code (GPIO18/19) is written but `N20_TICKS_PER_REV=840` and `ROPE_DRUM_DIAMETER_MM=20mm` are guesses — require physical measurement post-repair.

### 🟡 Load Cell — Not Properly Calibrated
- `HX711_DEFAULT_SCALE = 122000.0` counts/kg is a rough estimate from `loadcell_approx` test.
- No 2-point calibration with known reference weights done on final mechanical setup.
- Boom-angle compensation (`W_actual = F_measured / cos(θ)`) is planned for Phase 3 — currently `actualLoadKg` = raw reading, no compensation.

### 🟡 IMU / MPU6050 — Not Calibrated on Final Hardware
- Complementary filter active, gyro zero-offset calibration function exists (`calibrateGyro()`).
- Axis mapping UI in Debug tab implemented but proper static calibration not performed on final assembled crane.

### 🟡 Hardware — Outriggers & Hook/Rope Incomplete
- Outriggers: 3D printed and assembled. FSR sensors load. Minor adjustment needed — not all 4 corners show equal distribution under a centred load.
- Hook: Not yet designed or fabricated.
- Rope: No rope/drum assembly. N20 drum diameter unverified.

---

## Remaining Work (Priority Order)

1. Replace DRV8833 → verify N20 bidirectional operation
2. Measure actual N20 encoder ticks/rev and drum diameter → update `config.h`
3. Design and fabricate hook + rope assembly, mount on boom tip + drum
4. Proper 2-point load cell calibration → update or make `HX711_DEFAULT_SCALE` runtime-tunable
5. IMU static calibration on level assembled crane → run `CAL1`
6. Mechanical telescope fix (coupling, air-gap, wiring) → re-evaluate encoder noise
7. Outrigger FSR equalisation
8. **Phase 3 firmware:** `SafetyTask`, boom-angle load compensation, load chart lookup
9. Load Chart editor tab — upload LC entries from dashboard
10. Phase 4 — PID anti-sway (lower priority)

---

## Not Started (From Plan)

- `SafetyTask` — load chart lookup, alarm escalation
- Boom-angle compensation activated (code exists, not wired in)
- Load Chart editor UI tab
- Data Logger tab (session CSV viewer + rolling chart)
- PID anti-sway task
- 3D crane visualizer (future scope)
