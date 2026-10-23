// ============================================================
//  AS5600 Driver — Implementation
// ============================================================
#include "as5600_driver.h"
#include "soft_i2c.h"
#include "encoder_axis.h"   // EncoderAxis::wrapDiff

bool AS5600Driver::begin() {
    _lastProbeMs = millis();
    bool found = false;
    for (int retry = 0; retry < 3 && !found; retry++) {
        switch (_busType) {
            case BUS_WIRE0:
                Wire.beginTransmission(AS5600_ADDR);
                found = (Wire.endTransmission() == 0);
                break;
            case BUS_WIRE1:
                Wire1.beginTransmission(AS5600_ADDR);
                found = (Wire1.endTransmission() == 0);
                break;
            case BUS_SOFT:
                if (_softBus) {
                    found = _softBus->beginTransmission(AS5600_ADDR);
                    _softBus->endTransmission();
                }
                break;
        }
        if (!found) delay(2);
    }
    _connected = found;
    _missCount = 0;
    return found;
}

bool AS5600Driver::readRaw(uint16_t* out) {
    if (!_connected) {
        // Re-probe every 100 ms (was 1000 ms — the 1 s freeze was the main
        // user-visible lag when motor noise kept tripping the miss counter).
        if (millis() - _lastProbeMs < 100 || !begin()) return false;
    }

    // Wire0 is shared with the MPU6050, whose library may change the clock
    if (_busType == BUS_WIRE0) Wire.setClock(100000);

    uint16_t r[3];
    uint8_t  n = 0;
    for (uint8_t i = 0; i < 3; i++) {
        if (readAngleOnce(&r[n])) n++;
    }

    if (n == 0) {
        if (++_missCount > 50) _connected = false;   // ~0.5 s of failures at 100 Hz (was 30)
        return false;
    }
    _missCount = 0;

    // Best case: median / agreement of 2-3 reads
    if (EncoderAxis::combineReads(r, n, AGREE_COUNTS, out)) {
        _lastGoodRaw = *out;
        _hasLastGood = true;
        return true;
    }

    // Fallback: only 1 read succeeded — accept it if it's within a sane
    // distance of the last known good value (rejects wild bus errors).
    if (n == 1 && _hasLastGood) {
        int32_t diff = EncoderAxis::wrapDiff((int32_t)r[0], (int32_t)_lastGoodRaw);
        if (diff > -SINGLE_READ_GATE && diff < SINGLE_READ_GATE) {
            *out = r[0];
            _lastGoodRaw = r[0];
            return true;
        }
    }
    return false;
}

bool AS5600Driver::readAngleOnce(uint16_t* out) {
    uint8_t b[2];
    _reads++;
    if (!readBytes(REG_RAW_ANGLE, b, 2)) {
        _fails++;
        return false;
    }
    // Top nibble of the high byte is always 0; anything else is a bus error
    // (0xFFFF is the classic "nobody pulled SDA low" floating read).
    if (b[0] & 0xF0) {
        _fails++;
        return false;
    }
    *out = ((uint16_t)b[0] << 8) | b[1];
    return true;
}

bool AS5600Driver::readMagnetStatus(AS5600MagnetStatus* out) {
    out->valid = false;
    if (!_connected) return false;
    if (_busType == BUS_WIRE0) Wire.setClock(100000);

    uint8_t status = 0, agc = 0, mag[2] = {0, 0};
    if (!readBytes(REG_STATUS, &status, 1)) return false;
    if (!readBytes(REG_AGC, &agc, 1))       return false;
    if (!readBytes(REG_MAGNITUDE, mag, 2))  return false;

    out->detected  = (status & 0x20) != 0;
    out->tooWeak   = (status & 0x10) != 0;
    out->tooStrong = (status & 0x08) != 0;
    out->agc       = agc;
    out->magnitude = (((uint16_t)mag[0] << 8) | mag[1]) & 0x0FFF;
    out->valid     = true;
    return true;
}

bool AS5600Driver::readBytes(uint8_t reg, uint8_t* buf, uint8_t len) {
    // Each bus type uses its own Wire object.
    // All AS5600s have address 0x36 — separated by bus, not address.
    switch (_busType) {
        case BUS_WIRE0:
        case BUS_WIRE1: {
            TwoWire& w = (_busType == BUS_WIRE0) ? Wire : Wire1;
            w.beginTransmission(AS5600_ADDR);
            w.write(reg);
            if (w.endTransmission(false) != 0) {      // repeated start
                // Some modules NACK the repeated start on a shared bus: retry with a STOP
                w.beginTransmission(AS5600_ADDR);
                w.write(reg);
                if (w.endTransmission(true) != 0) return false;
            }
            if (w.requestFrom((uint8_t)AS5600_ADDR, len) != len) return false;
            for (uint8_t i = 0; i < len; i++) buf[i] = w.read();
            return true;
        }
        case BUS_SOFT: {
            if (!_softBus) return false;
            if (!_softBus->beginTransmission(AS5600_ADDR) || !_softBus->write(reg)) {
                _softBus->endTransmission();
                return false;
            }
            _softBus->endTransmission();
            if (_softBus->requestFrom(AS5600_ADDR, len) < len) return false;
            for (uint8_t i = 0; i < len; i++) buf[i] = _softBus->read();
            return true;
        }
    }
    return false;
}
