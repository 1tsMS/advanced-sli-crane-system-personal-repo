// ============================================================
//  MPU6050 IMU Driver — Implementation backed by Adafruit_MPU6050
// ============================================================
#include "mpu6050_driver.h"

bool MPU6050Driver::begin() {
    // Uses Wire (Bus 0: GPIO 21/22, initialized in esp32_main setup)
    // Probe primary address 0x68, then alternate address 0x69 (in case AD0 is pulled high)
    _addr = 0x68;
    bool ok = _mpu.begin(0x68, &Wire);
    if (!ok) {
        delay(10);
        _addr = 0x69;
        ok = _mpu.begin(0x69, &Wire);
    }

    if (!ok) {
        _connected = false;
        return false;
    }

    // Configure exact ranges matching proven test code (mpu_hx711_test.ino)
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    _mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);  // Cut stepper vibration before the comp filter

    _lastUpdateUs = micros();
    _connected = true;
    return true;
}

void MPU6050Driver::update() {
    if (!_connected) {
        // Non-blocking reconnect: quick I2C probe (~1 ms) instead of
        // full Adafruit begin() (~100+ ms) that would stall the sensor loop.
        // Never call Adafruit begin() here: it resets the chip with two 100 ms
        // delays and an untimed wait loop, which froze the WHOLE sensor loop for
        // up to ~2 s every time motor noise made the IMU drop out. Instead the
        // config registers are rewritten directly (~1 ms).
        if (millis() - _lastReconnectMs < 500) return;
        _lastReconnectMs = millis();
        if (fastReinit()) {
            _connected = true;
            _reconnects++;
            _lastUpdateUs = micros();
        }
        return;
    }

    sensors_event_t a, g, temp;
    if (!_mpu.getEvent(&a, &g, &temp)) {
        // Require 3 consecutive failures before marking disconnected:
        // a single noisy transfer shouldn't kill the IMU feed.
        if (++_consecutiveFails >= 3) {
            _connected = false;
            _consecutiveFails = 0;
        }
        return;
    }
    _consecutiveFails = 0;
    Wire.setClock(100000);  // Prevent Adafruit_I2CDevice from leaving bus at 400kHz

    // Plausibility check: under motor noise a transfer can "succeed" with
    // corrupted bytes, which threw the angle around. The crane moves slowly,
    // so a sample far from 1 g or with a huge rotation rate is garbage: skip it.
    {
        const float ax0 = a.acceleration.x, ay0 = a.acceleration.y, az0 = a.acceleration.z;
        const float g2  = ax0 * ax0 + ay0 * ay0 + az0 * az0;          // (m/s^2)^2
        const float gmax = 300.0f * PI / 180.0f;                     // 300 deg/s in rad/s
        if (g2 < 25.0f || g2 > 400.0f ||                              // outside 0.5 g .. 2 g
            fabsf(g.gyro.x) > gmax || fabsf(g.gyro.y) > gmax || fabsf(g.gyro.z) > gmax) {
            _rejected++;
            return;
        }
    }

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

bool MPU6050Driver::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

bool MPU6050Driver::fastReinit() {
    // Same configuration as begin(): wake + PLL clock, sample rate /1,
    // DLPF 5 Hz, gyro +/-500 deg/s, accel +/-8 g.
    return writeReg(0x6B, 0x01) &&   // PWR_MGMT_1: wake, clock = PLL gyro X
           writeReg(0x19, 0x00) &&   // SMPLRT_DIV
           writeReg(0x1A, 0x06) &&   // CONFIG: DLPF 5 Hz
           writeReg(0x1B, 0x08) &&   // GYRO_CONFIG: +/-500 deg/s
           writeReg(0x1C, 0x10);     // ACCEL_CONFIG: +/-8 g
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
