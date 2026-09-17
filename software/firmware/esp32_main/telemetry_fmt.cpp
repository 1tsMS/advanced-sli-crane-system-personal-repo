// ============================================================
//  Telemetry Formatter — Implementation
// ============================================================
#include "telemetry_fmt.h"
#include <stdio.h>

int TelemetryFormatter::format(const SensorData& data, char* buffer, size_t bufLen) {
    // Format: $T,boomAngle,extensionMM,measuredLoad,actualLoad,swingAngle,
    //            ropeLenMM,fsr1,fsr2,fsr3,fsr4,imuRoll,imuPitch,
    //            safeLimit,loadPct,alarmLvl\n
    int written = snprintf(buffer, bufLen,
        "$T,%.2f,%.1f,%.2f,%.2f,%.2f,%.1f,%u,%u,%u,%u,%.2f,%.2f,%.2f,%.1f,%u\n",
        data.boomAngle,
        data.extensionMM,
        data.loadCellRaw,
        data.actualLoadKg,
        data.swingAngle,
        data.ropeLengthMM,
        data.fsr[0], data.fsr[1], data.fsr[2], data.fsr[3],
        data.imuRoll,
        data.imuPitch,
        data.safeLoadLimit,
        data.loadPercent,
        data.alarmLevel
    );
    return written;
}

void TelemetryFormatter::formatDebugReport(
    bool swingOK, bool boomOK, bool teleOK,
    bool mpuOK, bool hx711OK,
    const uint16_t fsr[4],
    long n20Ticks,
    char* buffer, size_t bufLen
) {
    // Build multi-line debug response
    // Each line starts with $D for the PC parser to identify
    int pos = 0;

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,I2C0_SWING,0x%02X,%s\n", AS5600_ADDR, swingOK ? "OK" : "FAIL");

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,I2C1_BOOM,0x%02X,%s\n", AS5600_ADDR, boomOK ? "OK" : "FAIL");

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,I2C2_TELE,0x%02X,%s\n", AS5600_ADDR, teleOK ? "OK" : "FAIL");

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,MPU6050,0x%02X,%s\n", MPU6050_ADDR, mpuOK ? "OK" : "FAIL");

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,HX711,NA,%s\n", hx711OK ? "OK" : "FAIL");

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,FSR1,%u\n", fsr[0]);
    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,FSR2,%u\n", fsr[1]);
    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,FSR3,%u\n", fsr[2]);
    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,FSR4,%u\n", fsr[3]);

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,N20_TICKS,%ld\n", n20Ticks);

    pos += snprintf(buffer + pos, bufLen - pos,
        "$D,END\n");
}
