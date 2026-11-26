//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;  // I2C

void setup() {
  Serial.begin(115200);
  Serial.println("BMP280 test");

  Wire.begin();

  // Change to 0x76 if your sensor uses that address
  while (!bmp.begin(0x76,BMP280_CHIPID)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    delay(500);
  }

  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X2,
    Adafruit_BMP280::SAMPLING_X16,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_500
  );
}

void loop() {
  Serial.print(F("Temperatura = "));
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");

  Serial.println();
  delay(100);
}