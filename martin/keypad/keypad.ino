#include <Keypad.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define KEYPAD_ROWS 4 // Keypad de 4 filas
#define KEYPAD_COLS 4 // Keypad de 4 columnas
char keypadKeys[KEYPAD_ROWS][KEYPAD_COLS] = { // Teclas del keypad
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
#define KEYPAD_COMMAND_KEY '#' // Tecla de introducción de codigo/comando
#define KEYPAD_CLEAR_KEY '*'   // Tecla de borrado

byte KEYPAD_ROW_PINS[KEYPAD_ROWS] = {A4, A3, A2, A1};     // Pines de filas
byte KEYPAD_COL_PINS[KEYPAD_COLS] = {D13, D12, D11, D10}; // Pines de columnas

Keypad keypad = Keypad(makeKeymap(keypadKeys), KEYPAD_ROW_PINS, KEYPAD_COL_PINS, KEYPAD_ROWS, KEYPAD_COLS); // Controlador del Keypad

#define WIFI_SSID "POCOYo"       // ID de red WiFi
#define WIFI_PASSWORD "pocoyoyo" // Contraseña de red WiFi

WiFiClient wifiClient; // Cliente WiFi para el cliente MQTT

#define MQTT_SERVER "test.mosquitto.org"                    // Servidor MQTT
#define MQTT_PORT 1883                                      // Puerto del servidor MQTT
#define MQTT_CLIENT_ID "NodoNAPIoT-buzonInteligente-keypad" // ID de cliente a conectarse al servidor
#define MQTT_USER ""
#define MQTT_PASSWORD ""


#define MQTT_TOPIC_CODIGO "NAPIoT2025/buzonInteligente/keypad/codigo"
#define MQTT_TOPIC_CODIGO_CHECK "NAPIoT2025/buzonInteligente/keypad/codigo/check"
#define MQTT_TOPIC_CODIGO_ACTUALIZAR "NAPIoT2025/buzonInteligente/keypad/codigo/update"
#define MQTT_TOPIC_CODIGO_ACTUALIZADO "NAPIoT2025/buzonInteligente/keypad/codigo/updated"

PubSubClient mqttClient(wifiClient); // Cliente MQTT

#define CODIGO_MAX_LEN 16              // Máxima longitud del código
char codigo[CODIGO_MAX_LEN + 1] = {0}; // Almacén del código actual
int codigoSize = 0;                    // Logitud del código actual

#define RGB_RED_PIN D3   // Pin del terminal R del led RGB
#define RGB_GREEN_PIN D2 // Pin del terminal G del led RGB
#define RGB_BLUE_PIN D5  // Pin del terminal B del led RGB

#define RGB_LIFETIME_MILLIS 5000 // Tiempo de iluminación del led RGB
long unsigned rgbOnUntil = 0;    // Marca de tiempo hasta la cual el led RGB debe estar encendido

#define RGB_MAX_VALUE 255 

typedef struct {
  int r;
  int g;
  int b;
} Color;

#define COLOR_BLANK (Color){0}           // Color "apagado" 
#define COLOR_RED (Color){255, 0, 0}    // Color rojo
#define COLOR_GREEN (Color){0, 255, 0} // Color verde

// Ilumincación del led RGB con un color específico
void iluminarRGB(Color rgb) {
  analogWrite(RGB_RED_PIN, RGB_MAX_VALUE - rgb.r);
  analogWrite(RGB_GREEN_PIN, RGB_MAX_VALUE - rgb.g);
  analogWrite(RGB_BLUE_PIN, RGB_MAX_VALUE - rgb.b);
}

// Configuración inicial del led RGB
void configurarRGB() {
  pinMode(RGB_RED_PIN, OUTPUT);
  pinMode(RGB_GREEN_PIN, OUTPUT);
  pinMode(RGB_BLUE_PIN, OUTPUT);

  iluminarRGB(COLOR_BLANK);
}

// Conexión con la red WiFi
void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) { return; }

  Serial.println("\Contectando con la red WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Intentos de conexión hasta conseguirlo
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Fallo al conectar con la red WiFi: ");
    Serial.println(WiFi.status());
    delay(1000);
  }

  // Datos de la conexión
  Serial.println("Conectado a la red WiFi!");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
  Serial.print("IP gateway: ");
  Serial.println(WiFi.gatewayIP());
}

// Conexión con el broker MQTT
void conectarBrokerMQTT() {
  Serial.println("\nConectando al broker MQTT...");
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  // Intentos de conexión con el broker MQTT
  while (!mqttClient.connected() && !mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
    Serial.print("Fallo al conectar con el broker MQTT: ");
    Serial.println(mqttClient.state());
    delay(1000);
  }

  Serial.println("Conectado al broker MQTT!");
}

// Informa de la validez del último código introducido
void validacionCodigo(bool valido) {
  if (valido) {
    Serial.println("El código introducido es válido. Abriendo buzón.");
    iluminarRGB(COLOR_GREEN);
  } else {
    Serial.println("El código introducido no es válido!");
    iluminarRGB(COLOR_RED);
  }
  rgbOnUntil = millis() + RGB_LIFETIME_MILLIS;
}

// Callback para la recepción de mensajes MQTT
void subscriptionCallback(const char topic[], byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido del topico: ");
  Serial.println(topic);

  // Mensaje de validación del código
  if (strcmp(topic, MQTT_TOPIC_CODIGO_CHECK) == 0) {
    bool valido = strncmp((char *)payload, "true", 4) == 0; // Comprobamos si el mensaje devuelve "true"
    validacionCodigo(valido);
  }
}

// Subscripción a los tópicos de MQTT
void subscribeTopicsMQTT() {
  mqttClient.subscribe(MQTT_TOPIC_CODIGO_CHECK);
  mqttClient.setCallback(subscriptionCallback);
}

void setup(){
  Serial.begin(9600);

  configurarRGB();       // Configuracion LED RGB
  conectarWiFi();        // Conexión WiFi
  conectarBrokerMQTT();  // Conexión MQTT
  subscribeTopicsMQTT(); // Subscripción a topicos de respuestas
}

void loop() {
  long unsigned currentMillis = millis();
  
  mqttClient.loop();

  // Reconectar en caso de perder conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectada, intentando reconectar...");
    conectarWiFi();
  }

  // Apagar led RGB si se acaba el tiempo de iluminación
  if (rgbOnUntil <= currentMillis) {
    iluminarRGB(COLOR_BLANK);
  }

  // Comprobar pulsación de tecla
  char pressedKey = keypad.getKey();
  if (pressedKey != 0) {
    Serial.print("Tecla pulsada: ");
    Serial.println(pressedKey);
    switch(pressedKey) {      
      // Notificación del código
      case KEYPAD_COMMAND_KEY: {
        // Generamos null-terminated string del código
        codigo[codigoSize] = 0;

        // Enviamos el código al topico MQTT
        if (mqttClient.publish(MQTT_TOPIC_CODIGO, codigo)) {
          Serial.print("Código de desbloqueo enviado: ");
          Serial.println(codigo);
        } else {
          Serial.print("Fallo al enviar el código: ");
          Serial.println(codigo);
        }
        codigoSize = 0;
      } break;

      // Borrado del código
      case KEYPAD_CLEAR_KEY: {
        codigoSize = 0;
      } break;

      // Inclusión de caracter al código
      default: {
        if (codigoSize < CODIGO_MAX_LEN) {
          codigo[codigoSize] = pressedKey;
          codigoSize++;
        } else {
          Serial.println("\nLongitud máxima de código alcanzada!");
        }
      } break;
    }
  }
}