// ============================================================
//  MPU6050 6-Axis IMU Driver (Accelerometer + Gyroscope)
//  Lives on I2C Bus 2 (Software I2C, shared with AS5600 Tele)
//  Uses complementary filter for stable roll/pitch estimation.
// ============================================================
#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include "config.h"

class SoftI2C;  // Forward declaration

/**
 * MPU6050 driver reading accelerometer + gyroscope data.
 * Applies a complementary filter to fuse accel (noisy, no drift)
 * with gyro (smooth, drifts) for stable angle estimates.
 *
 * Complementary filter:
 *   angle = α × (angle + gyro × dt) + (1 - α) × accelAngle
 *   where α = 0.98 (trust gyro 98%, accel 2%)
 */
class MPU6050Driver {
public:
    MPU6050Driver(SoftI2C* bus)
        : _bus(bus), _connected(false),
          _roll(0.0f), _pitch(0.0f),
          _gyroOffsetX(0.0f), _gyroOffsetY(0.0f), _gyroOffsetZ(0.0f),
          _lastUpdateUs(0) {}

    /**
     * Initialize sensor: wake up from sleep, set ranges.
     * @return true if sensor responds
     */
    bool begin();

    bool isConnected() const { return _connected; }

    /**
     * Read raw data and update filtered roll/pitch.
     * Call this at a consistent rate (e.g., 100Hz from SensorTask).
     */
    void update();

    /** Calibrate gyroscope zero offset. Keep sensor STILL during this! */
    void calibrateGyro(uint16_t samples = 500);

    // Filtered orientation (degrees)
    float getRoll()  const { return _roll; }
    float getPitch() const { return _pitch; }

    // Raw accelerometer (m/s², useful for sway detection in Phase 4)
    float getAccelX() const { return _accelX; }
    float getAccelY() const { return _accelY; }
    float getAccelZ() const { return _accelZ; }

private:
    SoftI2C* _bus;
    bool     _connected;

    // Filtered angles
    float _roll, _pitch;

    // Raw readings (converted to physical units)
    float _accelX, _accelY, _accelZ;  // m/s²
    float _gyroX,  _gyroY,  _gyroZ;   // °/s

    // Gyro zero offsets (from calibration)
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;

    // Timing for integration
    unsigned long _lastUpdateUs;

    // Complementary filter coefficient (0.0–1.0)
    static constexpr float ALPHA = 0.98f;

    // Conversion factors
    static constexpr float ACCEL_SCALE = 16384.0f;  // ±2g range → LSB/g
    static constexpr float GYRO_SCALE  = 131.0f;    // ±250°/s range → LSB/(°/s)
    static constexpr float G_TO_MS2    = 9.80665f;

    // MPU6050 register addresses
    static const uint8_t REG_PWR_MGMT_1  = 0x6B;
    static const uint8_t REG_ACCEL_XOUT  = 0x3B;  // 14 bytes: accel(6) + temp(2) + gyro(6)
    static const uint8_t REG_GYRO_CONFIG = 0x1B;
    static const uint8_t REG_ACCEL_CONFIG = 0x1C;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readBytes(uint8_t reg, uint8_t* buffer, uint8_t count);
};

#endif // MPU6050_DRIVER_H
