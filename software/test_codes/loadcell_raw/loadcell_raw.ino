#include "HX711.h"

#define DT  17
#define SCK 16

HX711 scale;

long zero = 0;

void setup() {
  Serial.begin(115200);

  scale.begin(DT, SCK);

  Serial.println("HX711 LOAD CELL TEST");
  Serial.println("Remove all load...");
  delay(2000);

  // Take 30 samples for zero
  long sum = 0;
  int samples = 0;

  while (samples < 30) {
    if (scale.is_ready()) {
      sum += scale.read();
      samples++;
      delay(50);
    }
  }

  zero = sum / 30;

  Serial.print("ZERO = ");
  Serial.println(zero);
  Serial.println("Press the load cell now.");
  Serial.println();
}

void loop() {

  if (scale.is_ready()) {

    long raw = scale.read();
    long delta = raw - zero;

    Serial.print("RAW: ");
    Serial.print(raw);

    Serial.print("   DELTA: ");
    Serial.println(delta);
  }

  delay(200);
}