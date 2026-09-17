// ============================================================
//  HX711 Driver — Implementation backed by bogde/HX711
// ============================================================
#include "hx711_driver.h"

void HX711Driver::begin() {
    _scale.begin(_dt, _sck);
    _scale.set_scale(1.0f);  // Default: 1.0 (uncalibrated / 1:1)

    // Give HX711 chip time to settle on power up
    delay(100);
    _connected = _scale.is_ready();
}

bool HX711Driver::readRaw(int32_t* value) {
    if (_scale.is_ready()) {
        _lastRawValue = _scale.read();
        _connected = true;
        *value = _lastRawValue;
        return true;
    }
    *value = _lastRawValue;
    return false;
}

float HX711Driver::getWeight() {
    if (_scale.is_ready()) {
        // get_units(1) applies (raw - offset) / scale without blocking
        _lastWeight = _scale.get_units(1);
        _connected = true;
    }
    return _lastWeight;
}

void HX711Driver::tare(uint8_t samples) {
    // Wait for sensor ready with a short 500ms safety timeout
    uint32_t timeout = millis() + 500;
    while (!_scale.is_ready() && millis() < timeout) {
        delay(5);
    }

    if (_scale.is_ready()) {
        _scale.tare(samples);
        _connected = true;
    }
}
