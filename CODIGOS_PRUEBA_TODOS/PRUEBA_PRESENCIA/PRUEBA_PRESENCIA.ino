//Grupo 5: Sol Corujeira, Benjamin Fischtein y Lara Nussembaum

#include <ld2410.h>

ld2410 radar;

#define RADAR_SERIAL Serial1
#define RX 16
#define TX 17
unsigned long tiempoAnterior = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  while (!Serial)
    ;
  RADAR_SERIAL.begin(256000, SERIAL_8N1,RX,TX);

  if (radar.begin(RADAR_SERIAL)) {
    Serial.println("Sensor conectado");
  } else {
    Serial.println("Error al encontrar el sensor");
    while (1)
      ;  // Detiene la ejecución si falla
  }
}

void loop() {
  radar.read();

  if(millis() - tiempoAnterior >= 500){

    tiempoAnterior = millis();
    if(radar.isConnected()){
      if(radar.presenceDetected()){
        Serial.println("PRESENCIA DETECTADA");
      } else{
        Serial.println("NADA");
      }
    }
  }
}
