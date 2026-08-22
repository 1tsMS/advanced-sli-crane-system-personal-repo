#include <AccelStepper.h>

// RAMPS 1.4 X-axis pins
#define X_STEP_PIN 54
#define X_DIR_PIN  55
#define X_ENABLE_PIN 38

// STEP + DIR driver
AccelStepper stepper(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);

void setup()
{
  Serial.begin(115200);

  // Enable X stepper driver
  pinMode(X_ENABLE_PIN, OUTPUT);
  digitalWrite(X_ENABLE_PIN, LOW);

  // Motor speed and acceleration
  stepper.setMaxSpeed(1000);
  stepper.setAcceleration(500);

  Serial.println("Stepper test started");
}

void loop()
{
  // Move 2000 steps forward
  stepper.moveTo(2000);

  while (stepper.distanceToGo() != 0)
  {
    stepper.run();
  }

  delay(1000);

  // Move 2000 steps backward
  stepper.moveTo(0);

  while (stepper.distanceToGo() != 0)
  {
    stepper.run();
  }

  delay(1000);
}