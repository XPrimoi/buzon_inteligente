#include <WiFi.h>
#include <PubSubClient.h>

// ----------------------------
// CONFIG WIFI & MQTT
// ----------------------------
const char* ssid = "Grelo_Casa"; 
const char* password = "NH4zX6PT";

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

const char* mqttTopic = "NAPIoT2025/buzonInteligente/colisionSuperior"; 

WiFiClient espClient;
PubSubClient client(espClient);

// ----------------------------
// PIN DEL SENSOR DE COLISIÓN
// ----------------------------
#define PIN_COLISION  8  n

// ----------------------------
// SETUP
// ----------------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_COLISION, INPUT_PULLDOWN);  

  setup_wifi();
  client.setServer(mqttServer, mqttPort);

  Serial.println("=== SENSOR DE COLISION -> MQTT JSON ===");
}

// ----------------------------
// LOOP
// ----------------------------
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Leer estado del sensor
  int estado = digitalRead(PIN_COLISION);

  // Convertir a booleano para JSON
  bool choque = (estado == HIGH);

  // Crear JSON
  char jsonPayload[64];
  sprintf(jsonPayload, "{\"colisionSuperior\": %s}", choque ? "0" : "1");

  // En consola
  Serial.print("Enviando -> ");
  Serial.println(jsonPayload);

  // Publicar
  client.publish(mqttTopic, jsonPayload);

  delay(1000);
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
    if (client.connect("NodoColision_JSON", mqttUser, mqttPassword)) {
      Serial.println(" conectado");
    } else {
      Serial.print(" fallo rc=");
      Serial.println(client.state());
    }
  }
}
