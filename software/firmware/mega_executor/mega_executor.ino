// ============================================================
//  Arduino Mega — Motor Executor Firmware
//  Receives G-code commands from ESP32 via Serial1 (UART)
//  and controls 3 stepper motors via RAMPS 1.4 + A4988 drivers.
//
//  This is a "dumb" motor driver — all safety logic lives on ESP32.
//  The Mega just translates G-code into step/dir pin pulses.
//
//  RAMPS 1.4 Pin Mapping:
//    X-axis (Swing):     STEP=54, DIR=55, EN=38
//    Y-axis (Telescope):  STEP=60, DIR=61, EN=56
//    Z-axis (Boom Lift): STEP=46, DIR=48, EN=62
// ============================================================

#include <Arduino.h>
#include <ctype.h>

// ======================== RAMPS 1.4 PIN DEFINITIONS ========================
// X-axis: Swing
#define SWING_STEP  54
#define SWING_DIR   55
#define SWING_EN    38

// Y-axis: Telescope (M3)
#define TELE_STEP   60
#define TELE_DIR    61
#define TELE_EN     56

// Z-axis: Boom Lift (M2)
#define LIFT_STEP   46
#define LIFT_DIR    48
#define LIFT_EN     62

// ======================== MOTOR STATE ========================
struct StepperState {
    uint8_t stepPin;
    uint8_t dirPin;
    uint8_t enPin;
    bool    running;
    uint8_t direction;
    uint32_t speed;        // Delay between steps in microseconds (uint16 overflowed below ~16 steps/s)
    unsigned long lastStepTime;
    unsigned long lastCmdMs;   // millis() of the last M-command (dead-man watchdog)
};

// Dead-man: an axis keeps stepping only while its M-command keeps arriving
// (the dashboard repeats it every 100 ms while held). Same value as the ESP32.
#define MOTION_TIMEOUT_MS  400

StepperState motors[3] = {
    {SWING_STEP, SWING_DIR, SWING_EN, false, 0, 1000, 0, 0},  // M1 = Swing
    {LIFT_STEP,  LIFT_DIR,  LIFT_EN,  false, 0, 1000, 0, 0},  // M2 = Lift
    {TELE_STEP,  TELE_DIR,  TELE_EN,  false, 0, 1000, 0, 0}   // M3 = Telescope
};

// ======================== SERIAL BUFFERS ========================
// One line buffer per port: with a shared buffer, a byte arriving on USB
// in the middle of an ESP32 command would be spliced into that command.
struct LineBuffer {
    char buf[64];
    int  len;
};
LineBuffer espLine = { {0}, 0 };
LineBuffer usbLine = { {0}, 0 };

// ======================== SETUP ========================
void setup() {
    Serial.begin(115200);   // USB (for debug, optional)
    Serial1.begin(9600);    // UART from ESP32 (Pin 18=TX1, Pin 19=RX1)

    // Configure stepper pins
    for (int i = 0; i < 3; i++) {
        pinMode(motors[i].stepPin, OUTPUT);
        pinMode(motors[i].dirPin,  OUTPUT);
        pinMode(motors[i].enPin,   OUTPUT);
        digitalWrite(motors[i].enPin, LOW);  // Enable drivers (active LOW)
    }

    Serial.println("Mega Executor Ready");
    Serial1.println("$MEGA,READY");
}

// ======================== SERIAL INPUT ========================
void pollSerial(HardwareSerial& port, LineBuffer& line) {
    while (port.available()) {
        char c = port.read();
        if (c == '\n' || c == '\r') {
            if (line.len > 0) {
                line.buf[line.len] = '\0';
                processCommand(line.buf);
                line.len = 0;
            }
        } else if (line.len < (int)sizeof(line.buf) - 1) {
            line.buf[line.len++] = c;
        }
    }
}

