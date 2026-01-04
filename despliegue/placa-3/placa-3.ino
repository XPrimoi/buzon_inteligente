#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ===== WIFI & MQTT CONFIGURATION =====
const char* ssid = "Add ssid";
const char* password = "Add password";
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

// ===== MQTT TOPICS =====
const char* mqttTopicIREvent = "buzon/ir";
const char* mqttTopicPressure = "buzon/presion";
const char* mqttTopicCollision = "buzon/colision/superior";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== PINS =====
#define PIN_PRESSURE 4
#define PIN_COLLISION 5
#define PIN_IR_EMITTER 6
#define PIN_IR_RECEIVER 8

// ===== SENSOR VBLES =====
int pressureValue = 0;
const int PRESSURE_THRESHOLD = 2000;
int lastPressureSent = 0;

bool collisionState = LOW;
bool previousCollisionState = LOW;

bool irReceiverState = LOW;
bool previousIRReceiverState = LOW;

bool irEnabled = false;

// ===== HELPER FUNCTIONS =====
void logMqttResult(bool published) {
  Serial.println(published ?  " [MQTT OK]" :  " [MQTT ERROR]");
}

String getTimestampISO8601() {
  time_t now;
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) return "1970-01-01T00:00:00Z";

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

// ===== PUBLISH FUNCTIONS =====
void publishPressure(int value) {
  bool limite = (value >= 4095);

  char jsonBuffer[64];
  sprintf(jsonBuffer, "{\"valor\":%d,\"limite\":%s}", value, limite ? "true" : "false");

  Serial.print("Presion: ");
  Serial.print(value);
  Serial.print(" | Limite: ");
  Serial.println(limite ? "true" : "false");

  bool published = false;
  if (client.connected()) {
    published = client. publish(mqttTopicPressure, jsonBuffer);
  }
  logMqttResult(published);
}

void publishCollision(bool doorOpen) {
  char status[8];
  sprintf(status, "%d", doorOpen ? 1 : 0);

  Serial.print("Puerta: ");
  Serial.println(doorOpen ? "ABIERTA" : "CERRADA");

  bool published = false;
  if (client.connected()) {
    published = client.publish(mqttTopicCollision, status);
  }
  logMqttResult(published);
}

void publishIREvent() {
  String timestamp = getTimestampISO8601();

  Serial.println("\n========== CARTA DETECTADA ==========");
  Serial.println(timestamp);
  Serial.println("=====================================\n");
  
  bool published = client.publish(mqttTopicIREvent, timestamp. c_str(), true);
  logMqttResult(published);
}

// ===== SENSOR READING FUNCTIONS =====
void readPressureSensor() {
  pressureValue = analogRead(PIN_PRESSURE);
  publishPressure(pressureValue);
  if (pressureValue > PRESSURE_THRESHOLD &&pressureValue != lastPressureSent) {
    Serial.println("\n[PRESION]");
    Serial.printf("Valor: %d\n", pressureValue);
    lastPressureSent = pressureValue;
  }
}

void readCollisionSensor() {
  int collisionReading = digitalRead(PIN_COLLISION);

  if (collisionReading != previousCollisionState) {
    collisionState = collisionReading;
    previousCollisionState = collisionState;

    Serial.println("\n[SENSOR COLISION]");
    Serial.printf("Cambio: %d -> %d\n", !collisionState, collisionState);

    if (collisionState == HIGH) {
      Serial.println("Estado: Puerta CERRADA");
      irEnabled = false;
    } else {
      Serial.println("Estado: Puerta ABIERTA");
      irEnabled = true;
    }

    publishCollision(collisionState == LOW);
  }
}

void readIRSensor() {
  if (!irEnabled) return;
  
  int irReading = digitalRead(PIN_IR_RECEIVER);

  if (irReading != previousIRReceiverState) {
    irReceiverState = irReading;
    previousIRReceiverState = irReceiverState;

    Serial.println("\n[RECEPTOR IR]");
    Serial.printf("Cambio: %d -> %d\n", !irReceiverState, irReceiverState);

    if (irReceiverState == HIGH) {
      Serial.println("Estado: SEÑAL DETECTADA");
      publishIREvent();
    }
  }
}

void initializePins() {
  pinMode(PIN_PRESSURE, INPUT);
  pinMode(PIN_COLLISION, INPUT_PULLDOWN);
  pinMode(PIN_IR_EMITTER, OUTPUT);
  pinMode(PIN_IR_RECEIVER, INPUT);
  
  digitalWrite(PIN_IR_EMITTER, HIGH);
}

void initializeSensors() {
  delay(100);
  collisionState = digitalRead(PIN_COLLISION);
  previousCollisionState = collisionState;
  pressureValue = analogRead(PIN_PRESSURE);
  irReceiverState = digitalRead(PIN_IR_RECEIVER);
  previousIRReceiverState = irReceiverState;
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n============ BUZON INTELIGENTE ============\n");
  
  initializePins();
  initializeSensors();
  
  // Conexión WiFi bloqueante
  WiFi.begin(ssid, password);
  Serial.println(".. ................................ .");
  Serial.print("Conectando a WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a la red WiFi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  // Configurar servidor MQTT
  client.setServer(mqttServer, mqttPort);
  
  // Conexión MQTT bloqueante
  while (!client. connected()) {
    Serial.println("Conectando al broker MQTT...");
    if (client.connect("NodoNAPIoT", mqttUser, mqttPassword)) {
      Serial.println("Conectado al broker MQTT!");
    } else {
      Serial.print("Error al conectar con broker MQTT: ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  
  // Configurar NTP después de WiFi conectado
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  Serial.println("\nSistema listo!\n");
}

// ===== LOOP =====
void loop() {
  // Mantener conexión MQTT
  if (!client.connected()) {
    Serial.println("Reconectando al broker MQTT...");
    if (client.connect("NodoNAPIoT", mqttUser, mqttPassword)) {
      Serial.println("Reconectado!");
    } else {
      Serial.print("Error: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
  client.loop();
  
  readPressureSensor();
  readCollisionSensor();
  readIRSensor();

  delay(5);
}
