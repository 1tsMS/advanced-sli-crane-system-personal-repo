// ============================================================
//  Command Task — Implementation
// ============================================================
#include "command_task.h"
#include "telemetry_task.h"
#include "sensor_task.h"

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
                        // Update alarm level
                        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                            sensorData.alarmLevel = 3;  // ESTOP
                            xSemaphoreGive(sensorMutex);
                        }
                        Serial.println("$ACK,ESTOP");
                        break;
                    }

                    case CMD_MOTOR: {
                        if (cmd.motor.axis == 4) {
                            // Winch — handled directly by ESP32 DRV8833
                            if (_winch) {
                                _winch->setMotor(
                                    (uint8_t)cmd.motor.speed,
                                    cmd.motor.direction
                                );
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
                        } else {
                            // Stop stepper axis on Mega
                            if (_mega) _mega->sendMotorCommand(cmd.motor);
                        }
                        break;
                    }

                    case CMD_CALIBRATE: {
                        switch (cmd.calType) {
                            case CAL_TARE_LOAD:
                                if (_loadCell) {
                                    _loadCell->tare(10);
                                    Serial.println("$ACK,CAL0,TARE_DONE");
                                }
                                break;
                            case CAL_ZERO_IMU:
                                sensorTask_requestIMUCalibration(
                                    (uint8_t)cmd.boomAxis,
                                    (cmd.boomInv != 0),
                                    (uint8_t)cmd.tiltAxis,
                                    (cmd.tiltInv != 0)
                                );
                                Serial.println("$ACK,CAL1,IMU_ZEROED");
                                break;
                            case CAL_RESET_TELE:
                                sensorTask_resetTelescope(cmd.teleScale, cmd.teleInvert);
                                break;
                        }
                        break;
                    }

                    case CMD_DEBUG: {
                        // Send I2C scan + sensor status report
                        TelemetryFormatter fmt;
                        char dbgBuf[512];
                        uint16_t fsrVals[4] = {0};
                        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                            memcpy(fsrVals, sensorData.fsr, sizeof(fsrVals));
                            xSemaphoreGive(sensorMutex);
                        }
                        fmt.formatDebugReport(
                            _swingEnc ? _swingEnc->isConnected() : false,
                            _boomEnc  ? _boomEnc->isConnected()  : false,
                            _teleEnc  ? _teleEnc->isConnected()  : false,
                            _imu      ? _imu->isConnected()      : false,
                            _loadCell ? _loadCell->isConnected()  : false,
                            fsrVals,
                            _winch ? _winch->getCount() : 0,
                            dbgBuf, sizeof(dbgBuf)
                        );
                        Serial.print(dbgBuf);
                        break;
                    }

                    case CMD_CONFIG: {
                        telemetryTask_setRate(cmd.configRate);
                        Serial.print("$ACK,CFG,RATE,");
                        Serial.println(cmd.configRate);
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
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
