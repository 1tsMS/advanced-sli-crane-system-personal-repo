// ============================================================
//  HX711 Load Cell Driver — Backed by standard HX711 library (bogde)
//  FreeRTOS Non-Blocking Wrapper:
//  Only reads when scale.is_ready() is true, returning cached
//  values otherwise so FreeRTOS tasks are never starved.
// ============================================================
#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

#include "config.h"
#include <HX711.h>

class HX711Driver {
public:
    HX711Driver(uint8_t dtPin, uint8_t sckPin)
        : _dt(dtPin), _sck(sckPin),
          _lastRawValue(0), _lastWeight(0.0f), _connected(false) {}

    /** Initialize pins with standard HX711.begin(). */
    void begin();

    bool isConnected() const { return _connected; }

    /** Returns true if DOUT is LOW (conversion ready). Instantaneous check. */
    bool isReady() { return _scale.is_ready(); }

    /**
     * Non-blocking raw ADC read.
     * @return true if fresh value acquired, false if cached value returned.
     */
    bool readRaw(int32_t* value);

    /**
     * Non-blocking calibrated weight read in kg.
     * Applies tare offset and scale calibration factor.
     */
    float getWeight();

    /**
     * Tare the load cell to set zero offset.
     * Blocks briefly for averaging (call only during calibration, not in hot loop).
     */
    void tare(uint8_t samples = 10);

    /** Set calibration factor (raw counts per kg). */
    void setScale(float scale) { _scale.set_scale(scale); }
    float getScale() { return _scale.get_scale(); }

    /** Get current zero offset. */
    long getOffset() { return _scale.get_offset(); }

    /** Get the last read raw 24-bit ADC counts. */
    int32_t getLastRaw() const { return _lastRawValue; }

    /** Access underlying bogde HX711 instance directly if needed. */
    HX711& getRawScale() { return _scale; }

private:
    uint8_t _dt, _sck;
    HX711   _scale;
    int32_t _lastRawValue;
    float   _lastWeight;
    bool    _connected;
};

#endif // HX711_DRIVER_H
