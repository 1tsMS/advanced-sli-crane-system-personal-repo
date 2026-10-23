# Sensor issues after flashing `fix/sensors-calibration-recording` (2026-10-10)

Handoff notes: analysis only, no fixes applied. Branch: `fix/sensors-calibration-recording` (uncommitted).
File references are to that branch.

---

## Common thread: something resets or disturbs the sensors

Two things explain most of the issues below:

- **A. Connecting from the dashboard probably reboots the ESP32.** pyserial opens the COM port with DTR/RTS
  asserted. On ESP32 DevKit boards (CP2102/CH340 auto-reset circuit) that pulses EN, so the ESP32 restarts
  on every *Connect*. Everything that is "lost on reboot" is then lost on every connect.
  Code: `sli_backend/core/serial_manager.py` `connect()` opens `serial.Serial(port, baud, timeout)` with
  default DTR/RTS. Confirm: watch the ESP32 boot banner (`=== Advanced SLI ESP32 Starting ===`) appear in
  the backend log right after Connect.
- **B. Electrical noise from the motor supply.** IMU bounces only with motor power on, and the telescope hangs
  only while the motor runs. With noise, I2C transfers fail. Failed transfers make the firmware stall or
  freeze values (see issue 3). Confirm: Debug → Scan before and after running motors and compare
  `ENC_* fails`, `resync` and `SENSOR_LOOP period_max`.

---

## Issue 1: Load cell reads crazy high after connecting, must tare first

**Likely cause (high confidence):** the boot tare does not happen reliably, and it is the only tare.
- `hx711_driver.cpp` `begin()`: `delay(500); _connected = is_ready(); if (_connected) tare(20);`
  The HX711 runs at 10 SPS, so DOUT is LOW only part of each 100 ms cycle. If `is_ready()` is false at that
  instant, **no tare happens**: offset stays 0 and weight = raw counts / 122000. The Sept 22 log showed raw
  ≈ 568 798, which is ≈ 4.7 kg before any load; larger raw values give "ERR >10 kg".
- The first conversions after HX711 power-up are unsettled (datasheet: ~400 ms settling), so even a boot tare
  can be off.
- Combined with **A**: every Connect reboots, so every Connect loses the tare.
- The tare offset is not saved in flash.

**Fix direction:**
1. Stop the reset on connect (issue A fix below).
2. Save the tare offset in NVS when CAL0 finishes; load it at boot (`set_offset`). Do not auto-tare at boot
   (if something hangs on the hook at power-up, a boot tare would be wrong anyway).
3. If no offset is stored: at boot use `wait_ready_timeout(1000)`, discard the first 3–5 conversions, then tare.

---

## Issue 2: Telescope extension resets to 0 on restart

**Cause (certain, by design):** `EncoderAxis` in `ENC_LINEAR` mode counts turns from power-up
(`encoder_axis.cpp` `update()`: first sample sets `_turns = 0`). Only scale/invert are saved
(`sensor_task.cpp` `saveImuAndTele()`), not the position. Made worse by **A** (every Connect = restart).

**Fix direction:**
- Persist the position: save `turns - zeroTurns` (counts from reference) and the last raw angle to NVS
  when the telescope has stopped (e.g. value unchanged for 1 s and different from the last saved value).
  That is one flash write per move, fine for NVS wear.
