// ============================================================
//  MPU6050 Driver — Implementation (Wire / Bus 0)
// ============================================================
#include "mpu6050_driver.h"

bool MPU6050Driver::begin() {
    // Wake up MPU6050 (clears sleep bit)
    if (!writeRegister(REG_PWR_MGMT_1, 0x00)) {
        _connected = false;
        return false;
    }
    delay(100);

    // Gyro range: ±500°/s → register 0x01 (bits [4:3] = 01)
    // This matches test code: MPU6050_RANGE_500_DEG
    if (!writeRegister(REG_GYRO_CONFIG, 0x08)) {
        _connected = false;
        return false;
    }

    // Accel range: ±8g → register 0x10 (bits [4:3] = 10)
    // This matches test code: MPU6050_RANGE_8_G
    if (!writeRegister(REG_ACCEL_CONFIG, 0x10)) {
        _connected = false;
        return false;
    }

    // DLPF: 21 Hz bandwidth → register value 0x04
    // This matches test code: MPU6050_BAND_21_HZ
    writeRegister(REG_CONFIG, 0x04);

    _lastUpdateUs = micros();
    _connected = true;
    return true;
}

void MPU6050Driver::update() {
    if (!_connected) return;

    // Read 14 bytes: AccX(2) AccY(2) AccZ(2) Temp(2) GyroX(2) GyroY(2) GyroZ(2)
    uint8_t buf[14];
    if (!readBytes(REG_ACCEL_XOUT_H, buf, 14)) {
        _connected = false;
        return;
    }

    // Parse raw 16-bit signed big-endian values
    int16_t rawAccX  = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t rawAccY  = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t rawAccZ  = (int16_t)((buf[4]  << 8) | buf[5]);
    // buf[6..7] = temperature (skipped)
    int16_t rawGyroX = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t rawGyroY = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t rawGyroZ = (int16_t)((buf[12] << 8) | buf[13]);

    // Convert to physical units
    // Accel: LSB → g → m/s²
    _accelX = (rawAccX / ACCEL_SCALE) * G_TO_MS2;
    _accelY = (rawAccY / ACCEL_SCALE) * G_TO_MS2;
    _accelZ = (rawAccZ / ACCEL_SCALE) * G_TO_MS2;

    // Gyro: LSB → °/s, subtract calibration offset
    _gyroX = (rawGyroX / GYRO_SCALE) - _gyroOffsetX;
    _gyroY = (rawGyroY / GYRO_SCALE) - _gyroOffsetY;
    _gyroZ = (rawGyroZ / GYRO_SCALE) - _gyroOffsetZ;

    // Compute dt
    unsigned long now = micros();
    float dt = (now - _lastUpdateUs) / 1000000.0f;
    _lastUpdateUs = now;
    if (dt <= 0.0f || dt > 0.1f) dt = 0.01f;

    // Apply axis remapping before filter
    float rollAccelSrc  = _remappedAccel(_accelX, _accelY, _accelZ, _axisMap.roll_src,  _axisMap.roll_inv);
    float pitchAccelSrc = _remappedAccel(_accelX, _accelY, _accelZ, _axisMap.pitch_src, _axisMap.pitch_inv);
    float rollGyroSrc   = _remappedGyro(_gyroX,  _gyroY,  _gyroZ,  _axisMap.roll_src,  _axisMap.roll_inv);
    float pitchGyroSrc  = _remappedGyro(_gyroX,  _gyroY,  _gyroZ,  _axisMap.pitch_src, _axisMap.pitch_inv);

    // Accelerometer-based angle (noisy, no drift)
    float accelRoll  = atan2(_accelY, _accelZ) * 180.0f / PI;
    float accelPitch = atan2(-_accelX, sqrtf(_accelY * _accelY + _accelZ * _accelZ)) * 180.0f / PI;

    // Complementary filter: trust gyro 98%, correct drift with accel 2%
    _roll  = ALPHA * (_roll  + rollGyroSrc  * dt) + (1.0f - ALPHA) * accelRoll;
    _pitch = ALPHA * (_pitch + pitchGyroSrc * dt) + (1.0f - ALPHA) * accelPitch;

    (void)rollAccelSrc; (void)pitchAccelSrc;  // Suppress unused warning
}

void MPU6050Driver::calibrateGyro(uint16_t samples) {
    if (!_connected) return;

    float sumX = 0, sumY = 0, sumZ = 0;
    uint8_t buf[14];

    for (uint16_t i = 0; i < samples; i++) {
        if (readBytes(REG_ACCEL_XOUT_H, buf, 14)) {
            sumX += (int16_t)((buf[8]  << 8) | buf[9])  / GYRO_SCALE;
            sumY += (int16_t)((buf[10] << 8) | buf[11]) / GYRO_SCALE;
            sumZ += (int16_t)((buf[12] << 8) | buf[13]) / GYRO_SCALE;
        }
        delay(2);
    }
    _gyroOffsetX = sumX / samples;
    _gyroOffsetY = sumY / samples;
    _gyroOffsetZ = sumZ / samples;

    _roll = 0.0f;
    _pitch = 0.0f;
    _lastUpdateUs = micros();
}

// ---- Wire I2C Helpers ----

bool MPU6050Driver::writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return (Wire.endTransmission() == 0);
}

bool MPU6050Driver::readBytes(uint8_t reg, uint8_t* buffer, uint8_t count) {
    Wire.beginTransmission(MPU6050_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;  // Repeated start

    Wire.requestFrom((uint8_t)MPU6050_ADDR, count);
    if (Wire.available() < count) return false;

    for (uint8_t i = 0; i < count; i++) {
        buffer[i] = Wire.read();
    }
    return true;
}

// ---- Axis remapping helpers ----

float MPU6050Driver::_remappedAccel(float ax, float ay, float az, int8_t src, bool inv) const {
    float val;
    switch (src) {
        case 0: val = ax; break;
        case 1: val = ay; break;
        default: val = az; break;
    }
    return inv ? -val : val;
}

float MPU6050Driver::_remappedGyro(float gx, float gy, float gz, int8_t src, bool inv) const {
    float val;
    switch (src) {
        case 0: val = gx; break;
        case 1: val = gy; break;
        default: val = gz; break;
    }
    return inv ? -val : val;
}
