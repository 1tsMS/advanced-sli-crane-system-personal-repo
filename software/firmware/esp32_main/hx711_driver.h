// ============================================================
//  HX711 Load Cell Driver — Non-Blocking for FreeRTOS
//  Standard HX711 libraries use blocking delay() which starves
//  other RTOS tasks. This driver checks data-ready and reads
//  only when available, returning last-known value otherwise.
// ============================================================
#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

#include "config.h"

/**
 * Non-blocking HX711 24-bit ADC driver.
 *
 * How HX711 works:
 *   1. When data is ready, DOUT goes LOW
 *   2. Master sends 25 clock pulses on SCK
 *   3. HX711 shifts out 24 bits of data on DOUT (MSB first)
 *   4. 25th clock sets gain for next reading (1 pulse = gain 128, Channel A)
 *
 * Tare:  Stores the current reading as zero offset
 * Scale: User provides a calibration factor (units per raw count)
 */
class HX711Driver {
public:
    HX711Driver(uint8_t dtPin, uint8_t sckPin)
        : _dt(dtPin), _sck(sckPin),
          _offset(0), _scale(1.0f),
          _lastRawValue(0), _connected(false) {}

    /** Initialize pins. Call in setup(). */
    void begin();

    bool isConnected() const { return _connected; }

    /** Returns true if new data is ready to read (DOUT is LOW). */
    bool isReady();

    /**
     * Read raw 24-bit value. Non-blocking: only reads if isReady().
     * @return true if a fresh value was read, false if returning cached
     */
    bool readRaw(int32_t* value);

    /**
     * Get weight in kg (or whatever unit your scale factor is in).
     * Returns last computed value if no fresh reading available.
     */
    float getWeight();

    /**
     * Set current reading as zero (tare).
     * This one DOES block briefly to get a stable reading.
     * Call during calibration, not in a hot loop.
     */
    void tare(uint8_t samples = 10);

    /**
     * Set the calibration scale factor.
     * Determined by: place known weight → raw_reading / known_weight_kg
     */
    void setScale(float scale) { _scale = scale; }
    float getScale() const { return _scale; }

    /** Get the tare offset. */
    int32_t getOffset() const { return _offset; }

private:
    uint8_t _dt, _sck;
    int32_t _offset;
    float   _scale;
    int32_t _lastRawValue;
    bool    _connected;

    /** Blocking read of one 24-bit sample. Used internally. */
    int32_t readOneSample();
};

#endif // HX711_DRIVER_H
