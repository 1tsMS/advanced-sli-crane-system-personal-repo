// ============================================================
//  Lightweight Software I2C (bit-bang) for ESP32
//  Used for Bus 2 (GPIO32/33) since ESP32 only has 2 hardware
//  I2C peripherals and we need 3 independent buses.
//
//  API mirrors TwoWire for consistency:
//    beginTransmission(), write(), endTransmission(),
//    requestFrom(), read(), available()
// ============================================================
#ifndef SOFT_I2C_H
#define SOFT_I2C_H

#include <Arduino.h>

class SoftI2C {
public:
    /**
     * @param sdaPin  GPIO for SDA line
     * @param sclPin  GPIO for SCL line
     * @param delayUs Bit timing delay in microseconds (default 5µs ≈ ~100kHz)
     */
    SoftI2C(uint8_t sdaPin, uint8_t sclPin, uint32_t delayUs = 5)
        : _sda(sdaPin), _scl(sclPin), _delay(delayUs),
          _rxIndex(0), _rxCount(0) {}

    /** Initialize pin modes. Call in setup(). */
    inline void begin() {
        pinMode(_sda, INPUT_PULLUP);
        pinMode(_scl, INPUT_PULLUP);
        delayMicroseconds(_delay * 2);
    }

    /**
     * Start a write transaction to the given 7-bit address.
     * @return true if device ACKed its address
     */
    inline bool beginTransmission(uint8_t addr) {
        _start();
        return _writeByte((addr << 1) | 0x00);  // R/W = 0 (write)
    }

    /** Write a single byte during an open transmission. */
    inline bool write(uint8_t data) {
        return _writeByte(data);
    }

    /** End the current write transaction with a STOP condition. */
    inline void endTransmission() {
        _stop();
    }

    /**
     * Read `qty` bytes from the given 7-bit address.
     * Data is buffered internally — retrieve with read().
     * @return number of bytes actually read
     */
    inline uint8_t requestFrom(uint8_t addr, uint8_t qty) {
        _rxIndex = 0;
        _rxCount = 0;

        _start();
        if (!_writeByte((addr << 1) | 0x01)) {  // R/W = 1 (read)
            _stop();
            return 0;
        }

        for (uint8_t i = 0; i < qty; i++) {
            bool sendAck = (i < qty - 1);  // ACK all bytes except the last (NACK)
            _rxBuf[i] = _readByte(sendAck);
            _rxCount++;
        }

        _stop();
        return _rxCount;
    }

    /** Number of bytes remaining in the read buffer. */
    inline int available() {
        return _rxCount - _rxIndex;
    }

    /** Read next byte from buffer. Returns 0xFF if empty. */
    inline uint8_t read() {
        if (_rxIndex < _rxCount) return _rxBuf[_rxIndex++];
        return 0xFF;
    }

private:
    uint8_t  _sda, _scl;
    uint32_t _delay;
    uint8_t  _rxBuf[32];     // Internal read buffer (32 bytes max)
    uint8_t  _rxIndex, _rxCount;

    // Open-drain I/O helpers
    // SDA/SCL are driven LOW or released (input pullup = HIGH)
    inline void _sdaHigh() { pinMode(_sda, INPUT_PULLUP); }
    inline void _sdaLow()  { pinMode(_sda, OUTPUT); digitalWrite(_sda, LOW); }
    inline void _sclHigh() { pinMode(_scl, INPUT_PULLUP); delayMicroseconds(_delay); }
    inline void _sclLow()  { pinMode(_scl, OUTPUT); digitalWrite(_scl, LOW); delayMicroseconds(_delay); }

    /** Generate START condition: SDA falls while SCL is HIGH */
    inline void _start() {
        _sdaHigh();
        _sclHigh();
        _sdaLow();   // SDA ↓ while SCL is HIGH = START
        _sclLow();
    }

    /** Generate STOP condition: SDA rises while SCL is HIGH */
    inline void _stop() {
        _sdaLow();
        _sclHigh();
        _sdaHigh();  // SDA ↑ while SCL is HIGH = STOP
        delayMicroseconds(_delay);
    }

    /**
     * Clock out 8 bits MSB-first, then read ACK/NACK from slave.
     * @return true if slave sent ACK (SDA LOW on 9th clock)
     */
    inline bool _writeByte(uint8_t data) {
        for (int8_t i = 7; i >= 0; i--) {
            if (data & (1 << i)) _sdaHigh();
            else                 _sdaLow();
            _sclHigh();
            _sclLow();
        }
        // 9th clock — read ACK bit
        _sdaHigh();        // Release SDA for slave to drive
        _sclHigh();
        bool ack = (digitalRead(_sda) == LOW);
        _sclLow();
        return ack;
    }

    /**
     * Clock in 8 bits MSB-first, then send ACK or NACK.
     * @param ack  true = send ACK (SDA LOW), false = send NACK (SDA HIGH)
     */
    inline uint8_t _readByte(bool ack) {
        uint8_t data = 0;
        _sdaHigh();  // Release SDA for slave to drive
        for (int8_t i = 7; i >= 0; i--) {
            _sclHigh();
            if (digitalRead(_sda)) data |= (1 << i);
            _sclLow();
        }
        // 9th clock — send ACK/NACK
        if (ack) _sdaLow();   // ACK = pull SDA LOW
        else     _sdaHigh();  // NACK = leave SDA HIGH
        _sclHigh();
        _sclLow();
        _sdaHigh();  // Release SDA
        return data;
    }
};

#endif // SOFT_I2C_H
