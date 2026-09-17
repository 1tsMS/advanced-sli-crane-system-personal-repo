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

#include <Preferences.h>

static Preferences _prefs;
static float   _boomAngleOffset = 0.0f;
static float   _tiltAngleOffset = 0.0f;
static uint8_t _boomSource      = 0;     // 0 = Roll (X) — physical MPU placement default
static uint8_t _tiltSource      = 1;     // 1 = Pitch (Y) — independent from boom
static bool    _boomInvert      = false;
static bool    _tiltInvert      = false;

static volatile bool    _reqImuCalibration = false;
static volatile uint8_t _reqBoomSrc = 0;
static volatile uint8_t _reqTiltSrc = 1;
static volatile bool    _reqBoomInv = false;
static volatile bool    _reqTiltInv = false;

void sensorTask_saveCalibration() {
    _prefs.begin("sli_imu", false);
    _prefs.putFloat("b_off", _boomAngleOffset);
    _prefs.putFloat("t_off", _tiltAngleOffset);
    _prefs.putUChar("b_src", _boomSource);
    _prefs.putUChar("t_src", _tiltSource);
    _prefs.putBool("b_inv",  _boomInvert);
    _prefs.putBool("t_inv",  _tiltInvert);
    _prefs.end();
}

void sensorTask_loadCalibration() {
    _prefs.begin("sli_imu", true);
    _boomAngleOffset = _prefs.getFloat("b_off", 0.0f);
    _tiltAngleOffset = _prefs.getFloat("t_off", 0.0f);
    _boomSource      = _prefs.getUChar("b_src", 0);   // default: Roll (X)
    _tiltSource      = _prefs.getUChar("t_src", 1);   // default: Pitch (Y)
    _boomInvert      = _prefs.getBool("b_inv", false);
    _tiltInvert      = _prefs.getBool("t_inv", false);
    _prefs.end();
}

void sensorTask_zeroIMU() {
    if (_imu && _imu->isConnected()) {
        float r = _imu->getRoll();
        float p = _imu->getPitch();
        // Use independently configured source axes for zeroing
        _boomAngleOffset = (_boomSource == 1) ? p : r;
        _tiltAngleOffset = (_tiltSource == 1) ? p : r;
        sensorTask_saveCalibration();
    }
}

void sensorTask_requestIMUCalibration(uint8_t boomSrc, bool boomInv, uint8_t tiltSrc, bool tiltInv) {
    _reqBoomSrc = boomSrc;
    _reqBoomInv = boomInv;
    _reqTiltSrc = tiltSrc;
    _reqTiltInv = tiltInv;
    _reqImuCalibration = true;
}

void sensorTask(void* pvParameters) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(SENSOR_PERIOD_MS);

    for (;;) {
        // Process pending IMU calibration strictly on Core 1
        // This prevents I2C bus collision with CommandTask on Core 0
        if (_reqImuCalibration) {
            _reqImuCalibration = false;
            Serial.println("$ACK,DBG,Executing IMU Cal on Core 1...");
            if (_imu && _imu->isConnected()) {
                _imu->calibrateGyro(100);
                Wire.setClock(100000);
            }
            _boomSource = _reqBoomSrc;
            _tiltSource = _reqTiltSrc;
            _boomInvert = _reqBoomInv;
            _tiltInvert = _reqTiltInv;
            sensorTask_zeroIMU();

            Serial.print("$ACK,DBG,IMU Zeroed! b_off:");
            Serial.print(_boomAngleOffset);
            Serial.print(" t_off:");
            Serial.print(_tiltAngleOffset);
            Serial.print(" b_src:");
            Serial.print(_boomSource);
            Serial.print(" t_src:");
            Serial.print(_tiltSource);
            Serial.print(" b_inv:");
            Serial.print(_boomInvert);
            Serial.print(" t_inv:");
            Serial.println(_tiltInvert);
        }

        // ---- Read all sensors ----

        // AS5600 encoders (angles in degrees)
        float swing = _swingEnc ? _swingEnc->readAngle() : 0.0f;
        float tele  = _teleEnc  ? _teleEnc->readContinuousAngle() : 0.0f;

        // MPU6050 IMU on the boom (updates internal complementary filter)
        if (_imu) _imu->update();

        float mpuRoll  = _imu ? _imu->getRoll()  : 0.0f;
        float mpuPitch = _imu ? _imu->getPitch() : 0.0f;

        // Independent axis sources for boom and tilt
        float boomBase = (_boomSource == 1) ? mpuPitch : mpuRoll;
        float tiltBase = (_tiltSource == 1) ? mpuPitch : mpuRoll;

        // Calibrated zero offset subtraction and inversion
        float rawBoom = (boomBase - _boomAngleOffset) * (_boomInvert ? -1.0f : 1.0f);
        float rawTilt = (tiltBase - _tiltAngleOffset) * (_tiltInvert ? -1.0f : 1.0f);

        float boom = (_imu && _imu->isConnected())
            ? rawBoom
            : (_boomEnc ? _boomEnc->readAngle() : 0.0f);

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

            sensorData.loadCellRaw  = (float)(_loadCell ? _loadCell->getLastRaw() : 0);
            sensorData.actualLoadKg = weight;  // Phase 3: will add angle compensation

            sensorData.imuRoll  = boom;     // Calibrated Boom elevation angle
            sensorData.imuPitch = rawTilt;  // Calibrated lateral chassis tilt angle

            sensorData.fsr[0] = fsrVals[0];
            sensorData.fsr[1] = fsrVals[1];
            sensorData.fsr[2] = fsrVals[2];
            sensorData.fsr[3] = fsrVals[3];

            // Phase 3: these will be computed by SafetyTask
            sensorData.safeLoadLimit = 5.0f;   // Default 5kg crane limit

            // Load sanity check: on a 5kg crane, anything >10kg is uncalibrated raw data or overload error
            bool loadValid = (weight >= -0.5f && weight <= MAX_VALID_LOAD_KG);
            if (loadValid) {
                sensorData.loadPercent = (sensorData.safeLoadLimit > 0)
                    ? (sensorData.actualLoadKg / sensorData.safeLoadLimit) * 100.0f
                    : 0.0f;
                sensorData.alarmLevel = (sensorData.loadPercent >= 100.0f) ? 2 : ((sensorData.loadPercent >= 85.0f) ? 1 : 0);
            } else {
                sensorData.loadPercent = 999.0f;  // Sentinel error value
                sensorData.alarmLevel  = 2;       // Warning
            }

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
