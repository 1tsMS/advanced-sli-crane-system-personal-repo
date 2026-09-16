// ============================================================
//  Sensor Task — FreeRTOS Task (Core 1, Priority 5)
//  Reads ALL sensors at SENSOR_RATE_HZ and writes to the
//  shared SensorData struct (mutex-protected).
// ============================================================
#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "config.h"
#include "as5600_driver.h"
#include "mpu6050_driver.h"
#include "hx711_driver.h"
#include "fsr_reader.h"
#include "n20_encoder.h"

/**
 * Initialize all sensor driver instances.
 * Call ONCE in setup() before creating the task.
 */
void sensorTask_init(
    AS5600Driver* swingEnc,
    AS5600Driver* boomEnc,
    AS5600Driver* teleEnc,
    MPU6050Driver* imu,
    HX711Driver* loadCell,
    FSRReader* fsr,
    N20Encoder* winchEnc
);

/**
 * FreeRTOS task function — runs forever at SENSOR_RATE_HZ.
 * Reads all sensors → writes to global SensorData → releases mutex.
 */
void sensorTask(void* pvParameters);

#endif // SENSOR_TASK_H
