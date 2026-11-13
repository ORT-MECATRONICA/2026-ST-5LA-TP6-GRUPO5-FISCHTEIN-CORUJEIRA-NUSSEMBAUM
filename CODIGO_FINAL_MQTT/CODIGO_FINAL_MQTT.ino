#include <Arduino.h>
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
#include "AsyncMqttClient.h"

// ================= CONFIGURACIÓN WIFI Y DISPOSITIVO =================
const char* ssid = "MECA-IoT-V2";
const char* password = "IoT$2026";

const uint8_t name_device = 111; ////device numero de grupo 5A 11x siendo x el numero de grupo
                                 ///                        5B 12x siendo x el numero de grupo
                                 /// 5a grupo 4 => 114
                                 /// 5b grupo 3 => 123
                                 /// 5C grupo 3 => 133

#define EEPROM_SIZE 4

// ================= INSTANCIAS DE SENSORES Y PERIFÉRICOS =================
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

// ================= CONFIGURACIÓN TELEGRAM =================
#define BOTtoken "8919782216:AAHZLJSktuKHWctRGJ7UylZtetMbH0j7KOw"
#define CHAT_ID "6195445889"
#define TASA_TELEGRAM 5000
unsigned long lastTelegramCheck = 0;

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// ================= TIMERS Y NTP PARA MQTT =================
unsigned long now = 0;
unsigned long lastMeasure1 = 0; // Timer para envío MQTT
unsigned long lastMeasure2 = 0; // Timer para guardado en cola struct

const unsigned long interval_envio = 30000; // Intervalo de envío de datos MQTT (30 s)
const unsigned long interval_leeo = 60000;  // Intervalo de lectura y guardado en cola (60 s)

long unsigned int timestamp;
const char* ntpServer = "south-america.pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

// ================= COLA CIRCULAR (STRUCT) Y MQTT =================
int indice_entra = 0;
int indice_saca = 0;
bool flag_vacio = 1;

#define MQTT_HOST IPAddress(192, 168, 10, 151) /// Cambiar por IP del broker si corresponde
#define MQTT_PORT 1884
#define MQTT_USERNAME "esp32"
#define MQTT_PASSWORD "mirko15"
#define MQTT_PUB "/esp32/datos_sensores"

char mqtt_payload[150];
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

typedef struct {
  long time;
  float T1;    /// temp en grados
  float P1;    /// valor presion en hPa
  float luz;   /// valor entre 0 y 99
  float G1;    /// valor gas entre 0 y 99
  float cor;   /// valor corriente en mA
  bool pre;    /// Lectura sensor de presencia (1 presencia, 0 no presencia)
  bool Alarma; /// 1 si se supera algún umbral
} estructura;

const int valor_max_struct = 1000;
estructura datos_struct[valor_max_struct];
estructura aux2;

// ================= VARIABLES DE ESTADO Y SENSADO =================
enum Estados {
  MIDIENDO,
  EVALUANDO
};
Estados estadoSensado = MIDIENDO;

TaskHandle_t TareaSensado;
TaskHandle_t TareaComunicacion;

volatile float tempActual = 0;
float tempUmbral = 22;

volatile float presionActual = 0;
float presionUmbral = 1025; // Umbral en hPa (presión normal nivel del mar ~1013 hPa)

volatile bool presencia = false;

volatile float corriente = 0;
float corrienteUmbral = 20;

volatile float luz = 0;
float luzUmbral = 60;

volatile float gas = 0;
float gasUmbral = 60;

bool alertaTempEnviada = false;
bool alertaPresionEnviada = false;
bool alertaPresenciaEnviada = false;
bool alertaCorrienteEnviada = false;
bool alertaLuzEnviada = false;
bool alertaGasEnviada = false;

// ================= PROTOTIPOS DE FUNCIONES =================
float medirTemp();
float medirPresion();
float medirCorriente();
bool medirPresencia();
float medirLuz();
float medirGas();

void codigoSensado(void *parameter);
void codigoComunicacion(void *parameter);
void handleNewMessages(int numNewMessages);

void inicializarPerifericos();
void enviarTelegram(String mensaje);

