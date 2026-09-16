// ============================================================
//  HX711 Driver — Implementation
// ============================================================
#include "hx711_driver.h"

void HX711Driver::begin() {
    pinMode(_sck, OUTPUT);
    pinMode(_dt, INPUT);
    digitalWrite(_sck, LOW);

    // Power cycle: SCK HIGH for >60µs resets HX711, then LOW to start
    digitalWrite(_sck, HIGH);
    delayMicroseconds(100);
    digitalWrite(_sck, LOW);

    // Wait for first reading to become available
    delay(400);
    _connected = isReady();
}

bool HX711Driver::isReady() {
    // HX711 pulls DOUT LOW when a new conversion result is ready
    return digitalRead(_dt) == LOW;
}

bool HX711Driver::readRaw(int32_t* value) {
    if (!isReady()) {
        *value = _lastRawValue;  // Return cached value
        return false;            // No fresh data
    }

    _lastRawValue = readOneSample();
    *value = _lastRawValue;
    _connected = true;
    return true;
}

float HX711Driver::getWeight() {
    int32_t raw;
    readRaw(&raw);

    // Apply tare offset and scale
    if (_scale == 0.0f) return 0.0f;
    return (float)(raw - _offset) / _scale;
}

void HX711Driver::tare(uint8_t samples) {
    int64_t sum = 0;
    uint8_t validReadings = 0;

    for (uint8_t i = 0; i < samples; i++) {
        // Wait for data ready (blocking — acceptable during calibration)
        uint32_t timeout = millis() + 200;
        while (!isReady()) {
            if (millis() > timeout) break;
            delay(1);
        }
        if (isReady()) {
            sum += readOneSample();
            validReadings++;
        }
        delay(10);
    }

    if (validReadings > 0) {
        _offset = (int32_t)(sum / validReadings);
    }
}

int32_t HX711Driver::readOneSample() {
    // Read 24 bits of data from HX711
    // Clock out 25 pulses: 24 data bits + 1 pulse to set gain to 128 (Channel A)
    int32_t value = 0;

    // Disable interrupts briefly for precise bit-bang timing
    noInterrupts();

    for (uint8_t i = 0; i < 24; i++) {
        digitalWrite(_sck, HIGH);
        delayMicroseconds(1);
        value = (value << 1) | digitalRead(_dt);
        digitalWrite(_sck, LOW);
        delayMicroseconds(1);
    }

    // 25th pulse: sets gain to 128, Channel A for next conversion
    digitalWrite(_sck, HIGH);
    delayMicroseconds(1);
    digitalWrite(_sck, LOW);
    delayMicroseconds(1);

    interrupts();

    // HX711 outputs 24-bit two's complement
    // If bit 23 is set, the value is negative — sign-extend to 32 bits
    if (value & 0x800000) {
        value |= 0xFF000000;  // Sign extend
    }

    return value;
}
