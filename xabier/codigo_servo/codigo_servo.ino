#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

Servo servo;
const int servoPin = 13;
const int colisionPin = 38;
const int wledPin = 10;
const int bluePin = 4;
const int greenPin = 5;
const int redPin = 6;

// Definición del Enum para el estado del servo
enum EstadoServo { CERRADO, ABIERTO };
EstadoServo estadoServo = CERRADO;

// Credenciais da rede Wifi
const char* ssid = "ssid";
const char* password = "psswrd";

// Configuración do broker MQTT
const char* clientID = "NAPIoT-P3-buzonInteligente-servo-XPM-15392";
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;

// Topics
const char* servoTopic = "buzon/servo/desbloqueo";
const char* rgbTopic = "buzon/rgb";
const char* colisionTopic = "buzon/colision/inferior";
const char* estadoServoTopic = "buzon/servo/estado";

// Led que se acenderá/apagará
WiFiClient espClient;
PubSubClient client(espClient);

// Control servo
unsigned long ultimoTiempoServo = millis();
const long intervaloServo = 500;
bool flagPublicarEstado = false;

// Variables para el LED RGB Temporal
unsigned long rgbInicioMillis = millis();
const long intervaloRgb = 5000;
bool rgbEncendido = false;

int estadoActualColision;
int estadoAnteriorColision;


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

  // Imprimese o payload da mensaxe
  String message;
  for (int i = 0; i< length; i++) message += (char)payload[i];

  Serial.printf("Mensaxe recibida[%s]: %s\n", topic, message);
  
  // Apertura del servo
  if (topic == servoTopic){
    if (message == "1") {
      analogWrite(greenPin, 0);
      
      if(estadoServo == CERRADO){
        servo.write(180);
        delay(650);
        servo.write(90);
        estadoServo = ABIERTO;
      }

      analogWrite(greenPin, 255);
      
    }
    flagPublicarEstado = true;

  } else if (topic == rgbTopic) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (!error) {
      int r = doc["r"];
      int g = doc["g"];
      int b = doc["b"];

      // Aplicar color (Invertir valores: 255 - valor)
      analogWrite(redPin, 255 - r);
      analogWrite(greenPin, 255 - g);
      analogWrite(bluePin, 255 - b);

      rgbInicioMillis = millis();
      rgbEncendido = true;
    }
  }
}

// Reconecta co broker MQTT se se perde a conexión
void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a broker MQTT...");
    // Inténtase conectar indicando o ID do dispositivo
    //IMPORTANTE: este ID debe ser único!
    if (client.connect(clientID)) {
      Serial.println("conectado!");
      // Subscripción ao topic
      client.subscribe(servoTopic);
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
  // Configuración do porto serie
  Serial.begin(115200);
  
  servo.attach(servoPin);
  servo.write(90);

  pinMode(colisionPin, INPUT);
  pinMode(wledPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(greenPin, OUTPUT);

  digitalWrite(wledPin, LOW);
  analogWrite(redPin, 255);
  analogWrite(bluePin, 255);
  analogWrite(greenPin, 255);

  // Conexión coa WiFi
  setup_wifi();
  // Configuración de MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Verifica se o cliente está conectado
  if (!client.connected()) reconnect();
  client.loop();

  // Gestión del apagado automático del RGB
  if(rgbEncendido){
    if(millis() - rgbInicioMillis >= intervaloRgb){
      analogWrite(redPin, 255);   // Apagar
      analogWrite(greenPin, 255); 
      analogWrite(bluePin, 255);  
      rgbEncendido = false;
    }
  }

  // Estado de la puerta
  estadoAnteriorColision = estadoActualColision;
  estadoActualColision = digitalRead(colisionPin);
  client.publish(colisionTopic, "%d", estadoActualColision);

  if(estadoActualColision == 0) {
    digitalWrite(wledPin, LOW);

    // Si la puerta estaba abierta y ahora cerrada, se cierra el servo
    if(estadoAnteriorColision == 1 && estadoServo == ABIERTO){
      Serial.println("Cerrando puerta");
      servo.write(0);
      delay(650);
      servo.write(90);
    
      estadoServo = CERRADO;
      flagPublicarEstado = true;
    }
  } else {
    digitalWrite(wledPin, HIGH);
  }

  // Publicar el estado del servo
  if(flagPublicarEstado){
    unsigned long tiempoActual = millis();
    if (tiempoActual - ultimoTiempoServo >= intervaloRgb) {
      
      ultimoTiempoServo = millis();

      char str[64] = {0};
      sprintf(str, "%d", (estadoServo == ABIERTO ? 0 : 1));
      bool publica = client.publish(estadoServoTopic, str);
      flagPublicarEstado = false;
    }
  }
}


