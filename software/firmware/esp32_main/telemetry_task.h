// ============================================================
//  Telemetry Task — FreeRTOS Task (Core 0, Priority 3)
//  Reads the shared SensorData struct and pushes formatted
//  $T packets over USB Serial to the PC at TELEMETRY_RATE_HZ.
// ============================================================
#ifndef TELEMETRY_TASK_H
#define TELEMETRY_TASK_H

#include "config.h"

/**
 * FreeRTOS task — runs forever at TELEMETRY_RATE_HZ.
 * Reads SensorData (mutex) → formats $T packet → Serial.print()
 */
void telemetryTask(void* pvParameters);

/**
 * Dynamically change the telemetry push rate.
 * Called by CommandTask when a CFG RATE command is received.
 */
void telemetryTask_setRate(uint16_t hz);

#endif // TELEMETRY_TASK_H