// ======================== MAIN LOOP ========================
void loop() {
    // 1. Commands from the ESP32 (UART1), and from USB for direct debugging
    pollSerial(Serial1, espLine);
    pollSerial(Serial, usbLine);

    // 2. Dead-man watchdog: stop any axis whose command stream went quiet
    unsigned long nowMs = millis();
    for (int i = 0; i < 3; i++) {
        if (motors[i].running && (nowMs - motors[i].lastCmdMs) > MOTION_TIMEOUT_MS) {
            motors[i].running = false;
            Serial.print("WATCHDOG stopped axis "); Serial.println(i + 1);
        }
    }

    // 3. Step any running motors using non-blocking timing
    unsigned long now = micros();
    for (int i = 0; i < 3; i++) {
        if (motors[i].running && (now - motors[i].lastStepTime >= motors[i].speed)) {
            digitalWrite(motors[i].stepPin, HIGH);
            delayMicroseconds(2);  // Minimum pulse width for A4988
            digitalWrite(motors[i].stepPin, LOW);
            // Schedule from the ideal time, not "now", so loop jitter doesn't
            // slow the axis down; resync if we fell more than one step behind.
            motors[i].lastStepTime += motors[i].speed;
            if (now - motors[i].lastStepTime >= motors[i].speed) motors[i].lastStepTime = now;
        }
    }
}

// ======================== COMMAND PARSER ========================
void processCommand(const char* cmd) {
    // --- T0: Emergency Stop ---
    if (cmd[0] == 'T' && cmd[1] == '0') {
        emergencyStop();
        Serial.println("E-STOP TRIGGERED");
        Serial1.println("$ACK,ESTOP");
        return;
    }

    // --- M0 Ax: Stop specific axis ---
    if (cmd[0] == 'M' && cmd[1] == '0') {
        int axis = extractParam(cmd, 'A', 0);
        if (axis >= 1 && axis <= 3) {
            motors[axis - 1].running = false;
            Serial.print("Stopped axis "); Serial.println(axis);
        }
        return;
    }

    // --- M1-M3: Motor commands ---
    if (cmd[0] == 'M' && cmd[1] >= '1' && cmd[1] <= '3') {
        int axis = cmd[1] - '0';       // 1, 2, or 3
        int speed = extractParam(cmd, 'S', 0);
        int dir   = extractParam(cmd, 'D', 0);

        int idx = axis - 1;  // Array index (0-based)

        // Convert speed (steps/sec) to step delay in microseconds
        // e.g. Speed 500 = 2000µs delay (matches RAMPS_test.ino)
        if (speed <= 0) {
            motors[idx].running = false;
            return;
        }

        const bool wasRunning = motors[idx].running;
        motors[idx].speed = 1000000UL / (unsigned long)speed;
        motors[idx].direction = dir;
        motors[idx].running = true;
        motors[idx].lastCmdMs = millis();   // feed the dead-man watchdog
        if (!wasRunning) motors[idx].lastStepTime = micros();

        // Set direction pin
        digitalWrite(motors[idx].dirPin, dir ? HIGH : LOW);

        if (!wasRunning) {   // repeats arrive every 100 ms: only log starts
            Serial.print("M"); Serial.print(axis);
            Serial.print(" S"); Serial.print(speed);
            Serial.print(" D"); Serial.println(dir);
        }
    }
}

// ======================== EMERGENCY STOP ========================
void emergencyStop() {
    for (int i = 0; i < 3; i++) {
        motors[i].running = false;
        // Optionally disable drivers for immediate coast-down:
        // digitalWrite(motors[i].enPin, HIGH);  // HIGH = disabled
    }
}

// ======================== PARAMETER EXTRACTION ========================
int extractParam(const char* line, char prefix, int defaultVal) {
    char target = toupper(prefix);
    const char* p = line;
    while (*p) {
        if (toupper(*p) == target) {
            p++;
            while (*p == ' ') p++;
            if ((*p >= '0' && *p <= '9') || *p == '-') return atoi(p);
        }
        p++;
    }
    return defaultVal;
}

