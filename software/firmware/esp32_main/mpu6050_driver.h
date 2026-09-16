// ============================================================
//  MPU6050 6-Axis IMU Driver
//  Uses Hardware Wire (Bus 0, GPIO 21/22) — confirmed working
//  on this bus from mpu_hx711_test.ino.
//
//  Range settings match test code:
//    Accelerometer: ±8g  → scale = 4096 LSB/g
//    Gyroscope:     ±500°/s → scale = 65.5 LSB/(°/s)
//    Filter:        21 Hz bandwidth
//
//  Applies complementary filter for stable roll/pitch estimation.
// ============================================================
#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include "config.h"
#include <Wire.h>

/**
 * MPU6050 driver on Hardware Wire (Bus 0).
 * Range settings mirror the working test code (±8g, ±500°/s).
 *
 * Complementary filter:
 *   angle = α × (angle + gyro × dt) + (1 - α) × accelAngle
 *   α = 0.98
 */
class MPU6050Driver {
public:
    /**
     * Axis remapping for orientation-agnostic angle readings.
     * Set these if the IMU is mounted in a non-standard orientation.
     */
    struct AxisMap {
        int8_t roll_src  = 0;   // 0=X, 1=Y, 2=Z
        int8_t pitch_src = 1;
        bool   roll_inv  = false;
        bool   pitch_inv = false;
    };

    MPU6050Driver()
        : _connected(false),
          _roll(0.0f), _pitch(0.0f),
          _gyroOffsetX(0.0f), _gyroOffsetY(0.0f), _gyroOffsetZ(0.0f),
          _lastUpdateUs(0) {}

    /**
     * Initialize sensor on Wire (Bus 0).
     * @return true if sensor responds at 0x68
     */
    bool begin();

    bool isConnected() const { return _connected; }

    /**
     * Read raw data and update filtered roll/pitch.
     * Call at a consistent rate (100Hz from SensorTask).
     */
    void update();

    /** Calibrate gyroscope zero offset. Keep sensor STILL! */
    void calibrateGyro(uint16_t samples = 500);

    // Filtered orientation (degrees) — after axis remap
    float getRoll()  const { return _roll; }
    float getPitch() const { return _pitch; }

    // Raw accelerometer (m/s²)
    float getAccelX() const { return _accelX; }
    float getAccelY() const { return _accelY; }
    float getAccelZ() const { return _accelZ; }

    /** Set axis remapping for non-standard mounting orientation. */
    void setAxisMap(AxisMap map) { _axisMap = map; }
    AxisMap getAxisMap() const { return _axisMap; }

private:
    bool  _connected;
    float _roll, _pitch;
    float _accelX, _accelY, _accelZ;  // m/s²
    float _gyroX,  _gyroY,  _gyroZ;   // °/s
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;
    unsigned long _lastUpdateUs;
    AxisMap _axisMap;

    // Complementary filter coefficient
    static constexpr float ALPHA = 0.98f;

    // Scale factors matching test code:
    //   Accel: MPU6050_RANGE_8_G  → 4096 LSB/g
    //   Gyro:  MPU6050_RANGE_500_DEG → 65.5 LSB/(°/s)
    static constexpr float ACCEL_SCALE = 4096.0f;
    static constexpr float GYRO_SCALE  = 65.5f;
    static constexpr float G_TO_MS2    = 9.80665f;

    // MPU6050 register addresses
    static const uint8_t REG_PWR_MGMT_1   = 0x6B;
    static const uint8_t REG_ACCEL_CONFIG  = 0x1C;
    static const uint8_t REG_GYRO_CONFIG   = 0x1B;
    static const uint8_t REG_CONFIG        = 0x1A;  // DLPF
    static const uint8_t REG_ACCEL_XOUT_H = 0x3B;

    bool writeRegister(uint8_t reg, uint8_t value);
    bool readBytes(uint8_t reg, uint8_t* buffer, uint8_t count);

    // Apply axis remap and return the correct accel component
    float _remappedAccel(float ax, float ay, float az, int8_t src, bool inv) const;
    float _remappedGyro(float gx, float gy, float gz, int8_t src, bool inv) const;
};

#endif // MPU6050_DRIVER_H
