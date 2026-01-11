#include <Keypad.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define KEYPAD_ROWS 4                          // Keypad de 4 filas
#define KEYPAD_COLS 4                          // Keypad de 4 columnas
char keypadKeys[KEYPAD_ROWS][KEYPAD_COLS] = {  // Teclas del keypad
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
#define KEYPAD_COMMAND_KEY '#'  // Tecla de introducción de codigo/comando
#define KEYPAD_CLEAR_KEY   '*'  // Tecla de borrado

byte KEYPAD_ROW_PINS[KEYPAD_ROWS] = {A4, A3, A2, A1};      // Pines de filas
byte KEYPAD_COL_PINS[KEYPAD_COLS] = {D13, D12, D11, D10};  // Pines de columnas

Keypad keypad = Keypad(makeKeymap(keypadKeys), KEYPAD_ROW_PINS, KEYPAD_COL_PINS, KEYPAD_ROWS, KEYPAD_COLS);  // Controlador del Keypad

#define WIFI_SSID     "POCOYo"/* Rellenar */  // ID de red WiFi
#define WIFI_PASSWORD "pocoyoyo"/* Rellenar */  // Contraseña de red WiFi

WiFiClient wifiClient;  // Cliente WiFi para el cliente MQTT

#define MQTT_CLOUDLET_SERVER    "nodered.servergal.com.es"  // Servidor MQTT del cloudlet
#define MQTT_CLOUDLET_PORT      1883                        // Puerto del servidor MQTT del cloudlet
#define MQTT_FOG_SERVER         "panel.servergal.com.es"    // Servidor MQTT del nodo fog
#define MQTT_FOG_PORT           1884                        // Puerto del servidor MQTT del nodo fog
#define MQTT_CLIENT_ID          "ESP#2"                     // ID de cliente a conectarse al servidor
#define MQTT_USER               ""
#define MQTT_PASSWORD           ""

#define MQTT_TOPIC_KEYPAD "buzon/keypad"  // Tópico para enviar el código del keypad
#define MQTT_TOPIC_RGB    "buzon/rgb"     // Tópico para enviar el valor RGB de iluminación del LED

PubSubClient mqttClient(wifiClient);  // Cliente MQTT

#define CODIGO_MAX_LEN 16               // Máxima longitud del código
char codigo[CODIGO_MAX_LEN + 1] = {0};  // Almacén del código actual
int codigoSize = 0;                     // Logitud del código actual

#define CODIGO_LIFETIME_MILLIS 10000  // Tiempo de guardado de código entre pulsaciones del keypad
long unsigned codigoSavedUntil = 0;   // Marca de tiempo hasta la cual el código del keypad será guardado en la variable

typedef struct {
    unsigned r;
    unsigned g;
    unsigned b;
} Color;

#define COLOR_BLUE  (Color){0, 0, 255}    // Color azul
#define COLOR_AMBER (Color){255, 190, 0}  // Color ambar

// Conexión con la red WiFi
void conectarWiFi() {
    if (WiFi.status() == WL_CONNECTED) { return; }

    Serial.println("\nConectando con la red WiFi...");
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

// Conexión con el broker MQTT del nodo fog
bool conectarFogMQTT() {
    Serial.println("\nConectando al broker MQTT del nodo fog...");
    mqttClient.setServer(MQTT_FOG_SERVER, MQTT_FOG_PORT);

    // Intentos de conexión con el broker MQTT del nodo fog
    if (!mqttClient.connected() && !mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.print("Fallo al conectar con el broker MQTT del nodo fog: ");
        Serial.println(mqttClient.state());
        return false;
    } else {
        Serial.println("Conectado al broker MQTT del nodo fog!");
        return true;
    }
}

// Conexión con el broker MQTT del Cloudlet
void conectarCloudletMQTT() {
    Serial.println("\nConectando al broker MQTT del Cloudlet...");
    mqttClient.setServer(MQTT_CLOUDLET_SERVER, MQTT_CLOUDLET_PORT);

    // Intentos de conexión con el broker MQTT de Cloudlet
    while (!mqttClient.connected() && !mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.print("Fallo al conectar con el broker MQTT del Cloudlet: ");
        Serial.println(mqttClient.state());
        delay(1000);
    }

    Serial.println("Conectado al broker MQTT del Cloudlet!");
}

// Envío de valor de iluminación al led RGB con un color específico
void iluminarRGB(Color color) {
    char rgbJSONvalue[20] = {0};
    sprintf(rgbJSONvalue, "{r:%d,g:%d,b:%d}", color.r, color.g, color.b);

    if (mqttClient.publish(MQTT_TOPIC_RGB, rgbJSONvalue)) {
        Serial.print("Enviado valor de iluminación al led RGB: {r: ");
        Serial.print(color.r);
        Serial.print(", g: ");
        Serial.print(color.g);
        Serial.print(", b: ");
        Serial.print(color.b);
        Serial.println("}.");
    } else {
        Serial.print("Fallo al enviar valor de iluminación al led RGB.");
    }
}

void setup() {
    Serial.begin(9600);

    conectarWiFi();                  // Conexión WiFi
    bool conectadoFog = conectarFogMQTT(); // Conexión MQTT con el Nodo Fog
    if (!conectadoFog) {
        conectarCloudletMQTT();  // Conexión MQTT con el Cloudlet
    };       
}

void loop() {
    long unsigned currentMillis = millis();

    mqttClient.loop();

    // Reconectar en caso de perder conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi desconectada, intentando reconectar...");
        conectarWiFi();
    }

    // Borrar el código guardado si el tiempo límite es sobrepasado
    if (codigoSize > 0 && codigoSavedUntil <= currentMillis) {
        Serial.println("Código borrado por timeout entre pulsaciones.");
        codigoSize = 0;
        iluminarRGB(COLOR_BLUE);
    }

    // Comprobar pulsación de tecla
    char pressedKey = keypad.getKey();
    if (pressedKey) {
        codigoSavedUntil = currentMillis + CODIGO_LIFETIME_MILLIS;

        Serial.print("Tecla pulsada: ");
        Serial.println(pressedKey);
        switch (pressedKey) {
            // Notificación del código
            case KEYPAD_COMMAND_KEY: {
                // Generamos null-terminated string del código
                codigo[codigoSize] = 0;

                // Enviamos el código al topico MQTT
                if (mqttClient.publish(MQTT_TOPIC_KEYPAD, codigo)) {
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
                Serial.println("Código borrado.");
                iluminarRGB(COLOR_BLUE);
            } break;

            // Inclusión de caracter al código
            default: {
                if (codigoSize < CODIGO_MAX_LEN) {
                    codigo[codigoSize] = pressedKey;
                    codigoSize++;
                } else {
                    Serial.println("Longitud máxima de código alcanzada!");
                    iluminarRGB(COLOR_AMBER);
                }
            } break;
        }
    }
}