- At boot: `counts = saved_counts + wrapDiff(raw_now, saved_raw)`. Valid as long as the lead screw
  did not turn more than half a turn while powered off (it can't be back-driven much).
- Keep "Set Zero (Retracted)" to re-reference whenever needed.

---

## Issue 3: Telescope no longer jumps, but is laggy: stops changing for a few seconds while the motor runs, then continues

The magnet is fine (MS checked). Things in the code that can freeze the value:

1. **Encoder marked disconnected, then re-probed only once per second (most likely).**
   `as5600_driver.cpp` `readRaw()`: after 30 consecutive failed samples (`_missCount > 30`, ~0.3 s)
   `_connected = false`; then it re-probes only `if (millis() - _lastProbeMs >= 1000)`. During that time the
   value is frozen. With motor noise on the soft-I2C bus (GPIO32/33, internal ~45 kΩ pull-ups only), this
   can repeat → "stops for a few seconds, then continues".
2. **Median-of-3 needs at least 2 good reads.** `readRaw()` returns false if only 1 of 3 transfers succeeds
   (`EncoderAxis::combineReads` rejects n < 2), and the top-nibble check (`b[0] & 0xF0`) rejects more
   corrupted reads. On a noisy bus this turns into many skipped samples, which counts toward the 30 misses above.
3. **Hardware-I2C timeouts stall the whole sensor loop.** `Wire.setTimeOut(50)` in `esp32_main.ino`: each
   failed transfer on bus 0 (swing AS5600 + MPU6050) or bus 1 (boom) can take up to 50 ms, and the swing
   and boom encoders now do 3 transfers each per sample. A noisy cycle can take 100+ ms, which makes every
   reading (telescope too) lag.
4. **MPU6050 reconnect blocks.** `mpu6050_driver.cpp` `update()`: if `getEvent` fails once, `_connected = false`
   and `begin()` (Adafruit init with resets/delays, ~100+ ms) is retried every 2.5 s inside SensorTask.
5. Glitch gate / resync (`EncoderAxis::update`): unlikely to cause seconds of freeze (limit is 45° per 10 ms;
   resync after 5 rejections = ~50 ms), but each resync loses counts.

**How to confirm:** Debug → Scan right after a run where it hung. Look at:
`ENC_TELE fails/reads` (I2C error %), `resync`, `SENSOR_LOOP period_max` (should be ~10 ms),
and whether `I2C2_TELE` flips to FAIL. Repeat with motor power off for comparison.

**Fix direction (software):**
- Don't drop to "disconnected" so fast, and re-probe every ~100 ms, not 1 s.
- Accept a single good read if it is within the rate gate of the last value (instead of requiring 2 of 3).
- Lower `Wire.setTimeOut` to ~5 ms (an AS5600 read takes < 1 ms at 100 kHz).
- Rate-limit / make non-blocking the MPU reconnect.
- Log the per-cycle work time and failures so the lag can be seen in DBG.

**Fix direction (hardware, probably the real root):**
- External pull-ups on the soft-I2C lines: 2.2–4.7 kΩ to 3.3 V on GPIO32/33 (internal ~45 kΩ is too weak
  for long wires), also check bus 0/1 modules have pull-ups.
- Twist SDA and SCL each with GND, keep them away from the stepper wires, shorten the harness.
- 100 nF + 10 µF at each AS5600 / MPU6050 board.
- Longer term: a TCA9548A I2C multiplexer puts all three AS5600 on one hardware I2C bus (no bit-banging),
  or read the AS5600 OUT pin (PWM / analog) for the telescope instead of I2C.

---

## Issue 4: Turntable (swing) starts at a random value, and ~90° of real rotation reads only 10.9°

**Likely cause (high confidence): the swing encoder is NOT on the turntable shaft 1:1.**
The firmware treats swing as `ENC_ROTARY` with `SWING_DEG_PER_REV = 360` (`config.h`), i.e. one encoder turn =
one turntable turn, wrapped to ±180°. If the magnet is on the motor / worm / pinion shaft, the encoder turns
many times per turntable turn. Then:
- 90° of turntable = several encoder turns; the reading is just the leftover fraction (here 10.9°).
- Absolute angle at power-up means nothing → "random start value".
(CAD has `worm.stl`, so a worm/gear drive is likely. The original status report also said the telescope encoder
reading had to be scaled, 19.048 mm per encoder turn.)

**Same question for the boom:** `BOOM_DEG_PER_REV = 360` and "boom angle from encoder" both assume the boom
AS5600 is on the pivot 1:1 (Ali's assumption). **If the boom encoder is also on a worm/motor shaft, the boom
angle from the encoder will be wrong in the same way.** Verify before relying on it.

**Fix direction:**
- MS to confirm where each magnet sits and the gear ratio (turntable and boom).
- If geared: use multi-turn mode (`ENC_LINEAR`-style counting) for that axis with
  scale = axis degrees per encoder turn (= 360 / gear ratio), plus position persistence like issue 2.
- Add a ratio calibration like the telescope ruler tool: set zero, rotate the turntable a known angle
  (90° / 180° with a protractor), record encoder turns, compute degrees per turn.
- Lost counts from issue 3 (noise) would also make it inaccurate, so fix the bus too.

---

## Issue 5: IMU boom angle bounces when the motor power supply is on (stable when off, even when moving cables)

**Likely cause:** the stepper drivers, not the cables.
- With 12 V on, the A4988s are enabled and hold current even when idle (`mega_executor.ino` sets EN LOW at
  setup). That gives (a) mechanical vibration/"singing" from microstep chopping, which the accelerometer picks
  up, and (b) electrical noise on shared ground / 5 V and on the I2C lines.
- The complementary filter trusts the accelerometer 2% per sample at 100 Hz (`ALPHA = 0.98`), and the MPU DLPF
  is 21 Hz, so vibration shows up directly as angle noise.

**How to tell vibration from electrical noise:**
1. Motor power on, drivers disabled (send `T0` so the Mega sets EN high, or pull EN): if the bounce stops → it's
   the drivers.
2. Hold the boom / IMU firmly with a hand with power on: if the bounce drops → mechanical vibration.
3. Power the ESP32 from a battery/laptop on battery (no USB ground to the PC): if it improves → ground loop.

**Fix direction:**
- Software: set MPU DLPF to 5–10 Hz, raise ALPHA to ~0.99–0.995, optionally a short moving average.
  Now that the boom angle can come from the encoder, the IMU is only the cross-check, so heavier filtering is OK.
- Hardware: lower A4988 Vref (holding current), star ground at the supply, 100 µF + 100 nF across 12 V at the
  RAMPS, decoupling at the MPU board, keep I2C wires away from motor wires.
- Optional: disable steppers when idle (EN high) **only if** the worm gears hold the load without torque.

---

## Issue A fix: stop the ESP32 resetting on Connect

In `serial_manager.py` open the port without toggling DTR/RTS:

```python
ser = serial.Serial()
ser.port, ser.baudrate, ser.timeout = port, baud, 0.05
ser.dtr = False
ser.rts = False
ser.open()
```

Note: some USB-serial drivers on Windows still pulse once on open. If so, keep the persistence fixes
(tare offset, telescope position) so a reset no longer loses anything.

---

## Priority for the fix

1. Issue A (no reset on connect) + save tare offset → fixes issue 1.
2. Confirm gear ratios for swing and boom encoders (MS) → then multi-turn + ratio calibration for swing (issue 4).
3. Telescope position persistence (issue 2).
4. I2C robustness: pull-ups/wiring (hardware) + faster re-probe, single-read acceptance, `Wire.setTimeOut(5)` (issue 3).
5. IMU filtering + driver noise checks (issue 5).

## Questions for MS before fixing
- Where exactly is each AS5600 magnet: turntable shaft or motor/worm shaft? Gear ratio of swing and boom drives?
- Do the boom and turntable hold position with stepper drivers disabled (worm self-locking)?
- Are there external pull-up resistors on the three I2C buses?
- Is the ESP32 GND tied to the 12 V / RAMPS GND, and is the ESP32 powered from the PC USB?
