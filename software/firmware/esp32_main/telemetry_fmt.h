// ============================================================
//  Telemetry Packet Formatter
//  Formats SensorData into the $T CSV telemetry string
//  sent to the PC at TELEMETRY_RATE_HZ.
//
//  Packet format:
//  $T,boomAngle,extensionMM,measuredLoad,actualLoad,swingAngle,
//     ropeLenMM,fsr1,fsr2,fsr3,fsr4,imuRoll,imuPitch,
//     safeLimit,loadPct,alarmLvl\n
// ============================================================
#ifndef TELEMETRY_FMT_H
#define TELEMETRY_FMT_H

#include "config.h"

class TelemetryFormatter {
public:
    TelemetryFormatter() {}

    /**
     * Format a SensorData struct into the $T telemetry string.
     * @param data   Current sensor readings
     * @param buffer Output char buffer (must be >= 256 bytes)
     * @param bufLen Size of the output buffer
     * @return Number of chars written (excluding null terminator)
     */
    int format(const SensorData& data, char* buffer, size_t bufLen);

    /**
     * Format a debug/I2C scan report.
     * Called when the DBG command is received.
     * @param sensorStatus Array of {busName, address, isOK} entries
     */
    void formatDebugReport(
        bool swingOK, bool boomOK, bool teleOK,
        bool mpuOK, bool hx711OK,
        const uint16_t fsr[4],
        long n20Ticks,
        char* buffer, size_t bufLen
    );
};

#endif // TELEMETRY_FMT_H
