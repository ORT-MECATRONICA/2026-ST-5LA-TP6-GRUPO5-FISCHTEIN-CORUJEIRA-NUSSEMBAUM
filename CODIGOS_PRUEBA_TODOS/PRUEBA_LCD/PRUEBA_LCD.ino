#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

LiquidCrystal_PCF8574 lcd(0x27);

void setup() {
  Serial.begin(115200);
  Wire1.begin(16, 13);   // SDA, SCL

  lcd.begin(16, 2, Wire1);   // acá se le pasa el bus
  lcd.setBacklight(255);

  lcd.setCursor(0, 0);
  lcd.print("Hola ESP32!");
  lcd.setCursor(0, 1);
  lcd.print("LCD I2C Funciona");
}

void loop() {}