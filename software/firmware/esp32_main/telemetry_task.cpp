// ============================================================
//  Telemetry Task — Implementation
// ============================================================
#include "telemetry_task.h"
#include "telemetry_fmt.h"

// Global sensor data — declared in esp32_main.ino
extern SensorData sensorData;

// Runtime-adjustable telemetry period
static volatile uint16_t _telemetryPeriodMs = TELEMETRY_PERIOD_MS;

void telemetryTask_setRate(uint16_t hz) {
    if (hz > 0 && hz <= 200) {
        _telemetryPeriodMs = 1000 / hz;
    }
}

void telemetryTask(void* pvParameters) {
    TelemetryFormatter formatter;
    char txBuffer[256];

    TickType_t lastWakeTime = xTaskGetTickCount();

    for (;;) {
        // Snapshot sensor data under mutex
        SensorData snapshot;
        if (xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            snapshot = sensorData;  // Struct copy
            xSemaphoreGive(sensorMutex);
        }

        // Format the $T telemetry packet
        formatter.format(snapshot, txBuffer, sizeof(txBuffer));

        // Push to USB Serial (to PC / Python backend)
        Serial.print(txBuffer);

        // Sleep until next period
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(_telemetryPeriodMs));
    }
}
