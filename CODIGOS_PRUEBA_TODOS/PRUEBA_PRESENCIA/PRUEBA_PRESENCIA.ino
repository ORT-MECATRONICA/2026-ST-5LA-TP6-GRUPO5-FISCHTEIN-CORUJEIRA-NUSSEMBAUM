#include <ld2410.h>

// Definimos los pines para el puerto Serial2 del ESP32
#define RX2_PIN 16  // Va al TX del HLK
#define TX2_PIN 17  // Va al RX del HLK

ld2410 radar;

void setup() {
  // Inicializa el Monitor Serie de la PC
  Serial.begin(115200);   
  delay(500);
  
  Serial.println("\n--- Iniciando ESP32 con Radar HLK ---");

  // Inicializa el puerto HardwareSerial2 a 256000 bps con los pines elegidos
  Serial2.begin(256000, SERIAL_8N1, RX2_PIN, TX2_PIN);

  // Pasamos el puerto Serial2 a la librería del radar
  if (radar.begin(Serial2)) {
    Serial.println("¡Sensor HLK detectado correctamente en Serial2!");
  } else {
    Serial.println("Error: No se pudo comunicar con el sensor. Revisá el cableado.");
    while (1); 
  }
}

void loop() {
  // Alimenta el búfer de la librería con los datos que llegan por hardware
  radar.read();

  // Si el sensor está conectado y enviando tramas válidas
  if (radar.isConnected()) {
    static unsigned long ultimoCambio = 0;
    
    // Imprimimos datos cada 500ms para no saturar el Monitor Serie
    if (millis() - ultimoCambio > 500) {
      ultimoCambio = millis();

      Serial.println("\n=============================");
      if (radar.presenceDetected()) {
        Serial.println("ESTADO: [ PRESENCIA DETECTADA ]");
        
        if (radar.movingTargetDetected()) {
          Serial.print("-> Objetivo en movimiento a: ");
          Serial.print(radar.movingTargetDistance());
          Serial.print(" cm (Energía: ");
          Serial.print(radar.movingTargetEnergy());
          Serial.println(")");
        }
        
        if (radar.stationaryTargetDetected()) {
          Serial.print("-> Objetivo estático a: ");
          Serial.print(radar.stationaryTargetDistance());
          Serial.print(" cm (Energía: ");
          Serial.print(radar.stationaryTargetEnergy());
          Serial.println(")");
        }
      } else {
        Serial.println("ESTADO: [ ZONA VACÍA ]");
      }
    }
  }
}