// Prototipos MQTT y Cola
void setupmqtt();
void connectToWifi();
void connectToMqtt();
void WiFiEvent(WiFiEvent_t event);
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttPublish(uint16_t packetId);
void fun_saca();
void fun_entra();
void fun_envio_mqtt();

// ================= SETUP & LOOP =================
void setup() {
  inicializarPerifericos();
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// ================= INICIALIZACIÓN =================
void inicializarPerifericos() {
  Serial.begin(115200);
  pinMode(GAS, INPUT);
  pinMode(LDR, INPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);

  Wire.begin(21, 22); // Bus I2C0 para LCD e INA219
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

  Wire1.begin(18, 19); // Bus I2C1 para BMP280

  while (!bmp.begin(0x76, BMP280_CHIPID)) {
    Serial.println("Iniciando sensor de temperatura y presion");
    delay(100);
  }
  Serial.println("Sensor BMP280 inicializado");

  // Configuración de WiFi, Eventos y MQTT
  WiFi.mode(WIFI_STA);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  setupmqtt();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Conectando a red WiFi...");
  }
  Serial.println("Conectado a red WiFi");

  // Inicializar reloj NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Crear tareas en núcleos separados
  xTaskCreatePinnedToCore(codigoSensado, "sensado", 10000, NULL, 0, &TareaSensado, 0);
  xTaskCreatePinnedToCore(codigoComunicacion, "comunicacion", 10000, NULL, 0, &TareaComunicacion, 1);

  bot.sendMessage(CHAT_ID, "Bot started up", "");
  lastTelegramCheck = millis();
  lastMeasure1 = millis();
  lastMeasure2 = millis();
}

// ================= TAREAS FREERTOS =================
void codigoSensado(void* parameter) {
  while (1) {
    tempActual = medirTemp();
    presionActual = medirPresion();
    presencia = medirPresencia();
    corriente = medirCorriente();
    luz = medirLuz();
    gas = medirGas();
  }
}

void codigoComunicacion(void* parameter) {
  while (1) {
    // Evaluación de Temperatura
    if (tempActual >= tempUmbral && !alertaTempEnviada) {
      enviarTelegram("ALERTA! TEMPERATURA ELEVADA: " + String(tempActual) + " C");
      alertaTempEnviada = true;
    } else if (tempActual < tempUmbral - 1.0) {
      alertaTempEnviada = false;
    }

    // Evaluación de Presión
    if (presionActual >= presionUmbral && !alertaPresionEnviada) {
      enviarTelegram("ALERTA! PRESION ELEVADA: " + String(presionActual) + " hPa");
      alertaPresionEnviada = true;
    } else if (presionActual < presionUmbral - 2.0) {
      alertaPresionEnviada = false;
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
    if (millis() - lastTelegramCheck >= TASA_TELEGRAM) {
      int nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
      while (nuevosMensajes) {
        handleNewMessages(nuevosMensajes);
        nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTelegramCheck = millis();
    }

    // Lógica de Cola y Envío MQTT
    now = millis();
    if (now - lastMeasure1 > interval_envio) {
      lastMeasure1 = now;
      fun_envio_mqtt(); 
    }
    if (now - lastMeasure2 > interval_leeo) {
      lastMeasure2 = now;
      fun_entra();
    }
  }
}

// ================= FUNCIONES DE LECTURA DE SENSORES =================
float medirTemp() {
  return bmp.readTemperature();
}

float medirPresion() {
  return bmp.readPressure() / 100.0F;
}

float medirCorriente() {
  return ina219.getCurrent_mA();
}

bool medirPresencia() {
  radar.read();

  static unsigned long tiempoAnterior = 0;
  static bool estadoPresencia = false;

  if (millis() - tiempoAnterior >= 500) {
    tiempoAnterior = millis();

    if (radar.isConnected()) {
      estadoPresencia = radar.presenceDetected();
    }
  }

  return estadoPresencia;
}

float medirLuz() {
  return map(analogRead(LDR), 0, 4095, 0, 99);
}

float medirGas() {
  return map(analogRead(GAS), 0, 4095, 0, 99);
}

// ================= FUNCIONES DE TELEGRAM =================
void enviarTelegram(String mensaje) {
  bot.sendMessage(CHAT_ID, mensaje, "");
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/state") {
      String msg = "Temperatura: " + String(tempActual) + " C\n"
                 + "Presion: " + String(presionActual) + " hPa\n"
                 + "Presencia: " + (presencia ? "Detectada\n" : "No detectada\n")
                 + "Corriente: " + String(corriente) + " mA\n"
                 + "Luz: " + String(luz) + "%\n"
                 + "Gas: " + String(gas) + "%";
      bot.sendMessage(chat_id, msg, "");
    }
  }
}

