//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum

#include <LiquidCrystal_I2C.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x3F, 16, 2); 

void setup() {
  Wire.begin(21,22);
  lcd.init(); 
  lcd.backlight();     
  lcd.setCursor(0, 0); 
  lcd.print("Hola ESP32!"); 
  
  lcd.setCursor(0, 1); 
  lcd.print("LCD I2C Funciona");
}

void loop() {
}