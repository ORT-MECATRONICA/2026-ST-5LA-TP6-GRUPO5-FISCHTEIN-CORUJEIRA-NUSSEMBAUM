#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;  // I2C

void setup() {
  Serial.begin(9600);
  Serial.println(F("BMP280 test"));

  // Change to 0x76 if your sensor uses that address
  if (!bmp.begin(0x76)) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1)
      ;
  }

  /* Default settings from datasheet. */
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
    Adafruit_BMP280::STANDBY_MS_500
  ); /* Standby time. */
}

void loop() {
  Serial.print(F("Temperature = "));
  Serial.print(bmp.readTemperature());
  Serial.println(" *C");

  Serial.print(F("Pressure = "));
  Serial.print(bmp.readPressure() / 100.0);  // converting to hPa
  Serial.println(" hPa");

  Serial.print(F("Approx altitude = "));
  Serial.print(bmp.readAltitude(1013.25));  // Adjusted to local sealevel pressure in hPa
  Serial.println(" m");

  Serial.println();
  delay(2000);
}