// ============================================================
//  Telescope AS5600 Live Calibration & Tuning Tool
//  For Advanced SLI — ESP32 (GPIO 32=SDA, GPIO 33=SCL)
//
//  Upload this sketch to test your telescope encoder in isolation.
//  It will show you:
//    1. Live raw angle & continuous revolutions (with glitch filter)
//    2. One-step calibration: move a known distance (e.g. 30mm or 50mm)
//       and type 'd30' or 'd50' in Serial Monitor. It will calculate
//       your EXACT mm/revolution and inversion flag!
// ============================================================

#include <Arduino.h>

#define SDA_PIN  32
#define SCL_PIN  33
#define AS5600_ADDR 0x36

// Bit timing delay in microseconds (~60kHz for clean edges)
#define I2C_DELAY_US 8

// --- Minimal Soft I2C Bit-Bang ---
void i2c_init() {
    pinMode(SDA_PIN, INPUT_PULLUP);
    pinMode(SCL_PIN, INPUT_PULLUP);
    delayMicroseconds(20);
}

void i2c_sdaHigh() { pinMode(SDA_PIN, INPUT_PULLUP); }
void i2c_sdaLow()  { pinMode(SDA_PIN, OUTPUT); digitalWrite(SDA_PIN, LOW); }
void i2c_sclHigh() { pinMode(SCL_PIN, INPUT_PULLUP); delayMicroseconds(I2C_DELAY_US); }
void i2c_sclLow()  { pinMode(SCL_PIN, OUTPUT); digitalWrite(SCL_PIN, LOW); delayMicroseconds(I2C_DELAY_US); }

void i2c_start() {
    i2c_sdaHigh();
    i2c_sclHigh();
    i2c_sdaLow();
    i2c_sclLow();
}

void i2c_stop() {
    i2c_sdaLow();
    i2c_sclHigh();
    i2c_sdaHigh();
    delayMicroseconds(I2C_DELAY_US);
}

bool i2c_writeByte(uint8_t data) {
    for (int8_t i = 7; i >= 0; i--) {
        if (data & (1 << i)) i2c_sdaHigh();
        else                 i2c_sdaLow();
        i2c_sclHigh();
        i2c_sclLow();
    }
    i2c_sdaHigh();
    i2c_sclHigh();
    bool ack = (digitalRead(SDA_PIN) == LOW);
    i2c_sclLow();
    return ack;
}

uint8_t i2c_readByte(bool ack) {
    uint8_t data = 0;
    i2c_sdaHigh();
    for (int8_t i = 7; i >= 0; i--) {
        i2c_sclHigh();
        if (digitalRead(SDA_PIN)) data |= (1 << i);
        i2c_sclLow();
    }
    if (ack) i2c_sdaLow();
    else     i2c_sdaHigh();
    i2c_sclHigh();
    i2c_sclLow();
    i2c_sdaHigh();
    return data;
}

uint16_t readAS5600Raw() {
    i2c_start();
    if (!i2c_writeByte((AS5600_ADDR << 1) | 0x00)) {
        i2c_stop();
        return 0xFFFF;
    }
    // Register 0x0E = Filtered Angle (AS5600 internal digital filter)
    if (!i2c_writeByte(0x0E)) {
        i2c_stop();
        return 0xFFFF;
    }
    i2c_stop();

    i2c_start();
    if (!i2c_writeByte((AS5600_ADDR << 1) | 0x01)) {
        i2c_stop();
        return 0xFFFF;
    }
    uint8_t msb = i2c_readByte(true);
    uint8_t lsb = i2c_readByte(false);
    i2c_stop();

    if (msb == 0xFF && lsb == 0xFF) return 0xFFFF;
    return (((uint16_t)msb << 8) | lsb) & 0x0FFF;
}

// --- Tracking State ---
uint16_t zeroOffset = 0;
float    lastAngle = 0.0f;
float    continuousAngle = 0.0f;
bool     firstRead = true;
uint32_t glitchCount = 0;

// Calibration variables
float scale_mm_per_rev = 19.048f;
bool  invertDirection  = true;

void resetZero() {
    uint16_t raw = readAS5600Raw();
    if (raw != 0xFFFF) zeroOffset = raw;
    lastAngle = 0.0f;
    continuousAngle = 0.0f;
    firstRead = true;
    glitchCount = 0;
    Serial.println("\n>>> [ZEROED] Current position set to 0.0 mm <<<");
}

void updateAngle() {
    uint16_t raw = readAS5600Raw();
    if (raw == 0xFFFF) return; // Discard communication failure

    int16_t corrected = (int16_t)raw - (int16_t)zeroOffset;
    if (corrected < 0) corrected += 4096;
    float currentAngle = (corrected / 4096.0f) * 360.0f;

    if (firstRead) {
        lastAngle = currentAngle;
        firstRead = false;
        return;
    }

    float delta = currentAngle - lastAngle;
    if (delta < -180.0f) delta += 360.0f;
    else if (delta > 180.0f) delta -= 360.0f;

    // Glitch rejection: Discard impossible jumps (>45° in one sample)
    if (fabs(delta) > 45.0f) {
        glitchCount++;
        return;
    }

    continuousAngle += delta;
    lastAngle = currentAngle;
}