// ================= FUNCIONES MQTT Y COLA DE DATOS =================
void setupmqtt() {
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));
  WiFi.onEvent(WiFiEvent);
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USERNAME, MQTT_PASSWORD);
  connectToWifi();
}

void fun_envio_mqtt() {
  fun_saca();           ////veo si hay valores nuevos
  if (flag_vacio == 0)  ////si hay los envio
  {
    Serial.print("enviando");
    snprintf(mqtt_payload, sizeof(mqtt_payload), 
         "%d&%ld&%.2f&%.2f&%.2f&%.2f&%.2f&%u&%u", 
         name_device, aux2.time, aux2.T1, aux2.P1, aux2.luz, 
         aux2.G1, aux2.cor, aux2.pre, aux2.Alarma);
    aux2.time = 0;
    aux2.T1 = 0;
    aux2.P1 = 0;
    aux2.luz = 0;
    aux2.G1 = 0;
    aux2.cor = 0;
    aux2.pre = 0;
    aux2.Alarma = 0;

    Serial.print("Publish message: ");
    Serial.println(mqtt_payload);
    uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB, 1, true, mqtt_payload);
  } else {
    Serial.println("no hay valores nuevos");
  }
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;
    default:
      break;
  }
}

////////////////NO TOCAR FUNCIONES MQTT///////////////////////////////////
void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

////////////////NO TOCAR FUNCIONES MQTT///////////////////////////////////
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

////////////////NO TOCAR FUNCIONES MQTT///////////////////////////////////
void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void fun_saca() {
  if (indice_saca != indice_entra) {
    aux2.time = datos_struct[indice_saca].time;
    aux2.T1 = datos_struct[indice_saca].T1;
    aux2.P1 = datos_struct[indice_saca].P1;
    aux2.luz = datos_struct[indice_saca].luz;
    aux2.G1 = datos_struct[indice_saca].G1;
    aux2.cor = datos_struct[indice_saca].cor;
    aux2.pre = datos_struct[indice_saca].pre;
    aux2.Alarma = datos_struct[indice_saca].Alarma;

    flag_vacio = 0;

    Serial.println(indice_saca);
    if (indice_saca >= (valor_max_struct - 1)) {
      indice_saca = 0;
    } else {
      indice_saca++;
    }
    Serial.print("saco valores de la struct isaca:");
    Serial.println(indice_saca);
  } else {
    flag_vacio = 1; ///// no hay datos
  }
  return;
}

void fun_entra(void) {
  if (indice_entra >= valor_max_struct) {
    indice_entra = 0; ///si llego al maximo de la cola se vuelve a cero
  }
  //////////// timestamp/////// consigo la hora
  Serial.print("> NTP Time:");
  timestamp = time(NULL);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return; //// si no puede conseguir la hora
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  ///////////////////////// fin de consigo la hora

  bool alarmaActiva = (tempActual >= tempUmbral) || 
                      (presionActual >= presionUmbral) || 
                      presencia || 
                      (corriente >= corrienteUmbral) || 
                      (luz >= luzUmbral) || 
                      (gas >= gasUmbral);

  datos_struct[indice_entra].time = timestamp;
  datos_struct[indice_entra].T1 = tempActual;
  datos_struct[indice_entra].P1 = presionActual;
  datos_struct[indice_entra].luz = luz;
  datos_struct[indice_entra].G1 = gas;
  datos_struct[indice_entra].cor = corriente;
  datos_struct[indice_entra].pre = presencia;
  datos_struct[indice_entra].Alarma = alarmaActiva;

  indice_entra++;
  Serial.print("guardo valores en la struct ientra: ");
  Serial.println(indice_entra);
}