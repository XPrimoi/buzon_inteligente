#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

// -----------------------------------------------------
// CONFIG WIFI & MQTT
// -----------------------------------------------------
const char* ssid = "Grelo_Casa";
const char* password = "NH4zX6PT";

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

const char* mqttTopicRFID = "NAPIoT2025/buzonInteligente/rfid";
const char* mqttTopicRGB  = "NAPIoT2025/buzonInteligente/ledrgb";

WiFiClient espClient;
PubSubClient client(espClient);

// -----------------------------------------------------
// LED RGB
// -----------------------------------------------------
#define PIN_R  10
#define PIN_G  11
#define PIN_B  4

void apagarLED() {
  analogWrite(PIN_R, 255);
  analogWrite(PIN_G, 255);
  analogWrite(PIN_B, 255);
}

void setColor(int r, int g, int b) {
  analogWrite(PIN_R, 255 - r);
  analogWrite(PIN_G, 255 - g);
  analogWrite(PIN_B, 255 - b);
}

// Autoapagado LED (solo MQTT)
unsigned long rgbLastChange = 0;
bool rgbEncendido = false;

// -----------------------------------------------------
// RFID CONFIG
// -----------------------------------------------------
#define SS_PIN  A1
#define RST_PIN 21
#define MAX_USUARIOS 20

byte masterCard[4] = {19, 231, 249, 152};

struct Usuario {
  byte uid[4];
  bool esValido;
};

Usuario usuarios[MAX_USUARIOS];

MFRC522 rfid(SS_PIN, RST_PIN);
Preferences preferences;

bool modoAdmin = false;

// -----------------------------------------------------
// FUNCIONES RFID
// -----------------------------------------------------
bool compararUID(byte* leido, byte* guardado) {
  for (byte i = 0; i < 4; i++)
    if (leido[i] != guardado[i]) return false;
  return true;
}

int buscarUsuario(byte* uid) {
  for (int i = 0; i < MAX_USUARIOS; i++)
    if (usuarios[i].esValido && compararUID(usuarios[i].uid, uid))
      return i;
  return -1;
}

int buscarSlotVacio() {
  for (int i = 0; i < MAX_USUARIOS; i++)
    if (!usuarios[i].esValido) return i;
  return -1;
}

void guardarUsuarios() {
  preferences.putBytes("db", usuarios, sizeof(usuarios));
}

void cargarUsuarios() {
  if (preferences.isKey("db"))
    preferences.getBytes("db", usuarios, sizeof(usuarios));
}

void gestionarUsuarios(byte* uidLeido) {
  int indice = buscarUsuario(uidLeido);
  if (indice != -1) {
    usuarios[indice].esValido = false;
    Serial.println("Usuario borrado.");
  } else {
    int slot = buscarSlotVacio();
    if (slot != -1) {
      memcpy(usuarios[slot].uid, uidLeido, 4);
      usuarios[slot].esValido = true;
      Serial.println("Usuario agregado.");
    } else {
      Serial.println("Memoria llena.");
    }
  }
  guardarUsuarios();
}

// -----------------------------------------------------
// JSON PARSER PARA LED
// -----------------------------------------------------
int extraerValor(String json, String key) {
  int index = json.indexOf(key);
  if (index == -1) return 0;

  int colon = json.indexOf(":", index);
  int comma = json.indexOf(",", colon);
  int endBrace = json.indexOf("}", colon);

  int end = (comma == -1) ? endBrace : comma;

  return json.substring(colon + 1, end).toInt();
}

// -----------------------------------------------------
// CALLBACK MQTT → LED RGB
// -----------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT recibido: ");
  Serial.write(payload, length);
  Serial.println();

  if (strcmp(topic, mqttTopicRGB) != 0) return;

  String json = "";
  for (int i = 0; i < length; i++) json += (char)payload[i];

  int r = extraerValor(json, "r");
  int g = extraerValor(json, "g");
  int b = extraerValor(json, "b");

  Serial.printf("Color -> R:%d G:%d B:%d\n", r, g, b);

  setColor(r, g, b);

  // Activar temporizador
  rgbLastChange = millis();
  rgbEncendido = true;
}

// -----------------------------------------------------
// WIFI + MQTT
// -----------------------------------------------------
void setup_wifi() {
  Serial.print("Conectando WiFi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Reconectando MQTT...");
    if (client.connect("NodoUnificado")) {
      Serial.println(" OK");
      client.subscribe(mqttTopicRGB);
    } else {
      Serial.print(" error ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// -----------------------------------------------------
// SETUP PRINCIPAL
// -----------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);

  apagarLED();

  setup_wifi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  SPI.begin();
  rfid.PCD_Init();

  preferences.begin("access_db", false);
  cargarUsuarios();

  Serial.println("--- SISTEMA UNIFICADO RFID + LED RGB + MQTT ---");
}

// -----------------------------------------------------
// LOOP PRINCIPAL
// -----------------------------------------------------
void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // Autoapagado LED (solo MQTT)
  if (rgbEncendido && (millis() - rgbLastChange >= 5000)) {
    apagarLED();
    rgbEncendido = false;
    Serial.println("LED RGB apagado automáticamente tras 5 segundos");
  }

  // RFID
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  byte uid[4];
  memcpy(uid, rfid.uid.uidByte, 4);

  // Tarjeta maestra → admin ON/OFF
  if (compararUID(uid, masterCard)) {
    modoAdmin = !modoAdmin;

    if (modoAdmin) {
      Serial.println(">> ADMIN ON");
      setColor(0, 0, 255);  // Azul
    } else {
      Serial.println(">> ADMIN OFF");
      setColor(255, 0, 255);  // Morado
    }

    delay(1000);
    apagarLED();
    return;
  }

  // Admin mode → agregar/borrar usuarios
  if (modoAdmin) {
    gestionarUsuarios(uid);
    delay(1000);
    return;
  }

  // Modo normal → verificar acceso
  int indice = buscarUsuario(uid);
  char jsonBuffer[64];

  if (indice != -1) {
    sprintf(jsonBuffer, "{\"rfid\": true}");
    Serial.println("ACCESO PERMITIDO");
    client.publish(mqttTopicRFID, jsonBuffer);

  } else {
    sprintf(jsonBuffer, "{\"rfid\": false}");
    Serial.println("ACCESO DENEGADO");
    client.publish(mqttTopicRFID, jsonBuffer);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
