// ============================================================
//  MPU6050 IMU Driver — Implementation backed by Adafruit_MPU6050
// ============================================================
#include "mpu6050_driver.h"

bool MPU6050Driver::begin() {
    // Uses Wire (Bus 0: GPIO 21/22, initialized in esp32_main setup)
    // Probe primary address 0x68, then alternate address 0x69 (in case AD0 is pulled high)
    bool ok = _mpu.begin(0x68, &Wire);
    if (!ok) {
        delay(10);
        ok = _mpu.begin(0x69, &Wire);
    }

    if (!ok) {
        _connected = false;
        return false;
    }

    // Configure exact ranges matching proven test code (mpu_hx711_test.ino)
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    _lastUpdateUs = micros();
    _connected = true;
    return true;
}

void MPU6050Driver::update() {
    if (!_connected) {
        // Auto-reconnect probe every 2.5s if unplugged/loose on boot
        static unsigned long lastRetryMs = 0;
        if (millis() - lastRetryMs > 2500) {
            lastRetryMs = millis();
            begin();
        }
        return;
    }

    sensors_event_t a, g, temp;
    if (!_mpu.getEvent(&a, &g, &temp)) {
        _connected = false;
        return;
    }
    Wire.setClock(100000);  // Prevent Adafruit_I2CDevice from leaving bus at 400kHz

    // Accelerometer in m/s²
    _accelX = a.acceleration.x;
    _accelY = a.acceleration.y;
    _accelZ = a.acceleration.z;

    // Gyroscope converted from rad/s to deg/s with calibrated zero offset
    _gyroX = (g.gyro.x * 180.0f / PI) - _gyroOffsetX;
    _gyroY = (g.gyro.y * 180.0f / PI) - _gyroOffsetY;
    _gyroZ = (g.gyro.z * 180.0f / PI) - _gyroOffsetZ;

    // Compute loop delta time
    unsigned long now = micros();
    float dt = (now - _lastUpdateUs) / 1000000.0f;
    _lastUpdateUs = now;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.01f;

    // Remap axes based on physical orientation
    float ax = _remapped(_accelX, _accelY, _accelZ, _axisMap.roll_src,  _axisMap.roll_inv);
    float ay = _remapped(_accelX, _accelY, _accelZ, _axisMap.pitch_src, _axisMap.pitch_inv);
    float az = _accelZ;

    float gx = _remapped(_gyroX, _gyroY, _gyroZ, _axisMap.roll_src,  _axisMap.roll_inv);
    float gy = _remapped(_gyroX, _gyroY, _gyroZ, _axisMap.pitch_src, _axisMap.pitch_inv);

    // Accelerometer-based tilt angles (degrees)
    float accelRoll  = atan2(ay, az) * 180.0f / PI;
    float accelPitch = atan2(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;

    // Complementary filter: 98% gyro rate integration, 2% accelerometer tilt
    _roll  = ALPHA * (_roll  + gx * dt) + (1.0f - ALPHA) * accelRoll;
    _pitch = ALPHA * (_pitch + gy * dt) + (1.0f - ALPHA) * accelPitch;
}

void MPU6050Driver::calibrateGyro(uint16_t samples) {
    if (!_connected) return;

    float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
    uint16_t valid = 0;

    for (uint16_t i = 0; i < samples; i++) {
        sensors_event_t a, g, temp;
        if (_mpu.getEvent(&a, &g, &temp)) {
            sumX += (g.gyro.x * 180.0f / PI);
            sumY += (g.gyro.y * 180.0f / PI);
            sumZ += (g.gyro.z * 180.0f / PI);
            valid++;
        }
        delay(2);
    }

    if (valid > 0) {
        _gyroOffsetX = sumX / valid;
        _gyroOffsetY = sumY / valid;
        _gyroOffsetZ = sumZ / valid;
    }

    // Re-seed complementary filter angles directly to instantaneous accelerometer gravity vector
    sensors_event_t a, g, temp;
    if (_mpu.getEvent(&a, &g, &temp)) {
        float ax = _remapped(a.acceleration.x, a.acceleration.y, a.acceleration.z, _axisMap.roll_src, _axisMap.roll_inv);
        float ay = _remapped(a.acceleration.x, a.acceleration.y, a.acceleration.z, _axisMap.pitch_src, _axisMap.pitch_inv);
        float az = a.acceleration.z;
        _roll  = atan2(ay, az) * 180.0f / PI;
        _pitch = atan2(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
    }
    _lastUpdateUs = micros();
}

float MPU6050Driver::_remapped(float x, float y, float z, int8_t src, bool inv) const {
    float val;
    switch (src) {
        case 0: val = x; break;
        case 1: val = y; break;
        default: val = z; break;
    }
    return inv ? -val : val;
}
