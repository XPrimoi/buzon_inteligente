#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// CONSTANTES Y PINES
const int PIN_SERVO = 13;
const int PIN_COLISION = 38;
const int PIN_WLED = 10;
const int PIN_BLUE = 4;
const int PIN_GREEN = 5;
const int PIN_RED = 6;

const int ANGULO_ABIERTO = 180;
const int ANGULO_CERRADO = 0;
const int ANGULO_REPOSO = 90;

// CONFIGURACIÓN RED 
const char* ssid = "ssid";
const char* password = "psswrd";
const char* clientID = "NAPIoT-P3-buzonInteligente-XPM";

// Constantes Cloudlet
const char* mqtt_cloud = "test.mosquitto.org";
const int mqtt_port_cloud = 1883;

// Constantes Fog Computing
const char* mqtt_fog = "panel.servergal.com.es";
const int mqtt_port_fog = 1884;

// Topics
const char* TOPIC_SERVO_CMD = "buzon/servo/desbloqueo";
const char* TOPIC_RGB_CMD   = "buzon/rgb";
const char* TOPIC_COLISION  = "buzon/colision/inferior";
const char* TOPIC_ESTADO    = "buzon/servo/estado";

// OBJETOS
Servo servo;
WiFiClient espClient;
PubSubClient client(espClient);

// ESTADOS Y TIMERS 
enum EstadoServo { CERRADO, ABIERTO };
EstadoServo estadoServo = CERRADO;

unsigned long ultimoTiempoServo = 0;
const long INTERVALO_ESTADO = 5000;
bool flagPublicarEstado = false;

unsigned long rgbInicioMillis = 0;
const long DURACION_RGB = 5000;
bool rgbEncendido = false;

int estadoActualColision = 0;
int estadoAnteriorColision = 0;

// FUNCIONES AUXILIARES

void apagarRGB() {
  analogWrite(PIN_RED, 255);
  analogWrite(PIN_GREEN, 255);
  analogWrite(PIN_BLUE, 255);
  rgbEncendido = false;
}

void moverServo(int angulo) {
  servo.write(angulo);
  delay(650);
  servo.write(ANGULO_REPOSO);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  Serial.printf("Mensaje recibido [%s]: %s\n", topic, message.c_str());

  
  if (strcmp(topic, TOPIC_SERVO_CMD) == 0) {
    if (message == "1" && estadoServo == CERRADO) {
      moverServo(ANGULO_ABIERTO);
      estadoServo = ABIERTO;
      flagPublicarEstado = true;
    }
  } 
  else if (strcmp(topic, TOPIC_RGB_CMD) == 0) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (!error) {
      int r = doc["r"];
      int g = doc["g"];
      int b = doc["b"];

      // Aplicar color (Invertir valores: 255 - valor)
      analogWrite(PIN_RED, 255 - r);
      analogWrite(PIN_GREEN, 255 - g);
      analogWrite(PIN_BLUE, 255 - b);

      rgbInicioMillis = millis();
      rgbEncendido = true;
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a FOG");
    client.setServer(mqtt_fog, mqtt_port_fog);

    if (client.connect(clientID)) {
      Serial.println("¡Conectado a FOG!");
      client.subscribe(TOPIC_SERVO_CMD);
      client.subscribe(TOPIC_RGB_CMD);
      return;
    
    } else {
      Serial.print("Fallo FOG (rc=");
      Serial.print(client.state());
      Serial.println(").");
    }

    Serial.print("Intentando conectar a CLOUD");
    client.setServer(mqtt_cloud, mqtt_port_cloud);
    if (client.connect(clientID)) {
      Serial.println("¡Conectado a CLOUD!");
      client.subscribe(TOPIC_SERVO_CMD);
      client.subscribe(TOPIC_RGB_CMD);
      return;
    
    } else {
      Serial.print("Fallo CLOUD (rc=");
      Serial.print(client.state());
      Serial.println("). Reintentar en 5 segundos...");
      delay(5000);
    }

  }
}

void setup() {
  Serial.begin(115200);
  
  servo.attach(PIN_SERVO);
  servo.write(ANGULO_REPOSO);

  pinMode(PIN_COLISION, INPUT);
  pinMode(PIN_WLED, OUTPUT);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

  digitalWrite(PIN_WLED, LOW);
  apagarRGB();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // Timer RGB
  if (rgbEncendido && (millis() - rgbInicioMillis >= DURACION_RGB)) {
    apagarRGB();
  }

  // Gestión de puerta inferior
  estadoActualColision = digitalRead(PIN_COLISION);
  
  if (estadoActualColision != estadoAnteriorColision) {
    char buffer[2];
    sprintf(buffer, "%d", estadoActualColision);
    client.publish(TOPIC_COLISION, buffer);
    
    digitalWrite(PIN_WLED, estadoActualColision == 0 ? LOW : HIGH);

    // Lógica de cierre automático
    if (estadoActualColision == 0 && estadoServo == ABIERTO) {
      Serial.println("Cierre automático");
      moverServo(ANGULO_CERRADO);
      estadoServo = CERRADO;
      flagPublicarEstado = true;
    }
    estadoAnteriorColision = estadoActualColision;
  }

  // Reporte de estado periódico o por evento
  if (flagPublicarEstado || (millis() - ultimoTiempoServo >= INTERVALO_ESTADO)) {
    ultimoTiempoServo = millis();
    const char* txtEstado = (estadoServo == ABIERTO) ? "0" : "1";
    client.publish(TOPIC_ESTADO, txtEstado);
    flagPublicarEstado = false;
  }
}
