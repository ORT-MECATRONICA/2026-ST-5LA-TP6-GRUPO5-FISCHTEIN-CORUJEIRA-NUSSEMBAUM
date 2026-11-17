#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ld2410.h>
#include <Adafruit_INA219.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#include <WiFi.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h> 
#include <UniversalTelegramBot.h>

// ================= CONFIGURACIÓN WIFI Y DISPOSITIVO =================
const char* ssid = "PACE";
const char* password = "pace2026";

const uint8_t name_device = 115;

// ================= INSTANCIAS DE SENSORES Y PERIFÉRICOS =================
Adafruit_BMP280 bmp(&Wire1);
Adafruit_INA219 ina219;
LiquidCrystal_I2C lcd(0x27, 16, 2);
ld2410 radar;

#define RADAR_SERIAL Serial1
#define RX 16
#define TX 17

// Pines originales restaurados
#define LDR 3
#define GAS_1 2
#define GAS_2 4

#define LED_1 26
#define LED_2 27

// ================= CONFIGURACIÓN TELEGRAM =================
#define BOTtoken "8919782216:AAHZLJSktuKHWctRGJ7UylZtetMbH0j7KOw"
#define CHAT_ID "6195445889"
#define TASA_TELEGRAM 5000
unsigned long lastTelegramCheck = 0;

WiFiClientSecure clientTelegram;
UniversalTelegramBot bot(BOTtoken, clientTelegram);

// ================= TIMERS Y NTP PARA MQTT =================
unsigned long now = 0;
unsigned long lastMeasure1 = 0; 
unsigned long lastMeasure2 = 0; 

const unsigned long interval_envio = 30000; 
const unsigned long interval_leeo = 60000;  

const char* ntpServer = "south-america.pool.ntp.org";
const long gmtOffset_sec = -10800;
const int daylightOffset_sec = 0;

// ================= COLA CIRCULAR (STRUCT) Y MQTT =================
int indice_entra = 0;
int indice_saca = 0;
bool flag_vacio = 1;

#define MQTT_HOST "sa11b75f.ala.us-east-1.emqxsl.com"
#define MQTT_PORT 8883
#define MQTT_USERNAME "esp32"
#define MQTT_PASSWORD "mirko15"
#define MQTT_PUB "/esp32/datos_sensores"

char mqtt_payload[150];

typedef struct {
  long time;
  float T1;     // temp en grados
  float P1;     // valor presion en hPa
  float luz;    // valor entre 0 y 99
  float G1;     // valor gas 1 entre 0 y 99
  float G2;     // valor gas 2 entre 0 y 99
  float cor;    // valor corriente en mA
  bool pre;     // Lectura sensor de presencia
  bool Alarma;  // 1 si se supera algún umbral
} estructura;

const int valor_max_struct = 1000;
estructura datos_struct[valor_max_struct];
estructura aux2;

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// ================= VARIABLES DE ESTADO Y SENSADO =================
TaskHandle_t TareaSensado;
TaskHandle_t TareaComunicacion;

volatile float tempActual = 0;
float tempUmbral = 22;

volatile float presionActual = 0;
float presionUmbral = 1025;

volatile bool presencia = false;

volatile float corriente = 0;
float corrienteUmbral = 20;

volatile float luz = 0;
float luzUmbral = 60;

volatile float gas1 = 0;
float gas1Umbral = 60;

volatile float gas2 = 0;
float gas2Umbral = 60;

bool alertaTempEnviada = false;
bool alertaPresionEnviada = false;
bool alertaPresenciaEnviada = false;
bool alertaCorrienteEnviada = false;
bool alertaLuzEnviada = false;
bool alertaGas1Enviada = false;
bool alertaGas2Enviada = false;

// ================= PROTOTIPOS DE FUNCIONES =================
float medirTemp();
float medirPresion();
float medirCorriente();
bool medirPresencia();
float medirLuz();
float medirGas1();
float medirGas2();

void codigoSensado(void *parameter);
void codigoComunicacion(void *parameter);
void handleNewMessages(int numNewMessages);

