// ============================================================
//  Mega Bridge — Implementation
// ============================================================
#include "mega_bridge.h"
#include <stdio.h>

void MegaBridge::begin(HardwareSerial* serial) {
    _megaSerial = serial;
    _megaSerial->begin(MEGA_BAUD, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);
    _connected = true;
}

void MegaBridge::sendMotorCommand(const MotorCommand& cmd) {
    if (!_megaSerial || !_connected) return;

    char line[32];

    // Emergency stop → forward immediately
    if (cmd.isEstop) {
        _megaSerial->println("T0");
        return;
    }

    // Only forward stepper axes (1=Swing, 2=Lift, 3=Tele) to Mega
    // Axis 4 (Winch) is driven directly by ESP32's DRV8833
    if (cmd.axis >= 1 && cmd.axis <= 3) {
        snprintf(line, sizeof(line), "M%u S%u D%u",
                 cmd.axis, cmd.speed, cmd.direction);
        _megaSerial->println(line);
    }

    // Stop axis command
    if (cmd.axis == 0 && cmd.stopAxis >= 1 && cmd.stopAxis <= 3) {
        snprintf(line, sizeof(line), "M0 A%u", cmd.stopAxis);
        _megaSerial->println(line);
    }
}

void MegaBridge::sendRaw(const char* gcode) {
    if (!_megaSerial || !_connected) return;
    _megaSerial->println(gcode);
}

bool MegaBridge::hasResponse() {
    if (!_megaSerial) return false;
    return _megaSerial->available() > 0;
}

int MegaBridge::readResponse(char* buffer, size_t bufLen) {
    if (!_megaSerial || !_megaSerial->available()) return 0;

    int i = 0;
    unsigned long timeout = millis() + 10;  // 10ms max wait

    while (i < (int)(bufLen - 1) && millis() < timeout) {
        if (_megaSerial->available()) {
            char c = _megaSerial->read();
            if (c == '\n' || c == '\r') {
                if (i > 0) break;  // End of line
                continue;          // Skip leading newlines
            }
            buffer[i++] = c;
        }
    }
    buffer[i] = '\0';
    return i;
}
