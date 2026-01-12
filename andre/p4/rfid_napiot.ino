#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

// --- CREDENCIALES WIFI ---
const char* ssid = "Galaxy_Andre"; 
const char* password = "patataf1";

// --- CONFIGURACIÓN MQTT ---
#define MQTT_CLOUDLET_SERVER    "panel.servergal.com.es"
#define MQTT_CLOUDLET_PORT      1883
#define MQTT_FOG_SERVER         "fog.servergal.com.es"
#define MQTT_FOG_PORT           1884
#define MQTT_CLIENT_ID          "ESP#1_Firebeetle"
#define MQTT_USER               "" 
#define MQTT_PASSWORD           ""  

const char* mqttTopic = "buzon/rfid"; 
const char* mqttRGB = "buzon/rgb";

// Variables para almacenar el servidor actual activo
const char* activeServer = MQTT_FOG_SERVER;
int activePort = MQTT_FOG_PORT;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient); 

#define SS_PIN  A1  
#define RST_PIN 21 
#define LED_PIN 2         

#define MAX_USUARIOS 20   

// --- OBJETOS GLOBALES ---
MFRC522 rfid(SS_PIN, RST_PIN);
Preferences preferences;

// Tarjeta maestra y usuarios
byte masterCard[4] = {19, 231, 249, 152}; 
char jsonPayload[64]; 

struct Usuario {
  byte uid[4];
  bool esValido; 
};

Usuario usuarios[MAX_USUARIOS]; 
bool modoAdmin = false;

// --- DECLARACIÓN DE FUNCIONES ---
void setup_wifi();
void gestionarConexionMQTT();
void gestionarUsuarios(byte* uidLeido);
int buscarUsuario(byte* uid);
int buscarSlotVacio();
void guardarUsuarios();
void cargarUsuarios();
bool compararUID(byte* leido, byte* guardado);

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // 1. Iniciar WiFi
  setup_wifi();

  // 2. Iniciar MQTT (Determina servidor)
  // 
  gestionarConexionMQTT(); 

  // 3. Iniciar RFID y Memoria
  SPI.begin();
  rfid.PCD_Init();
  preferences.begin("access_db", false);
  cargarUsuarios();

  Serial.println(F("--- SISTEMA RFID LISTO ---"));
  // Indicar visualmente en el hardware
  pinMode(SS_PIN, OUTPUT); 
}

// --- LOOP ---
void loop() {
  // Verificación y reconexión MQTT
  if (!mqttClient.connected()) {
    gestionarConexionMQTT();
  }
  mqttClient.loop();

  // Revisar lector RFID
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  byte uidLeido[4];
  memcpy(uidLeido, rfid.uid.uidByte, 4); 

  Serial.print("UID Leído: ");
  for(int i=0; i<4; i++) { Serial.print(uidLeido[i]); Serial.print(" "); }
  Serial.println();

  // --- LÓGICA DE TARJETA MAESTRA ---
  if (compararUID(uidLeido, masterCard)) {
    modoAdmin = !modoAdmin; 
    digitalWrite(LED_PIN, modoAdmin); 
    
    if (modoAdmin) {
      Serial.println(F(">> MODO ADMIN ON"));
      sprintf(jsonPayload, "{\"r\": 125, \"g\": 125, \"b\": 0}");
    } else {
      Serial.println(F(">> MODO ADMIN OFF"));
      sprintf(jsonPayload, "{\"r\": 0, \"g\": 125, \"b\": 125}");
    }
    mqttClient.publish(mqttRGB, jsonPayload); 
    delay(1000); 
  }
  
  // --- MODO ADMINISTRADOR (Añadir/Borrar) ---
  else if (modoAdmin) {
    gestionarUsuarios(uidLeido);
    delay(1000);
  }
  
  // --- MODO NORMAL (Acceso) ---
  else {
    int indice = buscarUsuario(uidLeido);
    
    if (indice != -1) {
      // ACCESO CONCEDIDO
      Serial.println(F(">> ACCESO CONCEDIDO"));
      // sprintf(jsonPayload, "{\"rfid\": true}");
      // mqttClient.publish(mqttTopic, jsonPayload);
      
      mqttClient.publish(mqttTopic, "1"); 
      
      // sprintf(jsonPayload, "{\"r\": 0, \"g\": 255, \"b\": 0}");
      // mqttClient.publish(mqttRGB, jsonPayload); 

    } else {
      // ACCESO DENEGADO
      Serial.println(F(">> ACCESO DENEGADO"));
      sprintf(jsonPayload, "{\"rfid\": false}");
      mqttClient.publish(mqttTopic, "0"); 
    }
    delay(1000); 
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// --- FUNCIONES AUXILIARES ---

void setup_wifi() {
  delay(10);
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado. IP: ");
  Serial.println(WiFi.localIP());
}

// Lógica unificada para conectar/reconectar
void gestionarConexionMQTT() {
  // Primero intentamos FOG
  if (!mqttClient.connected()) {
    Serial.print("Intentando FOG (Puerto "); Serial.print(MQTT_FOG_PORT); Serial.println(")...");
    mqttClient.setServer(MQTT_FOG_SERVER, MQTT_FOG_PORT);
    
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Conectado a FOG Node.");
      activeServer = MQTT_FOG_SERVER;
      activePort = MQTT_FOG_PORT;
      return;
    } else {
      Serial.print("ERROR FOG: Fallo de conexión. Código de estado = ");
      Serial.println(mqttClient.state()); 
    }
  }

  if (!mqttClient.connected()) {
    Serial.println("Saltando a CLOUDLET...");
    mqttClient.setServer(MQTT_CLOUDLET_SERVER, MQTT_CLOUDLET_PORT);
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("Conectado a Cloudlet.");
    }
  }
}

void gestionarUsuarios(byte* uidLeido) {
    int indice = buscarUsuario(uidLeido);
    if (indice != -1) {
      usuarios[indice].esValido = false; 
      Serial.println(F("Usuario borrado."));
      sprintf(jsonPayload, "{\"r\": 255, \"g\": 255, \"b\": 0}");
      mqttClient.publish(mqttRGB, jsonPayload);
    } else {
      int slotVacio = buscarSlotVacio();
      if (slotVacio != -1) {
        memcpy(usuarios[slotVacio].uid, uidLeido, 4);
        usuarios[slotVacio].esValido = true;
        Serial.println(F("Usuario agregado."));
        sprintf(jsonPayload, "{\"r\": 0, \"g\": 0, \"b\": 255}");
        mqttClient.publish(mqttRGB, jsonPayload);
      } else {
        Serial.println(F("Memoria llena."));
      }
    }
    guardarUsuarios(); 
}

int buscarUsuario(byte* uid) {
  for (int i = 0; i < MAX_USUARIOS; i++) {
    if (usuarios[i].esValido && compararUID(usuarios[i].uid, uid)) return i;
  }
  return -1;
}

int buscarSlotVacio() {
  for (int i = 0; i < MAX_USUARIOS; i++) {
    if (!usuarios[i].esValido) return i;
  }
  return -1;
}

void guardarUsuarios() {
  preferences.putBytes("db", usuarios, sizeof(usuarios));
}

void cargarUsuarios() {
  if (preferences.isKey("db")) {
    preferences.getBytes("db", usuarios, sizeof(usuarios));
  }
}

bool compararUID(byte* leido, byte* guardado) {
  for (byte i = 0; i < 4; i++) {
    if (leido[i] != guardado[i]) return false;
  }
  return true;
}