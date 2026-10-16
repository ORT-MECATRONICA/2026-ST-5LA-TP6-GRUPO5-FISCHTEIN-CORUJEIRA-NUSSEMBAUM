#include <WiFiManager.h>
#include <strings_en.h>
#include <wm_consts_en.h>
#include <wm_strings_en.h>
#include <wm_strings_es.h>

#include <LiquidCrystal_I2C.h>
#include <ld2410.h>
#include <Adafruit_INA219.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#include <EEPROM.h>
#include <WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <UniversalTelegramBot.h>
#define WIFI_SSID "MECA-IoT-V2"
#define WIFI_PASSWORD "IoT$2026"

#define EEPROM_SIZE 4

Adafruit_BMP280 bmp(&Wire1);
Adafruit_INA219 ina219;
LiquidCrystal_I2C lcd(0x27, 16, 2);
ld2410 radar;

#define RADAR_SERIAL Serial1
#define RX 19
#define TX 25
#define LDR 15
#define GAS 12

#define LED_1 6
#define LED_2 7

//Declaración de ID y Token del chat de Telegram
#define BOTtoken "8919782216:AAHZLJSktuKHWctRGJ7UylZtetMbH0j7KOw"
#define CHAT_ID "6195445889"

enum Estados{
  MIDIENDO,
  EVALUANDO
};
Estados estadoSensado = MIDIENDO;
TaskHandle_t TareaSensado;
TaskHandle_t TareaComunicacion;

//Inicio de WiFi y el bot según su token y ID
char* ssid = "MECA-IoT-V2";
char* password = "IoT$2026";
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

float tempActual;
float tempUmbral = 22;
bool presencia = false;
float corriente;
float luz;
float gas;

float medirTemp();
float medirCorriente();
bool medirPresencia();
float medirLuz();
float medirGas();

void mostrarMensajes(String mensaje);
void inicializarPerifericos();
void configurarTelegram();
void enviarTelegram(String mensaje);
String leerTelegram();
void enviarMQTT();
void guardarEEPROM();
void configurarValores();
void iniciarGmt();

void setup() {
  inicializarPerifericos();
}

void loop() {
  
}

void inicializarPerifericos() {
  Serial.begin(115200);
  pinMode(GAS, INPUT);
  pinMode(LDR, INPUT);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  while (!Serial) {}
  RADAR_SERIAL.begin(256000, SERIAL_8N1, RX, TX);
  Serial.println("Sensor de presencia inicializado");

  while (!ina219.begin()) {
    Serial.println("Iniciando sensor de corriente");
    delay(100);
  }
  Serial.println("Sensor de corriente inicializado");

  Wire1.begin(18, 19);

  while (!bmp.begin(0x76, BMP280_CHIPID)) {
    Serial.println("Iniciado sensor de temperatura");
    delay(100);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando a red WiFi");
  }
  Serial.println("Conectado a red WiFi");


  xTaskCreatePinnedToCore(codigoSensado, "sensado", 10000, NULL, 1, &TareaSensado, 0);
  xTaskCreatePinnedToCore(codigoComunicacion, "comunicacion", 10000, NULL, 1, &TareaComunicacion, 1);

  bot.sendMessage(CHAT_ID, "Bot started up");
}

void codigoSensado(void *parameter){
  switch(estadoSensado){
    case MIDIENDO:
      break;

    case EVALUANDO:
      break;
  }
}

void codigoComunicacion(void *parameter){

}

float medirTemp(){
  return bmp.readTemperature();
}

float medirCorriente(){
  return ina219.getCurrent_mA();
}

bool medirPresencia(){
  radar.read();

  static unsigned long tiempoAnterior = 0;
  static bool presencia = false;

  if(millis() - tiempoAnterior >= 500){
    tiempoAnterior = millis();

    if(radar.isConnected()){
      presencia = radar.presenceDetected();
    }
  }

  return presencia;
}

float medirLuz(){
  return map(analogRead(LDR),0,4095,0,100);
}

float medirGas(){
  return map(analogRead(GAS),0,4095,0,100);
}

void enviarTelegram(String mensaje){
  bot.sendMessage(CHAT_ID, mensaje);
}