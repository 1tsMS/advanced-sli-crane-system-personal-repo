// ============================================================
//  G-Code Style Command Parser
//  Parses incoming serial commands from PC into MotorCommand
//  or calibration actions.
//
//  Supported commands:
//    T0              → Emergency stop
//    M1 S200 D1      → Swing motor, speed 200, direction 1
//    M2 S150 D0      → Boom lift, speed 150, direction 0
//    M3 S100 D1      → Telescope, speed 100, direction 1
//    M4 S200 D0      → Winch, speed 200, direction 0
//    M0 A1           → Stop axis 1 (swing)
//    CAL0            → Tare load cell
//    CAL1            → Calibrate IMU
//    CAL2            → Reset telescope encoder
//    DBG             → Request debug/I2C scan report
//    CFG RATE 50     → Set telemetry rate
// ============================================================
#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include "config.h"

/**
 * Result of parsing a G-code command line.
 */
enum ParseResult {
    PARSE_OK,
    PARSE_EMPTY,
    PARSE_UNKNOWN_CMD,
    PARSE_INVALID_PARAMS
};

/**
 * The type of command that was parsed.
 */
enum CommandType {
    CMD_NONE,
    CMD_ESTOP,         // T0
    CMD_MOTOR,         // M1-M4
    CMD_STOP_AXIS,     // M0 Ax
    CMD_CALIBRATE,     // CAL0, CAL1, CAL2
    CMD_DEBUG,         // DBG
    CMD_CONFIG         // CFG
};

/**
 * Parsed command structure containing all extracted fields.
 */
struct ParsedCommand {
    CommandType type;
    MotorCommand motor;       // Filled for CMD_MOTOR / CMD_STOP_AXIS / CMD_ESTOP
    CalCommand   calType;     // Filled for CMD_CALIBRATE
    uint16_t     configRate;  // Filled for CMD_CONFIG (telemetry rate Hz)
    int8_t       boomAxis;    // Filled for CAL_ZERO_IMU (0=X, 1=Y, 2=Z)
    int8_t       boomInv;     // 0 or 1
    int8_t       tiltAxis;    // 0=X, 1=Y, 2=Z
    int8_t       tiltInv;     // 0 or 1
};

class GCodeParser {
public:
    GCodeParser() {}

    /**
     * Parse a single line of G-code text into a ParsedCommand.
     * @param line  Null-terminated string (e.g., "M1 S200 D1\n")
     * @param out   Parsed result
     * @return PARSE_OK on success
     */
    ParseResult parse(const char* line, ParsedCommand* out);

private:
    /**
     * Extract an integer parameter from the line.
     * Looks for prefix char (e.g., 'S') followed by a number.
     * @return the number, or defaultVal if not found
     */
    int extractParam(const char* line, char prefix, int defaultVal);
};

#endif // GCODE_PARSER_H
