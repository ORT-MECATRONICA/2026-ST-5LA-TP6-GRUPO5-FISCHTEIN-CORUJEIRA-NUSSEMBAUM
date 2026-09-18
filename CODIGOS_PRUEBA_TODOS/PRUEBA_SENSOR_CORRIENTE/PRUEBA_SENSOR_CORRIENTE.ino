//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum

#include <Adafruit_INA219.h>
#include <Wire.h>

Adafruit_INA219 ina219;

void setup() {
  Serial.begin(115200);

  while(!ina219.begin()){
    Serial.println("Iniciando sensor");
    delay(100);
  }
  Serial.println("Sensor iniciado");
}

void loop() {
  float corriente = ina219.getCurrent_mA();

  Serial.println("Corriente en mA: " + String(corriente));
}
