// ============================================================
//  serialLine() — print one complete line to the PC in a single write
//
//  SensorTask (core 1), CommandTask and TelemetryTask (core 0) all print
//  to the same USB serial. The ESP32 core locks the UART per write() call,
//  so a line sent as several print() calls (print + println, or a chain of
//  prints) can get another task's text spliced into the middle of it and
//  corrupt both lines. Formatting into one buffer and printing it once
//  keeps every line intact.
// ============================================================
#ifndef SERIAL_OUT_H
#define SERIAL_OUT_H

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

inline void serialLine(const char* fmt, ...) {
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(buf) - 2) n = sizeof(buf) - 2;   // truncated: still end the line
    buf[n]     = '\n';
    buf[n + 1] = '\0';
    Serial.print(buf);
}

#endif // SERIAL_OUT_H