void inicializarPerifericos();
void enviarTelegram(String mensaje);
void setupmqtt();
void connectToWifi();
void connectToMqtt();
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
  pinMode(GAS_1, INPUT);
  pinMode(GAS_2, INPUT);
  pinMode(LDR, INPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(LED_2, OUTPUT);

  Wire.begin(21, 22); 
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  RADAR_SERIAL.begin(256000, SERIAL_8N1, RX, TX);
  radar.begin(RADAR_SERIAL);
  Serial.println("Sensor de presencia inicializado");

  while (!ina219.begin()) {
    Serial.println("Iniciando sensor de corriente...");
    delay(500);
  }
  Serial.println("Sensor de corriente inicializado");

  Wire1.begin(18, 19); 
  while (!bmp.begin(0x76, BMP280_CHIPID)) {
    Serial.println("Iniciando sensor BMP280...");
    delay(500);
  }
  Serial.println("Sensor BMP280 inicializado");

  WiFi.mode(WIFI_STA);
  connectToWifi();

  clientTelegram.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  espClient.setInsecure(); 
  setupmqtt();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  xTaskCreatePinnedToCore(codigoSensado, "sensado", 10000, NULL, 1, &TareaSensado, 0);
  xTaskCreatePinnedToCore(codigoComunicacion, "comunicacion", 10000, NULL, 1, &TareaComunicacion, 1);

  bot.sendMessage(CHAT_ID, "Bot iniciado correctamente", "");
  lastTelegramCheck = millis();
  lastMeasure1 = millis();
  lastMeasure2 = millis();
  lcd.clear();
  lcd.print("Sistema Online");
}

