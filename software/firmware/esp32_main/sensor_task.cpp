// ============================================================
//  Sensor Task — Implementation
// ============================================================
#include "sensor_task.h"

// Local references to driver instances (set by init)
static AS5600Driver*  _swingEnc  = nullptr;
static AS5600Driver*  _boomEnc   = nullptr;
static AS5600Driver*  _teleEnc   = nullptr;
static MPU6050Driver* _imu       = nullptr;
static HX711Driver*   _loadCell  = nullptr;
static FSRReader*     _fsr       = nullptr;
static N20Encoder*    _winchEnc  = nullptr;

// Global sensor data — declared in esp32_main.ino
extern SensorData sensorData;

void sensorTask_init(
    AS5600Driver* swingEnc,
    AS5600Driver* boomEnc,
    AS5600Driver* teleEnc,
    MPU6050Driver* imu,
    HX711Driver* loadCell,
    FSRReader* fsr,
    N20Encoder* winchEnc
) {
    _swingEnc = swingEnc;
    _boomEnc  = boomEnc;
    _teleEnc  = teleEnc;
    _imu      = imu;
    _loadCell = loadCell;
    _fsr      = fsr;
    _winchEnc = winchEnc;
}

void sensorTask(void* pvParameters) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SENSOR_PERIOD_MS);

    for (;;) {
        // ---- Read all sensors ----

        // AS5600 encoders (angles in degrees)
        float swing = _swingEnc ? _swingEnc->readAngle() : 0.0f;
        float boom  = _boomEnc  ? _boomEnc->readAngle()  : 0.0f;
        float tele  = _teleEnc  ? _teleEnc->readAngle()  : 0.0f;

        // MPU6050 IMU (updates internal complementary filter)
        if (_imu) _imu->update();

        // HX711 load cell (non-blocking — returns cached if not ready)
        float weight = _loadCell ? _loadCell->getWeight() : 0.0f;

        // FSR outrigger sensors (raw ADC)
        uint16_t fsrVals[4] = {0, 0, 0, 0};
        if (_fsr) _fsr->readAll(fsrVals);

        // N20 winch encoder (rope length)
        float ropeLen = _winchEnc ? _winchEnc->getRopeLengthMM() : 0.0f;

        // ---- Convert telescope angle to linear extension ----
        // Full AS5600 rotation (360°) = TELE_MM_PER_REVOLUTION mm
        float extensionMM = (tele / 360.0f) * TELE_MM_PER_REVOLUTION;

        // ---- Write to shared struct under mutex ----
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            sensorData.swingAngle   = swing;
            sensorData.boomAngle    = boom;
            sensorData.teleAngle    = tele;
            sensorData.extensionMM  = extensionMM;
            sensorData.ropeLengthMM = ropeLen;

            sensorData.loadCellRaw  = weight;
            sensorData.actualLoadKg = weight;  // Phase 3: will add angle compensation

            sensorData.imuRoll  = _imu ? _imu->getRoll()  : 0.0f;
            sensorData.imuPitch = _imu ? _imu->getPitch() : 0.0f;

            sensorData.fsr[0] = fsrVals[0];
            sensorData.fsr[1] = fsrVals[1];
            sensorData.fsr[2] = fsrVals[2];
            sensorData.fsr[3] = fsrVals[3];

            // Phase 3: these will be computed by SafetyTask
            sensorData.safeLoadLimit = 5.0f;   // Default placeholder
            sensorData.loadPercent   = (sensorData.safeLoadLimit > 0)
                ? (sensorData.actualLoadKg / sensorData.safeLoadLimit) * 100.0f
                : 0.0f;
            sensorData.alarmLevel = 0;  // OK

            // System health check
            sensorData.sensorsOK = (
                (_swingEnc ? _swingEnc->isConnected() : true) &&
                (_boomEnc  ? _boomEnc->isConnected()  : true) &&
                (_teleEnc  ? _teleEnc->isConnected()  : true) &&
                (_imu      ? _imu->isConnected()      : true) &&
                (_loadCell ? _loadCell->isConnected()  : true)
            );

            xSemaphoreGive(sensorMutex);
        }

        // Sleep until next period — precise 100Hz timing
        vTaskDelayUntil(&lastWakeTime, period);
    }
}
