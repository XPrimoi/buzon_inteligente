#include <WiFi.h>
#include <PubSubClient.h>

// ----------------------------
// CONFIGURACIÓN WIFI Y MQTT
// ----------------------------
const char* ssid = "Wifi"; 
const char* password = "Contrasena";

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

const char* mqttTopic = "NAPIoT2025/buzonInteligente/presion"; 

WiFiClient espClient;
PubSubClient client(espClient);

// ----------------------------
// CONFIGURACIÓN SENSOR
// ----------------------------
#define PIN_SENSOR 6   // Entrada analógica

// ----------------------------
// SETUP
// ----------------------------
void setup() {
  Serial.begin(115200);

  setup_wifi();
  client.setServer(mqttServer, mqttPort);

  Serial.println("=== SENSOR ANALÓGICO -> MQTT JSON ===");
}

// ----------------------------
// LOOP PRINCIPAL
// ----------------------------
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Leer analógico
  int valor = analogRead(PIN_SENSOR);
  int presion = 4095 - valor;

  // Crear JSON
  char jsonPayload[64];
  sprintf(jsonPayload, "{\"presion\": %d}", presion);

  // Mostrar en Serial
  Serial.print("Enviando -> ");
  Serial.println(jsonPayload);

  // Publicar
  client.publish(mqttTopic, jsonPayload);

  delay(500);
}

// ----------------------------
// FUNCIONES AUXILIARES
// ----------------------------
void setup_wifi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
}

void reconnect() {
  if (!client.connected()) {
    Serial.print("Intentando reconectar MQTT...");
    if (client.connect("NodoPresion_JSON", mqttUser, mqttPassword)) {
      Serial.println(" conectado");
    } else {
      Serial.print(" fallo rc=");
      Serial.println(client.state());
    }
  }
}
