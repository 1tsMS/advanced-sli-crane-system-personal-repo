#include <Wire.h>

// --- RAMPS 1.4 Stepper Pins ---
#define SWING_STEP 54
#define SWING_DIR 55
#define SWING_EN 38

#define LIFT_STEP 60
#define LIFT_DIR 61
#define LIFT_EN 56

#define TELE_STEP 46
#define TELE_DIR 48
#define TELE_EN 62

// --- N20 DC Winch Pins (RAMPS Servo Header) ---
#define WINCH_FWD 11
#define WINCH_REV 6

// --- AS5600 I2C Address ---
#define AS5600_ADDR 0x36

// Continuous Run States
bool runSwing = false;
bool runLift = false;
bool runTele = false;

// Speed Variables
int stepDelay = 1000; // Stepper delay in microseconds (lower = faster)
int winchSpeed = 255; // Winch PWM speed (0 - 255)

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50); // Fast timeout for reading numbers
  Wire.begin(); 
  
  // Configure Pins
  pinMode(SWING_STEP, OUTPUT); pinMode(SWING_DIR, OUTPUT); pinMode(SWING_EN, OUTPUT);
  pinMode(LIFT_STEP, OUTPUT); pinMode(LIFT_DIR, OUTPUT); pinMode(LIFT_EN, OUTPUT);
  pinMode(TELE_STEP, OUTPUT); pinMode(TELE_DIR, OUTPUT); pinMode(TELE_EN, OUTPUT);
  pinMode(WINCH_FWD, OUTPUT); pinMode(WINCH_REV, OUTPUT);
  
  // Enable all A4988 drivers (Active LOW)
  digitalWrite(SWING_EN, LOW);
  digitalWrite(LIFT_EN, LOW);
  digitalWrite(TELE_EN, LOW);
  
  Serial.println("--- Continuous Run Test Ready ---");
  Serial.println("SPEED CONTROLS:");
  Serial.println("  V <num> : Set Stepper Delay (us). Example: V 500");
  Serial.println("  B <num> : Set Winch PWM (0-255). Example: B 150");
  Serial.println("MOVEMENT CONTROLS (Continuous):");
  Serial.println("  Q / W : Start Swing Left / Right");
  Serial.println("  A / S : Start Lift Up / Down");
  Serial.println("  Z / C : Start Tele Out / In");
  Serial.println("  E / R : Start Winch Up / Down");
  Serial.println("  P     : Read AS5600 Encoder");
  Serial.println("  X     : STOP ALL MOTORS");
}

void loop() {
  // 1. Check for incoming Serial commands
  if (Serial.available() > 0) {
    char cmd = toupper(Serial.read());
    int val = Serial.parseInt(); // Read number if present

    // --- Emergency Stop ---
    if (cmd == 'X' || cmd == ' ') {
      runSwing = runLift = runTele = false;
      analogWrite(WINCH_FWD, 0); analogWrite(WINCH_REV, 0);
      Serial.println("ALL MOTORS STOPPED.");
    }
    
    // --- Speed Adjustments ---
    else if (cmd == 'V') { 
      if (val > 0) stepDelay = val; 
      Serial.print("Stepper Delay updated to: "); Serial.println(stepDelay); 
    }
    else if (cmd == 'B') {
      if (val >= 0 && val <= 255) winchSpeed = val;
      Serial.print("Winch Speed updated to: "); Serial.println(winchSpeed);
    }

    // --- Swing (Continuous) ---
    else if (cmd == 'Q') { runSwing = true; digitalWrite(SWING_DIR, HIGH); Serial.println("Swinging Left..."); }
    else if (cmd == 'W') { runSwing = true; digitalWrite(SWING_DIR, LOW); Serial.println("Swinging Right..."); }
    
    // --- Lift (Continuous) ---
    else if (cmd == 'A') { runLift = true; digitalWrite(LIFT_DIR, HIGH); Serial.println("Lifting Up..."); }
    else if (cmd == 'S') { runLift = true; digitalWrite(LIFT_DIR, LOW); Serial.println("Lifting Down..."); }
    
    // --- Telescope (Continuous) ---
    else if (cmd == 'Z') { runTele = true; digitalWrite(TELE_DIR, HIGH); Serial.println("Extending..."); }
    else if (cmd == 'C') { runTele = true; digitalWrite(TELE_DIR, LOW); Serial.println("Retracting..."); }

    // --- Winch (Continuous) ---
    else if (cmd == 'E') { analogWrite(WINCH_FWD, winchSpeed); analogWrite(WINCH_REV, 0); Serial.println("Winching Up..."); }
    else if (cmd == 'R') { analogWrite(WINCH_FWD, 0); analogWrite(WINCH_REV, winchSpeed); Serial.println("Winching Down..."); }

    // --- AS5600 Encoder Read ---
    else if (cmd == 'P') {
      readEncoder();
    }
  }

  // 2. Execute Stepper Steps Continuously
  if (runSwing) stepMotor(SWING_STEP);
  if (runLift) stepMotor(LIFT_STEP);
  if (runTele) stepMotor(TELE_STEP);
}

// Function to pulse a stepper pin
void stepMotor(int stepPin) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(stepDelay);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(stepDelay);
}

// Function to read raw angle from AS5600
void readEncoder() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0E); // Raw Angle Register (High Byte)
  if (Wire.endTransmission() != 0) {
    Serial.println("Error: AS5600 Encoder not found on I2C.");
    return;
  }
  
  Wire.requestFrom(AS5600_ADDR, 2);
  if (Wire.available() <= 2) {
    int highByte = Wire.read();
    int lowByte = Wire.read();
    int rawAngle = (highByte << 8) | lowByte;
    Serial.print("AS5600 Raw Angle: ");
    Serial.println(rawAngle);
  }
}