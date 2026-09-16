// ============================================================
//  MPU6050 Driver — Implementation
// ============================================================
#include "mpu6050_driver.h"
#include "soft_i2c.h"

bool MPU6050Driver::begin() {
    // Wake up MPU6050 (clear sleep bit in PWR_MGMT_1)
    if (!writeRegister(REG_PWR_MGMT_1, 0x00)) {
        _connected = false;
        return false;
    }
    delay(100);  // Startup time

    // Set gyro range to ±250°/s (GYRO_SCALE = 131 LSB/(°/s))
    writeRegister(REG_GYRO_CONFIG, 0x00);

    // Set accel range to ±2g (ACCEL_SCALE = 16384 LSB/g)
    writeRegister(REG_ACCEL_CONFIG, 0x00);

    _lastUpdateUs = micros();
    _connected = true;
    return true;
}

void MPU6050Driver::update() {
    if (!_connected) return;

    // Read 14 bytes starting at ACCEL_XOUT_H
    // Layout: AccX(2) AccY(2) AccZ(2) Temp(2) GyroX(2) GyroY(2) GyroZ(2)
    uint8_t buf[14];
    if (!readBytes(REG_ACCEL_XOUT, buf, 14)) {
        _connected = false;
        return;
    }

    // Parse raw 16-bit signed values (big-endian)
    int16_t rawAccX  = (buf[0]  << 8) | buf[1];
    int16_t rawAccY  = (buf[2]  << 8) | buf[3];
    int16_t rawAccZ  = (buf[4]  << 8) | buf[5];
    // buf[6..7] = temperature (unused)
    int16_t rawGyroX = (buf[8]  << 8) | buf[9];
    int16_t rawGyroY = (buf[10] << 8) | buf[11];
    int16_t rawGyroZ = (buf[12] << 8) | buf[13];

    // Convert to physical units
    _accelX = (rawAccX / ACCEL_SCALE) * G_TO_MS2;
    _accelY = (rawAccY / ACCEL_SCALE) * G_TO_MS2;
    _accelZ = (rawAccZ / ACCEL_SCALE) * G_TO_MS2;

    _gyroX = (rawGyroX / GYRO_SCALE) - _gyroOffsetX;  // °/s, offset-corrected
    _gyroY = (rawGyroY / GYRO_SCALE) - _gyroOffsetY;
    _gyroZ = (rawGyroZ / GYRO_SCALE) - _gyroOffsetZ;

    // Compute dt in seconds
    unsigned long now = micros();
    float dt = (now - _lastUpdateUs) / 1000000.0f;
    _lastUpdateUs = now;

    // Clamp dt to avoid spikes after long pauses
    if (dt > 0.1f) dt = 0.01f;

    // Accelerometer-based angle estimates (noisy but no drift)
    // atan2 gives angle of gravity vector
    float accelRoll  = atan2(_accelY, _accelZ) * 180.0f / PI;
    float accelPitch = atan2(-_accelX, sqrt(_accelY * _accelY + _accelZ * _accelZ)) * 180.0f / PI;

    // Complementary filter:
    //   Fuse gyro integration (smooth, drifts) with accel angle (noisy, no drift)
    //   α = 0.98 means we trust the gyro 98% and correct drift with accel 2%
    _roll  = ALPHA * (_roll  + _gyroX * dt) + (1.0f - ALPHA) * accelRoll;
    _pitch = ALPHA * (_pitch + _gyroY * dt) + (1.0f - ALPHA) * accelPitch;
}

void MPU6050Driver::calibrateGyro(uint16_t samples) {
    if (!_connected) return;

    float sumX = 0, sumY = 0, sumZ = 0;
    uint8_t buf[14];

    for (uint16_t i = 0; i < samples; i++) {
        if (readBytes(REG_ACCEL_XOUT, buf, 14)) {
            int16_t rawGyroX = (buf[8]  << 8) | buf[9];
            int16_t rawGyroY = (buf[10] << 8) | buf[11];
            int16_t rawGyroZ = (buf[12] << 8) | buf[13];
            sumX += rawGyroX / GYRO_SCALE;
            sumY += rawGyroY / GYRO_SCALE;
            sumZ += rawGyroZ / GYRO_SCALE;
        }
        delay(2);  // ~500Hz sampling during calibration
    }

    _gyroOffsetX = sumX / samples;
    _gyroOffsetY = sumY / samples;
    _gyroOffsetZ = sumZ / samples;

    // Reset filtered angles after calibration
    _roll = 0.0f;
    _pitch = 0.0f;
    _lastUpdateUs = micros();
}

// ---- I2C Helpers (via SoftI2C) ----

bool MPU6050Driver::writeRegister(uint8_t reg, uint8_t value) {
    if (!_bus) return false;
    _bus->beginTransmission(MPU6050_ADDR);
    _bus->write(reg);
    _bus->write(value);
    _bus->endTransmission();
    return true;  // SoftI2C doesn't return error codes easily, assume success
}

bool MPU6050Driver::readBytes(uint8_t reg, uint8_t* buffer, uint8_t count) {
    if (!_bus) return false;
    _bus->beginTransmission(MPU6050_ADDR);
    _bus->write(reg);
    _bus->endTransmission();

    uint8_t received = _bus->requestFrom(MPU6050_ADDR, count);
    if (received < count) return false;

    for (uint8_t i = 0; i < count; i++) {
        buffer[i] = _bus->read();
    }
    return true;
}
