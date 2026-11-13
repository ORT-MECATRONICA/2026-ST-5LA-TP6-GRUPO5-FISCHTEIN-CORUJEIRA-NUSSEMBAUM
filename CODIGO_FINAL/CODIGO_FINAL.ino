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
#define RX 16
#define TX 17
#define LDR 34
#define GAS 35

#define LED_1 25
#define LED_2 26

//Declaración de ID y Token del chat de Telegram
#define BOTtoken "8919782216:AAHZLJSktuKHWctRGJ7UylZtetMbH0j7KOw"
#define CHAT_ID "6195445889"
#define TASA_TELERGRAM 5000
unsigned long lastMeasure1 = 0;

Estados estadoSensado = MIDIENDO;
TaskHandle_t TareaSensado;
TaskHandle_t TareaComunicacion;

//Inicio de WiFi y el bot según su token y ID
char* ssid = "MECA-IoT-V2";
char* password = "IoT$2026";
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

volatile float tempActual;
float tempUmbral = 22;
volatile bool presencia = false;
volatile float corriente;
float corrienteUmbral = 20;
volatile float luz;
float luzUmbral = 60;
volatile float gas;
float gasUmbral = 60;

bool alertaTempEnviada = false;
bool alertaPresenciaEnviada = false;
bool alertaCorrienteEnviada = false;
bool alertaLuzEnviada = false;
bool alertaGasEnviada = false;

float medirTemp();
float medirCorriente();
bool medirPresencia();
float medirLuz();
float medirGas();

void codigoSensado(void *parameter);
void codigoComunicacion(void *parameter);
void handleNewMessages(int num);

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
  radar.begin(RADAR_SERIAL);
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
  lastMeasure1 = millis();
}

void codigoSensado(void* parameter) {
  while (1) {
    tempActual = medirTemp();
    presencia = medirPresencia();
    corriente = medirCorriente();
    luz = medirLuz();
    gas = medirGas();;
  }
}

void codigoComunicacion(void* parameter) {
  while(1) {
    // Evaluación de Temperatura con histéresis básica por bandera
    if (tempActual >= tempUmbral && !alertaTempEnviada) {
      enviarTelegram("ALERTA! TEMPERATURA ELEVADA: " + String(tempActual) + " C");
      alertaTempEnviada = true;
    } else if (tempActual < tempUmbral - 1.0) {
      alertaTempEnviada = false;
    }

    // Evaluación de Presencia
    if (presencia && !alertaPresenciaEnviada) {
      enviarTelegram("ALERTA! PRESENCIA DETECTADA");
      alertaPresenciaEnviada = true;
    } else if (!presencia) {
      alertaPresenciaEnviada = false;
    }

    // Evaluación de Corriente
    if (corriente >= corrienteUmbral && !alertaCorrienteEnviada) {
      enviarTelegram("ALERTA! CORRIENTE ELEVADA: " + String(corriente) + " mA");
      alertaCorrienteEnviada = true;
    } else if (corriente < corrienteUmbral - 2.0) {
      alertaCorrienteEnviada = false;
    }

    // Evaluación de Luz
    if (luz >= luzUmbral && !alertaLuzEnviada) {
      enviarTelegram("ALERTA! LUZ ELEVADA: " + String(luz) + "%");
      alertaLuzEnviada = true;
    } else if (luz < luzUmbral - 5.0) {
      alertaLuzEnviada = false;
    }

    // Evaluación de Gas
    if (gas >= gasUmbral && !alertaGasEnviada) {
      enviarTelegram("ALERTA! GAS ELEVADO: " + String(gas) + "%");
      alertaGasEnviada = true;
    } else if (gas < gasUmbral - 5.0) {
      alertaGasEnviada = false;
    }

    // Revisión periódica de mensajes entrantes en Telegram
    if (millis() - lastMeasure1 >= TASA_TELEGRAM) {
      int nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
      while (nuevosMensajes) {
        handleNewMessages(nuevosMensajes);
        nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
      }
      lastMeasure1 = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(250)); // Pausa para no bloquear el Watchdog
  }
}

float medirTemp() {
  return bmp.readTemperature();
}

float medirCorriente() {
  return ina219.getCurrent_mA();
}

bool medirPresencia() {
  radar.read();

  static unsigned long tiempoAnterior = 0;
  static bool presencia = false;

  if (millis() - tiempoAnterior >= 500) {
    tiempoAnterior = millis();

    if (radar.isConnected()) {
      presencia = radar.presenceDetected();
    }
  }

  return presencia;
}

float medirLuz() {
  return map(analogRead(LDR), 0, 4095, 0, 100);
}

float medirGas() {
  return map(analogRead(GAS), 0, 4095, 0, 100);
}

void enviarTelegram(String mensaje) {
  bot.sendMessage(CHAT_ID, mensaje);
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/state") {

      bot.sendMessage(
        "Temperatura: " + String(tempActual) + " C\n"
        + "Presencia: " + (presencia ? "Detectada\n" : "No detectada\n")
        + "Corriente: " + String(corriente) + " mA\n"
        + "Luz: " + String(luz) + "%\n"
        + "Gas: " + String(gas) + "%"
      );
    }
  }
}