// ================= TAREAS FREERTOS =================
void codigoSensado(void* parameter) {
  while (1) {
    tempActual = medirTemp();
    presionActual = medirPresion();
    presencia = medirPresencia();
    corriente = medirCorriente();
    luz = medirLuz();
    gas1 = medirGas1();
    gas2 = medirGas2();
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void codigoComunicacion(void* parameter) {
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      connectToWifi();
    }
    if (!mqttClient.connected()) {
      connectToMqtt();
    }
    mqttClient.loop(); 

    if (tempActual >= tempUmbral && !alertaTempEnviada) {
      enviarTelegram("ALERTA! TEMPERATURA ELEVADA: " + String(tempActual) + " C");
      alertaTempEnviada = true;
    } else if (tempActual < tempUmbral - 1.0) {
      alertaTempEnviada = false;
    }

    if (presionActual >= presionUmbral && !alertaPresionEnviada) {
      enviarTelegram("ALERTA! PRESION ELEVADA: " + String(presionActual) + " hPa");
      alertaPresionEnviada = true;
    } else if (presionActual < presionUmbral - 2.0) {
      alertaPresionEnviada = false;
    }

    if (presencia && !alertaPresenciaEnviada) {
      enviarTelegram("ALERTA! PRESENCIA DETECTADA");
      alertaPresenciaEnviada = true;
    } else if (!presencia) {
      alertaPresenciaEnviada = false;
    }

    if (corriente >= corrienteUmbral && !alertaCorrienteEnviada) {
      enviarTelegram("ALERTA! CORRIENTE ELEVADA: " + String(corriente) + " mA");
      alertaCorrienteEnviada = true;
    } else if (corriente < corrienteUmbral - 2.0) {
      alertaCorrienteEnviada = false;
    }

    if (luz >= luzUmbral && !alertaLuzEnviada) {
      enviarTelegram("ALERTA! LUZ ELEVADA: " + String(luz) + "%");
      alertaLuzEnviada = true;
    } else if (luz < luzUmbral - 5.0) {
      alertaLuzEnviada = false;
    }

    if (gas1 >= gas1Umbral && !alertaGas1Enviada) {
      enviarTelegram("ALERTA! GAS 1 ELEVADO: " + String(gas1) + "%");
      alertaGas1Enviada = true;
    } else if (gas1 < gas1Umbral - 5.0) {
      alertaGas1Enviada = false;
    }

    if (gas2 >= gas2Umbral && !alertaGas2Enviada) {
      enviarTelegram("ALERTA! GAS 2 ELEVADO: " + String(gas2) + "%");
      alertaGas2Enviada = true;
    } else if (gas2 < gas2Umbral - 5.0) {
      alertaGas2Enviada = false;
    }

    if (millis() - lastTelegramCheck >= TASA_TELEGRAM) {
      int nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
      while (nuevosMensajes) {
        handleNewMessages(nuevosMensajes);
        nuevosMensajes = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTelegramCheck = millis();
    }

    now = millis();
    if (now - lastMeasure2 >= interval_leeo) {
      lastMeasure2 = now;
      fun_entra(); 
    }
    if (now - lastMeasure1 >= interval_envio) {
      lastMeasure1 = now;
      fun_envio_mqtt(); 
    }

    vTaskDelay(pdMS_TO_TICKS(50)); 
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

float medirGas1() {
  return map(analogRead(GAS_1), 0, 4095, 0, 99);
}

float medirGas2() {
  return map(analogRead(GAS_2), 0, 4095, 0, 99);
}

// ================= FUNCIONES DE TELEGRAM =================
void enviarTelegram(String mensaje) {
  bot.sendMessage(CHAT_ID, mensaje, "");
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    if (text == "/state") {
      String msg = "Temperatura: " + String(tempActual) + " C\n"
                 + "Presion: " + String(presionActual) + " hPa\n"
                 + "Presencia: " + (presencia ? "Detectada\n" : "No detectada\n")
                 + "Corriente: " + String(corriente) + " mA\n"
                 + "Luz: " + String(luz) + "%\n"
                 + "Gas 1: " + String(gas1) + "%\n"
                 + "Gas 2: " + String(gas2) + "%";
      bot.sendMessage(chat_id, msg, "");
    }
  }
}

// ================= FUNCIONES MQTT Y COLA DE DATOS =================
void setupmqtt() {
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
}

void fun_entra() {
  time_t now_ntp;
  time(&now_ntp); 

  datos_struct[indice_entra].time = (long)now_ntp;
  datos_struct[indice_entra].T1 = tempActual;
  datos_struct[indice_entra].P1 = presionActual;
  datos_struct[indice_entra].luz = luz;
  datos_struct[indice_entra].G1 = gas1;
  datos_struct[indice_entra].G2 = gas2;
  datos_struct[indice_entra].cor = corriente;
  datos_struct[indice_entra].pre = presencia;
  datos_struct[indice_entra].Alarma = (alertaTempEnviada || alertaPresionEnviada || 
                                       alertaPresenciaEnviada || alertaCorrienteEnviada || 
                                       alertaLuzEnviada || alertaGas1Enviada || alertaGas2Enviada);

  if (indice_entra >= (valor_max_struct - 1)) {
    indice_entra = 0;
  } else {
    indice_entra++;
  }
  Serial.print("Dato guardado en struct. Nuevo indice_entra: ");
  Serial.println(indice_entra);
}

void fun_saca() {
  if (indice_saca != indice_entra) {
    aux2 = datos_struct[indice_saca]; 
    flag_vacio = 0;

    if (indice_saca >= (valor_max_struct - 1)) {
      indice_saca = 0;
    } else {
      indice_saca++;
    }
    Serial.print("Dato extraido de struct. Nuevo indice_saca: ");
    Serial.println(indice_saca);
  } else {
    flag_vacio = 1;
  }
}

void fun_envio_mqtt() {
  fun_saca();
  if (flag_vacio == 0) {
    snprintf(mqtt_payload, sizeof(mqtt_payload), 
             "%d&%ld&%.2f&%.2f&%.2f&%.2f&%.2f&%.2f&%u&%u", 
             name_device, aux2.time, aux2.T1, aux2.P1, aux2.luz, 
             aux2.G1, aux2.G2, aux2.cor, aux2.pre, aux2.Alarma);

    Serial.print("Publish message: ");
    Serial.println(mqtt_payload);

    mqttClient.publish(MQTT_PUB, mqtt_payload);
    memset(&aux2, 0, sizeof(aux2)); 
  } else {
    Serial.println("No hay valores nuevos en cola");
  }
}

void connectToWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("Conectando a Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());
}

void connectToMqtt() {
  if (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT (EMQX Cloud SSL)...");
    String clientId = "ESP32_Grupo_" + String(name_device) + "_" + WiFi.macAddress();
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println(" ¡Conectado a MQTT!");
    } else {
      Serial.print(" Fallo conexion, rc=");
      Serial.println(mqttClient.state());
    }
  }
}