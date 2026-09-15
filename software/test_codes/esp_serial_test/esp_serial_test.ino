#define RX2 23
#define TX2 27

HardwareSerial MegaSerial(2);

void setup() {
  Serial.begin(115200);  // ESP32 USB
  MegaSerial.begin(9600, SERIAL_8N1, RX2, TX2);

  Serial.println("ESP32 ready");
}

void loop() {
  // ESP32 USB -> Mega
  while (Serial.available()) {
    MegaSerial.write(Serial.read());
  }

  // Mega -> ESP32 USB
  while (MegaSerial.available()) {
    Serial.write(MegaSerial.read());
  }
}