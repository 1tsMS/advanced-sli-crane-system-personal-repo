# Advanced SLI — Protoboard Connections

## Architecture

ESP32 = sensor acquisition + safety/control brain
Mega 2560 + RAMPS 1.4 = stepper motor executor (3 axes only)
N20 winch (DRV8833) = driven directly by ESP32, not RAMPS

---

## ESP32 (30-pin) — Sensor + Control Brain

### I2C — 3 separate buses (AS5600 all share address 0x36, cannot share a bus)

| Bus | Type | SDA | SCL | Device(s) |
|---|---|---|---|---|
| I2C Bus 0 | Hardware (`Wire`) | GPIO21 | GPIO22 | AS5600 — Swing |
| I2C Bus 1 | Hardware (`Wire1`) | GPIO25 | GPIO26 | AS5600 — Boom Lift |
| I2C Bus 2 | Software/bit-banged | GPIO32 | GPIO33 | AS5600 — Telescope + MPU6050 (top of boom, shares bus — different address 0x68, no conflict) |

### Analog — Outrigger Force Sensors (FSR RP-C18.3-ST)

| Signal | GPIO | Notes |
|---|---|---|
| FSR 1 | GPIO34 | ADC1, input-only pin |
| FSR 2 | GPIO35 | ADC1, input-only pin |
| FSR 3 | GPIO36 | ADC1, input-only pin |
| FSR 4 | GPIO39 | ADC1, input-only pin |

### Load Cell (Top/Hook) — HX711

| Signal | GPIO |
|---|---|
| DT | GPIO4 |
| SCK | GPIO27 |

### Winch — N20 (encoder-integrated) via DRV8833

| Signal | GPIO | Notes |
|---|---|---|
| DRV8833 AIN1 | GPIO13 | Direction/PWM |
| DRV8833 AIN2 | GPIO14 | Direction/PWM |
| DRV8833 STBY | GPIO12 (or tie to 3.3V if always-enabled) | Confirm before final wiring |
| N20 Encoder A | GPIO18 | Interrupt-capable |
| N20 Encoder B | GPIO19 | Interrupt-capable |

### Communication

| Link | ESP32 Pin | Connects To |
|---|---|---|
| UART2 TX | GPIO17 | Mega RX1 (Pin 19) |
| UART2 RX | GPIO16 | Mega TX1 (Pin 18) |
| UART0 (USB) | Built-in | PC (telemetry to PyQt dashboard) |

### Power

- 5V / 3.3V and GND common rail for all sensors (check each sensor's rated voltage individually — AS5600 and MPU6050 breakouts are typically 3.3-5V tolerant, FSR voltage divider reference should match ADC reference)

---

## Mega 2560 + RAMPS 1.4 — Motor Executor (3 stepper axes)

| Axis | RAMPS Slot | Driver | Motor |
|---|---|---|---|
| X | X | A4988 | Swing — NEMA17 |
| Y | Y | A4988 | Telescope — NEMA17 |
| Z | Z | A4988 | Boom Lift — NEMA17 |
| E0 | — | Unused | (winch moved to ESP32, not RAMPS) |

| Signal | Mega Pin | Connects To |
|---|---|---|
| Serial1 RX1 | Pin 19 | ESP32 GPIO17 (TX2) |
| Serial1 TX1 | Pin 18 | ESP32 GPIO16 (RX2) |
| Power In | RAMPS power terminal | 12V PSU |

---

controlled standby
- FSR voltage divider resistor values — to be set once sensors arrive and calibration begins
