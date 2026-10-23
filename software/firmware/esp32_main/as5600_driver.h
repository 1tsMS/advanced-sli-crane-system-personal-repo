// ============================================================
//  AS5600 12-bit Magnetic Rotary Encoder Driver
//  Supports all 3 I2C buses (Wire, Wire1, SoftI2C)
//
//  Only SensorTask may call the methods that touch the bus (begin,
//  readRaw, readMagnetStatus). Other tasks use isConnected(), which is
//  a cached flag, so two cores never drive the same bus at once.
// ============================================================
#ifndef AS5600_DRIVER_H
#define AS5600_DRIVER_H

#include "config.h"
#include <Wire.h>

class SoftI2C;  // Forward declaration

/** Magnet health as reported by the AS5600 itself. */
struct AS5600MagnetStatus {
    bool     valid;      // the registers were read successfully
    bool     detected;   // MD: magnet detected
    bool     tooWeak;    // ML: field too weak (magnet too far / too small)
    bool     tooStrong;  // MH: field too strong (magnet too close)
    uint8_t  agc;        // automatic gain: mid-range is ideal (0..128 at 3.3 V, 0..255 at 5 V)
    uint16_t magnitude;  // CORDIC magnitude (field strength, 12-bit)
};

/**
 * Driver for AS5600 contactless magnetic rotary position sensor.
 * Each AS5600 has a fixed address (0x36), so each must be on a
 * separate I2C bus. We support hardware Wire/Wire1 and software I2C.
 */
class AS5600Driver {
public:
    enum BusType { BUS_WIRE0, BUS_WIRE1, BUS_SOFT };

    AS5600Driver(BusType bus, SoftI2C* softBus = nullptr)
        : _busType(bus), _softBus(softBus) {}

    /** Probe the sensor. Returns true if it responds on I2C. */
    bool begin();

    /** Cached: true while reads are succeeding. Never touches the bus. */
    bool isConnected() const { return _connected; }

    /**
     * Read the raw 12-bit angle (0-4095) as the wrap-aware median of three
     * back-to-back reads, so one corrupted I2C transfer can't produce a spike.
     * If the sensor is marked disconnected it is re-probed at most once a second.
     * @return false if no trustworthy value could be read (out is untouched)
     */
    bool readRaw(uint16_t* out);

    /** Read STATUS, AGC and MAGNITUDE (magnet placement diagnostics). */
    bool readMagnetStatus(AS5600MagnetStatus* out);

    /** Single-transfer reads attempted / failed since boot (wiring quality). */
    uint32_t readCount() const { return _reads; }
    uint32_t failCount() const { return _fails; }

private:
    BusType   _busType;
    SoftI2C*  _softBus;
    bool      _connected = false;
    uint8_t   _missCount = 0;
    uint32_t  _lastProbeMs = 0;
    uint32_t  _reads = 0;
    uint32_t  _fails = 0;

    // Register addresses (datasheet section 8)
    static const uint8_t REG_STATUS    = 0x0B;  // MD (bit 5), ML (bit 4), MH (bit 3)
    static const uint8_t REG_RAW_ANGLE = 0x0C;  // 12-bit unscaled angle
    static const uint8_t REG_AGC       = 0x1A;
    static const uint8_t REG_MAGNITUDE = 0x1B;  // 12-bit

    // Two good reads must agree within this many counts (~2.1 deg)
    static const int32_t AGREE_COUNTS = 24;
    // Single-read fallback: accepted if within this of the last known good value
    // (~8.4 deg — generous enough for normal shaft speeds, rejects wild bus errors)
    static const int32_t SINGLE_READ_GATE = 96;

    bool readAngleOnce(uint16_t* out);
    bool readBytes(uint8_t reg, uint8_t* buf, uint8_t len);

    uint16_t _lastGoodRaw = 0;     // last accepted raw angle for single-read fallback
    bool     _hasLastGood = false;  // true after first successful combineReads
};

#endif // AS5600_DRIVER_H