void printHelp() {
    Serial.println("\n--- Commands available in Serial Monitor ---");
    Serial.println("  z           : Zero telescope at current position (retracted)");
    Serial.println("  d<distance> : Calibrate with actual measured mm (e.g. d30, d50)");
    Serial.println("  s<scale>    : Set custom scale in mm/rev (e.g. s19.05)");
    Serial.println("  i           : Toggle invert direction (currently: " + String(invertDirection ? "INVERTED" : "NORMAL") + ")");
    Serial.println("  h           : Print this help\n");
}

void processSerialInput() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    char action = tolower(cmd[0]);
    if (action == 'z') {
        resetZero();
    } else if (action == 'i') {
        invertDirection = !invertDirection;
        Serial.print(">>> Invert direction is now: ");
        Serial.println(invertDirection ? "INVERTED (true)" : "NORMAL (false)");
    } else if (action == 'h') {
        printHelp();
    } else if (action == 's') {
        float val = cmd.substring(1).toFloat();
        if (val > 0.1f) {
            scale_mm_per_rev = val;
            Serial.print(">>> Scale updated to: ");
            Serial.print(scale_mm_per_rev, 4);
            Serial.println(" mm/rev");
        }
    } else if (action == 'd' || action == 'm') {
        float actual_mm = cmd.substring(1).toFloat();
        if (actual_mm <= 0.1f) {
            Serial.println(">>> Error: Please enter a positive physical distance, e.g. 'd30' or 'd50'");
            return;
        }

        float revs = continuousAngle / 360.0f;
        if (fabs(revs) < 0.05f) {
            Serial.println(">>> Error: Encoder has not moved enough to calibrate (< 0.05 rev). Move further first!");
            return;
        }

        bool neededInvert = (revs < 0);
        float newScale = actual_mm / fabs(revs);

        Serial.println("\n=======================================================");
        Serial.println("             CALIBRATION RESULT CALCULATED             ");
        Serial.println("=======================================================");
        Serial.print("  Actual Physical Travel : "); Serial.print(actual_mm, 2); Serial.println(" mm");
        Serial.print("  Encoder Total Angle    : "); Serial.print(continuousAngle, 2); Serial.println("°");
        Serial.print("  Total Revolutions      : "); Serial.println(revs, 4);
        Serial.print("  Calculated Scale       : "); Serial.print(newScale, 4); Serial.println(" mm/revolution");
        Serial.print("  Required Inversion     : "); Serial.println(neededInvert ? "true" : "false");
        Serial.println("-------------------------------------------------------");
        Serial.println("To use this in your main firmware config.h:");
        Serial.println("  #define TELE_MM_PER_REVOLUTION  " + String(newScale, 4) + "f");
        Serial.println("  #define TELE_DEFAULT_INVERT     " + String(neededInvert ? "true" : "false"));
        Serial.println("=======================================================\n");

        scale_mm_per_rev = newScale;
        invertDirection = neededInvert;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("   AS5600 TELESCOPE TUNING & CALIBRATION TOOL     ");
    Serial.println("==================================================");
    Serial.println("Hardware: ESP32 GPIO 32 (SDA), GPIO 33 (SCL)");
    i2c_init();

    uint16_t probe = readAS5600Raw();
    if (probe == 0xFFFF) {
        Serial.println("\n[!] WARNING: AS5600 NOT DETECTED ON GPIO 32/33!");
        Serial.println("    Check your 3.3V, GND, and wiring on pins 32 & 33.");
    } else {
        Serial.println("[OK] AS5600 detected! Initial angle: " + String(probe * 360.0f / 4096.0f, 1) + " deg");
    }

    printHelp();
    resetZero();
}

unsigned long lastPrintTime = 0;

void loop() {
    updateAngle();
    processSerialInput();

    // Print live status every 150ms
    unsigned long now = millis();
    if (now - lastPrintTime >= 150) {
        lastPrintTime = now;

        uint16_t raw = readAS5600Raw();
        float revs = continuousAngle / 360.0f;
        float signedRevs = invertDirection ? -revs : revs;
        float extensionMM = signedRevs * scale_mm_per_rev;

        Serial.print("Raw: ");
        if (raw == 0xFFFF) Serial.print("ERR ");
        else {
            if (raw < 1000) Serial.print(" ");
            if (raw < 100) Serial.print(" ");
            Serial.print(raw);
        }

        Serial.print(" | Angle: ");
        Serial.print(lastAngle, 1);
        Serial.print("° | Cont: ");
        if (continuousAngle >= 0) Serial.print("+");
        Serial.print(continuousAngle, 1);
        Serial.print("° | Revs: ");
        if (signedRevs >= 0) Serial.print("+");
        Serial.print(signedRevs, 3);
        Serial.print(" | Ext: ");
        if (extensionMM >= 0) Serial.print("+");
        Serial.print(extensionMM, 1);
        Serial.print(" mm");

        if (glitchCount > 0) {
            Serial.print(" | [Spikes Filtered: ");
            Serial.print(glitchCount);
            Serial.print("]");
        }

        Serial.println();
    }

    delay(10); // 100Hz update loop matching main firmware
}
