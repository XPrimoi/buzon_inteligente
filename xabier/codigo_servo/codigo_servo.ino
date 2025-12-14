#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

Servo servo;
const int servoPin = 10;

// Credenciais da rede Wifi
const char* ssid = "ssid";
const char* password = "psswrd";

// Configuración do broker MQTT
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic = "NAPIoT2025/buzonInteligente/puerta/servo";

// Led que se acenderá/apagará
const int ledPin = LED_BUILTIN;
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long ultimoTiempo = 0;
const long intervalo = 500;
int ultimoAngulo= -1;
bool flag = false;

// Función para conectar á WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi conectada!");
  Serial.println("IP: ");
  Serial.println(WiFi.localIP());
}

// Función de callback que procesa as mensaxes MQTT recibidas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaxe recibida[");
  Serial.print(topic);
  Serial.print("] ");
  // Imprimese o payload da mensaxe
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  // Controlamos o estado do LED en fucnión da mensaxe
  if (message == "0") {
    digitalWrite(ledPin, LOW); // Apaga o LED
    Serial.println("LED OFF");
    servo.write(0);
  } else if (message == "1") {
    digitalWrite(ledPin, HIGH); // Acende o LED
    Serial.println("LED ON");
    servo.write(180);
  } else {
    Serial.println("Comando descoñecido");
  }
  flag = true;
}

// Reconecta co broker MQTT se se perde a conexión
void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a broker MQTT...");
    // Inténtase conectar indicando o ID do dispositivo
    //IMPORTANTE: este ID debe ser único!
    if (client.connect("NAPIoT-P2-buzonInteligente-servo-XPM-15392")) {
      Serial.println("conectado!");
      // Subscripción ao topic
      client.subscribe(mqtt_topic);
      Serial.println("Subscrito ao topic");
    } else {
      Serial.print("erro na conexión, erro=");
      Serial.print(client.state());
      Serial.println(" probando de novo en 5 segundos");
      delay(5000);
    }
  }
}


void setup() {
  servo.attach(servoPin);
  servo.write(0);

  // Configuración do pin do LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  // Configuración do porto serie
  Serial.begin(115200);
  // Conexión coa WiFi
  setup_wifi();
  // Configuración de MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Verifica se o cliente está conectado
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if(flag){
    unsigned long tiempoActual = millis();
    if (tiempoActual - ultimoTiempo >= intervalo) {
      
      ultimoTiempo = tiempoActual;

      char str[64] = {0};
      sprintf(str, "{\"servoAngulo\": %d}", servo.read());
      bool publica = client.publish("NAPIoT2025/buzonInteligente/servoAngulo", str);
      flag = false;
    }
  }
}

