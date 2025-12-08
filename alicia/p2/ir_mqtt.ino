#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "MatePad";        
const char* password = "12345678.";  
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

WiFiClient espClient;
PubSubClient client(espClient);

const int IR_RECEIVER_PIN = 4;
const int IR_TRANSMITTER_PIN = 5;

int cartaCount = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Evento recibido en topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  cartaCount++;
  Serial.println("Evento: Carta introducida!");
  Serial.print("Total de cartas: ");
  Serial.println(cartaCount);

  // Publicar estado con contador
  String statusMsg = "Carta detectada en " + msg + " | Total: " + String(cartaCount);
  client.publish("NAPIoT2025/buzonInteligente/ir/status", statusMsg.c_str());
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_RECEIVER_PIN, INPUT);
  pinMode(IR_TRANSMITTER_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Conectando á WiFi.");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connctado á rede WiFi!");
  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);

  while (!client.connected()) {
    if (client.connect("NodoNAPIoT", mqttUser, mqttPassword)) {
      Serial.println("Conectado ao broker MQTT!");
      client.subscribe("NAPIoT2025/buzonInteligente/ir/event");
    } else {
      Serial.print("Erro ao conectar co broker: ");
      Serial.print(client.state());
      delay(2000);
    }
  }

}

void loop() {
  client.loop();

  // Emitimos un pulso IR
  digitalWrite(IR_TRANSMITTER_PIN, HIGH);
  delay(10);
  digitalWrite(IR_TRANSMITTER_PIN, LOW);

  // Lectura del receptor IR
  int irValue = digitalRead(IR_RECEIVER_PIN);

  char str[16];
  sprintf(str, "%d", irValue);
  client.publish("NAPIoT2025/buzonInteligente/ir/event", str);

  Serial.print("Lectura IR: ");
  Serial.println(irValue);

  delay(2000);
}
