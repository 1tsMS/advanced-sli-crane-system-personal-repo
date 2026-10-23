// ============================================================
//  Command Task — Implementation
// ============================================================
#include "command_task.h"
#include "telemetry_task.h"
#include "sensor_task.h"
#include "serial_out.h"

// Subsystem references (set by init)
static MegaBridge*   _mega      = nullptr;
static N20Encoder*   _winch     = nullptr;
static HX711Driver*  _loadCell  = nullptr;
static MPU6050Driver* _imu      = nullptr;
static AS5600Driver* _teleEnc   = nullptr;
static AS5600Driver* _swingEnc  = nullptr;
static AS5600Driver* _boomEnc   = nullptr;

// Global sensor data — declared in esp32_main.ino
extern SensorData sensorData;

/** Map a G-code speed (0..MOTOR_SPEED_MAX) onto the winch's 0..255 PWM range.
 *  (A plain (uint8_t) cast wraps: S1000 became 232 and S256 became 0.) */
static inline uint8_t speedToPwm(uint16_t speed) {
    if (speed > MOTOR_SPEED_MAX) speed = MOTOR_SPEED_MAX;
    return (uint8_t)(((uint32_t)speed * 255UL) / MOTOR_SPEED_MAX);
}

/** Hand a calibration step to SensorTask (core 1, owner of the buses). */
static void queueCal(CalCommand type, uint8_t axis, float value, int8_t flag) {
    CalRequest req;
    req.type  = type;
    req.axis  = axis;
    req.value = value;
    req.flag  = flag;
    if (!sensorTask_requestCal(req)) serialLine("$ACK,CAL%u,ERR,BUSY", (unsigned)type);
}

// Winch dead-man: the dashboard repeats a held M4 command every 100 ms.
// If nothing arrives for MOTION_TIMEOUT_MS the winch is stopped (link lost,
// browser frozen, button release missed). The Mega does the same for M1-M3.
static bool          _winchActive    = false;
static unsigned long _lastWinchCmdMs = 0;

void commandTask_init(
    MegaBridge* mega,
    N20Encoder* winch,
    HX711Driver* loadCell,
    MPU6050Driver* imu,
    AS5600Driver* teleEnc,
    AS5600Driver* swingEnc,
    AS5600Driver* boomEnc
) {
    _mega     = mega;
    _winch    = winch;
    _loadCell = loadCell;
    _imu      = imu;
    _teleEnc  = teleEnc;
    _swingEnc = swingEnc;
    _boomEnc  = boomEnc;
}

