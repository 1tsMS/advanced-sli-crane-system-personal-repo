#include <Wire.h>

#define AS5600_ADDR 0x36

bool detectAS5600(int sda, int scl)
{
  Wire.end();
  delay(5);

  if (!Wire.begin(sda, scl, 100000)) {
    return false;
  }

  Wire.beginTransmission(AS5600_ADDR);
  return (Wire.endTransmission() == 0);
}

uint16_t readAngle(int sda, int scl)
{
  Wire.end();
  delay(5);
  Wire.begin(sda, scl, 100000);

  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0C);

  if (Wire.endTransmission(false) != 0)
    return 0xFFFF;

  if (Wire.requestFrom(AS5600_ADDR, 2) != 2)
    return 0xFFFF;

  uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();

  return raw & 0x0FFF;
}

void printSensor(const char* name, int sda, int scl)
{
  uint16_t raw = readAngle(sda, scl);

  Serial.print(name);
  Serial.print(": ");

  if (raw == 0xFFFF) {
    Serial.println("NOT DETECTED");
  }
  else {
    float angle = raw * 360.0 / 4096.0;

    Serial.print(angle, 2);
    Serial.println(" deg");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("=== 3x AS5600 TEST ===");
}

void loop()
{
  printSensor("Swing      (21/22)", 21, 22);
  printSensor("Boom Lift  (25/26)", 25, 26);
  printSensor("Telescope  (32/33)", 32, 33);

  Serial.println("-------------------------");

  delay(200);
}