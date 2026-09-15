#define FSR1 34
#define FSR2 35
#define FSR3 36
#define FSR4 39

void setup() {
  Serial.begin(115200);
  Serial.println("FSR test — press each sensor individually");
}

void loop() {
  int f1 = analogRead(FSR1);
  int f2 = analogRead(FSR2);
  int f3 = analogRead(FSR3);
  int f4 = analogRead(FSR4);

  Serial.print("FSR1: "); Serial.print(f1);
  Serial.print("  FSR2: "); Serial.print(f2);
  Serial.print("  FSR3: "); Serial.print(f3);
  Serial.print("  FSR4: "); Serial.println(f4);

  delay(200);
}