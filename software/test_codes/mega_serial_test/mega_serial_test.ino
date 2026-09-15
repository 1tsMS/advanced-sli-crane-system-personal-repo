void setup() {
  Serial.begin(115200);   // Mega USB
  Serial1.begin(9600);    // TX1=18, RX1=19

  Serial.println("Mega ready");
}

void loop() {
  // Mega USB -> ESP32
  while (Serial.available()) {
    Serial1.write(Serial.read());
  }

  // ESP32 -> Mega USB
  while (Serial1.available()) {
    Serial.write(Serial1.read());
  }
}