void commandTask(void* pvParameters) {
    GCodeParser parser;
    char lineBuffer[128];
    int lineIdx = 0;

    for (;;) {
        // Read characters from USB Serial until we get a full line
        while (Serial.available()) {
            char c = Serial.read();

            if (c == '\n' || c == '\r') {
                if (lineIdx == 0) continue;  // Skip empty lines

                lineBuffer[lineIdx] = '\0';  // Null-terminate
                lineIdx = 0;

                // Parse the G-code line
                ParsedCommand cmd;
                ParseResult result = parser.parse(lineBuffer, &cmd);

                if (result != PARSE_OK) continue;

                // Dispatch based on command type
                switch (cmd.type) {

                    case CMD_ESTOP: {
                        // Emergency stop ALL motors
                        if (_mega)  _mega->sendMotorCommand(cmd.motor);
                        if (_winch) _winch->stopMotor();
                        _winchActive = false;
                        // Update alarm level
                        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                            sensorData.alarmLevel = 3;  // ESTOP
                            xSemaphoreGive(sensorMutex);
                        }
                        serialLine("$ACK,ESTOP");
                        break;
                    }

                    case CMD_MOTOR: {
                        if (cmd.motor.axis == 4) {
                            // Winch — handled directly by ESP32 DRV8833
                            if (_winch) {
                                const uint8_t pwm = speedToPwm(cmd.motor.speed);
                                _winch->setMotor(pwm, cmd.motor.direction);
                                _winchActive    = (pwm > 0);
                                _lastWinchCmdMs = millis();
                            }
                        } else {
                            // Axes 1-3 (Swing, Lift, Tele) → forward to Mega
                            if (_mega) _mega->sendMotorCommand(cmd.motor);
                        }
                        break;
                    }

                    case CMD_STOP_AXIS: {
                        if (cmd.motor.stopAxis == 4) {
                            // Stop winch locally
                            if (_winch) _winch->stopMotor();
                            _winchActive = false;
                        } else {
                            // Stop stepper axis on Mega
                            if (_mega) _mega->sendMotorCommand(cmd.motor);
                        }
                        break;
                    }

                    case CMD_CALIBRATE: {
                        switch (cmd.calType) {
                            // Everything that touches a sensor runs on core 1 (SensorTask),
                            // which sends the $ACK when done. The command core never
                            // blocks here, so a T0 is never stuck behind a calibration.
                            case CAL_TARE_LOAD:
                                queueCal(CAL_TARE_LOAD, 0, 0.0f, -1);
                                break;
                            case CAL_ZERO_IMU:
                                sensorTask_requestIMUCalibration(
                                    (uint8_t)cmd.boomAxis,
                                    (cmd.boomInv != 0),
                                    (uint8_t)cmd.tiltAxis,
                                    (cmd.tiltInv != 0)
                                );
                                break;
                            case CAL_RESET_TELE:
                                queueCal(CAL_RESET_TELE, ENC_TELE, cmd.teleScale, cmd.teleInvert);
                                break;
                            case CAL_ENC_REF:
                                queueCal(CAL_ENC_REF, cmd.encAxis, cmd.encValue, -1);
                                break;
                            case CAL_ENC_INVERT:
                                queueCal(CAL_ENC_INVERT, cmd.encAxis, 0.0f, cmd.encInvert);
                                break;
                            case CAL_ROPE_SCALE:
                                queueCal(CAL_ROPE_SCALE, CAL_AXIS_ROPE, cmd.encValue, -1);
                                break;
                        }
                        break;
                    }

                    case CMD_DEBUG: {
                        // Send I2C scan + sensor status report
                        // Only cached flags and SensorTask's diagnostics are used here:
                        // probing the buses from this core would collide with SensorTask.
                        TelemetryFormatter fmt;
                        char dbgBuf[1536];
                        uint16_t fsrVals[4] = {0};
                        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                            memcpy(fsrVals, sensorData.fsr, sizeof(fsrVals));
                            xSemaphoreGive(sensorMutex);
                        }
                        SensorDiag diag;
                        sensorTask_getDiag(&diag, true);
                        fmt.formatDebugReport(
                            _imu      ? _imu->isConnected()      : false,
                            _loadCell ? _loadCell->isConnected()  : false,
                            fsrVals,
                            _winch ? _winch->getCount() : 0,
                            diag,
                            dbgBuf, sizeof(dbgBuf)
                        );
                        Serial.print(dbgBuf);
                        break;
                    }

                    case CMD_CONFIG: {
                        telemetryTask_setRate(cmd.configRate);
                        serialLine("$ACK,CFG,RATE,%u", (unsigned)cmd.configRate);
                        break;
                    }

                    default:
                        break;
                }
            } else {
                // Buffer the character (prevent overflow)
                if (lineIdx < (int)(sizeof(lineBuffer) - 1)) {
                    lineBuffer[lineIdx++] = c;
                }
            }
        }

        // Yield to other tasks when no serial data
        if (_winchActive && (millis() - _lastWinchCmdMs) > MOTION_TIMEOUT_MS) {
            if (_winch) _winch->stopMotor();
            _winchActive = false;
            serialLine("$ACK,WATCHDOG,WINCH_STOPPED");
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
