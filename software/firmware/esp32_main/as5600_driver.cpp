// ============================================================
//  AS5600 Driver — Implementation
// ============================================================
#include "as5600_driver.h"
#include "soft_i2c.h"

bool AS5600Driver::begin() {
    uint16_t testVal;
    _connected = readRegister16(REG_RAW_ANGLE, &testVal);
    return _connected;
}

float AS5600Driver::readAngle() {
    uint16_t raw = readRawAngle();
    // Apply zero offset correction
    int16_t corrected = (int16_t)raw - (int16_t)_offset;
    if (corrected < 0) corrected += 4096;
    _lastAngle = (corrected / 4096.0f) * 360.0f;
    return _lastAngle;
}

uint16_t AS5600Driver::readRawAngle() {
    uint16_t value = 0;
    if (!readRegister16(REG_RAW_ANGLE, &value)) {
        _connected = false;
        // Return based on last known angle to avoid jumps
    }
    return value;
}

void AS5600Driver::setZero() {
    _offset = readRawAngle();
}

bool AS5600Driver::readRegister16(uint8_t reg, uint16_t* outValue) {
    // Each bus type uses its own Wire object.
    // All AS5600s have address 0x36 — separated by bus, not address.

    switch (_busType) {
        case BUS_WIRE0: {
            Wire.beginTransmission(AS5600_ADDR);
            Wire.write(reg);
            if (Wire.endTransmission() != 0) return false;
            Wire.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
            if (Wire.available() < 2) return false;
            *outValue = ((uint16_t)Wire.read() << 8) | Wire.read();
            _connected = true;
            return true;
        }

        case BUS_WIRE1: {
            Wire1.beginTransmission(AS5600_ADDR);
            Wire1.write(reg);
            if (Wire1.endTransmission() != 0) return false;
            Wire1.requestFrom((uint8_t)AS5600_ADDR, (uint8_t)2);
            if (Wire1.available() < 2) return false;
            *outValue = ((uint16_t)Wire1.read() << 8) | Wire1.read();
            _connected = true;
            return true;
        }

        case BUS_SOFT: {
            if (!_softBus) return false;
            _softBus->beginTransmission(AS5600_ADDR);
            _softBus->write(reg);
            _softBus->endTransmission();
            if (_softBus->requestFrom(AS5600_ADDR, (uint8_t)2) < 2) return false;
            *outValue = ((uint16_t)_softBus->read() << 8) | _softBus->read();
            _connected = true;
            return true;
        }
    }
    return false;
}
