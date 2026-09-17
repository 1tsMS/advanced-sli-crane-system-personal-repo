// RAMPS 1.4 - 3 Motor Test
// Arduino Mega
// Motors: X, Y, Z

#define X_STEP 54
#define X_DIR  55
#define X_EN   38

#define Y_STEP 60
#define Y_DIR  61
#define Y_EN   56

#define Z_STEP 46
#define Z_DIR  48
#define Z_EN   62

// Selected motor
int selectedMotor = 1;

// Speed in steps/second
unsigned long speed = 500;

bool running = false;
bool direction = true;

unsigned long lastStepMicros = 0;


void setup() {

  Serial.begin(115200);

  pinMode(X_STEP, OUTPUT);
  pinMode(X_DIR, OUTPUT);
  pinMode(X_EN, OUTPUT);

  pinMode(Y_STEP, OUTPUT);
  pinMode(Y_DIR, OUTPUT);
  pinMode(Y_EN, OUTPUT);

  pinMode(Z_STEP, OUTPUT);
  pinMode(Z_DIR, OUTPUT);
  pinMode(Z_EN, OUTPUT);

  // Enable drivers
  digitalWrite(X_EN, LOW);
  digitalWrite(Y_EN, LOW);
  digitalWrite(Z_EN, LOW);

  setDirection();

  Serial.println();
  Serial.println("=== RAMPS 3 MOTOR TEST ===");
  Serial.println("1 = X Motor");
  Serial.println("2 = Y Motor");
  Serial.println("3 = Z Motor");
  Serial.println("Sxxx = Set speed (steps/sec)");
  Serial.println("F = Forward");
  Serial.println("R = Reverse");
  Serial.println("X = Stop");
  Serial.println("A = Run ALL motors");
  Serial.println();
}


void loop() {

  readSerial();

  if (!running)
    return;

  unsigned long interval = 1000000UL / speed;

  if (micros() - lastStepMicros >= interval) {

    lastStepMicros = micros();

    stepSelectedMotor();
  }
}


// -------------------------
// SERIAL COMMANDS
// -------------------------

void readSerial() {

  if (!Serial.available())
    return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.length() == 0)
    return;

  // Motor selection
  if (cmd == "1") {

    selectedMotor = 1;
    running = false;

    Serial.println("Selected X motor");
  }

  else if (cmd == "2") {

    selectedMotor = 2;
    running = false;

    Serial.println("Selected Y motor");
  }

  else if (cmd == "3") {

    selectedMotor = 3;
    running = false;

    Serial.println("Selected Z motor");
  }

  // Forward
  else if (cmd == "F") {

    direction = true;
    setDirection();

    running = true;

    Serial.println("Forward");
  }

  // Reverse
  else if (cmd == "R") {

    direction = false;
    setDirection();

    running = true;

    Serial.println("Reverse");
  }

  // Stop
  else if (cmd == "X") {

    running = false;

    Serial.println("STOP");
  }

  // Run all
  else if (cmd == "A") {

    running = true;

    Serial.println("Running ALL motors");
  }

  // Speed
  else if (cmd.charAt(0) == 'S') {

    int newSpeed = cmd.substring(1).toInt();

    if (newSpeed > 0) {

      speed = newSpeed;

      Serial.print("Speed = ");
      Serial.print(speed);
      Serial.println(" steps/sec");
    }
  }

  else {

    Serial.println("Unknown command");
  }
}


// -------------------------
// STEP SELECTED MOTOR
// -------------------------

void stepSelectedMotor() {

  if (selectedMotor == 1) {

    digitalWrite(X_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(X_STEP, LOW);
  }

  else if (selectedMotor == 2) {

    digitalWrite(Y_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(Y_STEP, LOW);
  }

  else if (selectedMotor == 3) {

    digitalWrite(Z_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(Z_STEP, LOW);
  }
}


// -------------------------
// DIRECTION
// -------------------------

void setDirection() {

  digitalWrite(X_DIR, direction);
  digitalWrite(Y_DIR, direction);
  digitalWrite(Z_DIR, direction);
}