// ============================================================
//  MPU6050 6-Axis IMU Driver — Backed by Adafruit_MPU6050
//  Uses Hardware Wire (Bus 0, GPIO 21/22) — confirmed working
//  on this bus from mpu_hx711_test.ino.
//
//  Range settings mirror working test code:
//    Accelerometer: ±8g
//    Gyroscope:     ±500°/s
//    Filter:        21 Hz DLPF bandwidth
//
//  Applies complementary filter for stable roll/pitch estimation.
// ============================================================
#ifndef MPU6050_DRIVER_H
#define MPU6050_DRIVER_H

#include "config.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

class MPU6050Driver {
public:
    struct AxisMap {
        int8_t roll_src  = 0;   // 0=X, 1=Y, 2=Z
        int8_t pitch_src = 1;
        bool   roll_inv  = false;
        bool   pitch_inv = false;
    };

    MPU6050Driver()
        : _connected(false),
          _roll(0.0f), _pitch(0.0f),
          _accelX(0.0f), _accelY(0.0f), _accelZ(0.0f),
          _gyroX(0.0f), _gyroY(0.0f), _gyroZ(0.0f),
          _gyroOffsetX(0.0f), _gyroOffsetY(0.0f), _gyroOffsetZ(0.0f),
          _lastUpdateUs(0) {}

    /** Initialize sensor on Wire using Adafruit_MPU6050. */
    bool begin();

    bool isConnected() const { return _connected; }

    /** Read sensor event and update complementary filter. Call at 100Hz. */
    void update();

    /** Calibrate gyroscope zero offset while stationary. */
    void calibrateGyro(uint16_t samples = 200);

    // Filtered orientation in degrees
    float getRoll()  const { return _roll; }
    float getPitch() const { return _pitch; }

    // Accelerometer readings in m/s²
    float getAccelX() const { return _accelX; }
    float getAccelY() const { return _accelY; }
    float getAccelZ() const { return _accelZ; }

    // Gyro readings in °/s
    float getGyroX() const { return _gyroX; }
    float getGyroY() const { return _gyroY; }
    float getGyroZ() const { return _gyroZ; }

    void setAxisMap(AxisMap map) { _axisMap = map; }
    AxisMap getAxisMap() const { return _axisMap; }

    Adafruit_MPU6050& getRawMPU() { return _mpu; }

private:
    Adafruit_MPU6050 _mpu;
    bool  _connected;
    float _roll, _pitch;
    float _accelX, _accelY, _accelZ;  // m/s²
    float _gyroX,  _gyroY,  _gyroZ;   // °/s
    float _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ;
    unsigned long _lastUpdateUs;
    AxisMap _axisMap;

    static constexpr float ALPHA = 0.98f;

    float _remapped(float x, float y, float z, int8_t src, bool inv) const;
};

#endif // MPU6050_DRIVER_H
