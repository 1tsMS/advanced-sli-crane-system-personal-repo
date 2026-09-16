#include <Wire.h>
#include <HX711.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------------- HX711 ----------------
#define HX711_DT 17
#define HX711_SCK 16

HX711 scale;

// ---------------- MPU6050 ----------------
Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // ================= HX711 =================
  Serial.println("Initializing HX711...");

  scale.begin(HX711_DT, HX711_SCK);

  if (scale.is_ready()) {
    Serial.println("HX711 detected!");
  } else {
    Serial.println("HX711 NOT detected!");
  }

  // ================= MPU6050 =================
  Serial.println("Initializing MPU6050...");

  Wire.begin(21, 22);  // SDA = 21, SCL = 22

  if (!mpu.begin()) {
    Serial.println("MPU6050 NOT detected!");
    while (1) {
      delay(10);
    }
  }

  Serial.println("MPU6050 detected!");

  // Set MPU6050 ranges
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("Sensors ready!\n");
}

void loop() {

  // ================= HX711 =================
  if (scale.is_ready()) {
    long reading = scale.read();

    Serial.print("HX711 Raw: ");
    Serial.println(reading);
  } else {
    Serial.println("HX711: Not ready");
  }

  // ================= MPU6050 =================
  sensors_event_t a, g, temp;

  mpu.getEvent(&a, &g, &temp);

  Serial.print("Accel X: ");
  Serial.print(a.acceleration.x);
  Serial.print(" | Y: ");
  Serial.print(a.acceleration.y);
  Serial.print(" | Z: ");
  Serial.print(a.acceleration.z);
  Serial.println(" m/s^2");

  Serial.print("Gyro X: ");
  Serial.print(g.gyro.x);
  Serial.print(" | Y: ");
  Serial.print(g.gyro.y);
  Serial.print(" | Z: ");
  Serial.print(g.gyro.z);
  Serial.println(" rad/s");

  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" °C");

  Serial.println("-----------------------------");

  delay(500);
}