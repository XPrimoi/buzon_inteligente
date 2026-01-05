#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

//Variables de conexion wifi y mqtt
const char* ssid = "Grelo_Casa"; 
const char* password = "NH4zX6PT";
const char* mqttServer = "panel.servergal.com.es";
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";

// TOPIC MQTT
const char* mqttTopic = "buzon/rfid"; 
const char* mqttRGB = "buzon/rgb";

// Configuracion de pines de rfid
#define SS_PIN  A1
#define RST_PIN 21
#define LED_PIN 2         
#define MAX_USUARIOS 20   

// creacion de tarjeta maestra
byte masterCard[4] = {19, 231, 249, 152}; 

char jsonPayload[64]; 


// definicion de usuario
struct Usuario {
  byte uid[4];
  bool esValido; 
};

// Limite de usuarios
Usuario usuarios[MAX_USUARIOS]; 

// Objetos Globales
WiFiClient espClient;
PubSubClient client(espClient);
MFRC522 rfid(SS_PIN, RST_PIN);
Preferences preferences;

bool modoAdmin = false;


// SETUP
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  setup_wifi();
  client.setServer(mqttServer, mqttPort);

  SPI.begin();
  rfid.PCD_Init();
  preferences.begin("access_db", false);
  cargarUsuarios();

  Serial.println(F("--- SISTEMA RFID JSON ---"));
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Si no hay tarjeta, salir
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  byte uidLeido[4];
  memcpy(uidLeido, rfid.uid.uidByte, 4); 

  // Si hay Tarjeta Maestra
  if (compararUID(uidLeido, masterCard)) {
    modoAdmin = !modoAdmin; 
    digitalWrite(LED_PIN, modoAdmin); 
    if (modoAdmin) 
    {
      Serial.println(F(">> MODO ADMIN ON"));
      sprintf(jsonPayload, "{\"r\": 125, \"g\": 125, \"b\": 0}");
      client.publish(mqttRGB, jsonPayload); 
    }

    else
    {
      Serial.println(F(">> MODO ADMIN OFF"));
      sprintf(jsonPayload, "{\"r\": 0, \"g\": 125, \"b\": 125}");
      client.publish(mqttRGB, jsonPayload); 
    } 
    delay(1000); 
  }
  
  // Modo Admin
  else if (modoAdmin) {
    gestionarUsuarios(uidLeido);
    delay(1000);
  }
  
  // Modo Normal
  else {
    int indice = buscarUsuario(uidLeido);
    
    // Buffer para crear el mensaje
    if (indice != -1) {
      // ACCESO CONCEDIDO
      // Creamos el JSON
      
      Serial.println(F(">> ACCESO CONCEDIDO -> Enviando JSON: {\"rfid\": true}"));
      client.publish(mqttTopic, "1"); 

      //sprintf(jsonPayload, "{\"r\": 0, \"g\": 255, \"b\": 0}");
      //client.publish(mqttRGB, jsonPayload); 

      
    } else {
      // ACCESO DENEGADO
      // Creamos el JSON: { "rfid": false }
      sprintf(jsonPayload, "{\"rfid\": false}");
      
      Serial.println(F(">> ACCESO DENEGADO -> Enviando JSON: {\"rfid\": false}"));
      client.publish(mqttTopic, "0"); 

      //sprintf(jsonPayload, "{\"r\": 255, \"g\": 0, \"b\": 0}");
      //client.publish(mqttRGB, jsonPayload); 
      
    }
    delay(1000); 
  }
  
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// FUNCIONES AUXILIARES 

void setup_wifi() {
  delay(10);
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
    Serial.print("Reconectando MQTT...");
    if (client.connect("NodoNAPIoT_Firebeetle_JSON", mqttUser, mqttPassword)) {
      Serial.println("conectado");
    } else {
      Serial.print("falló rc=");
      Serial.print(client.state());
      Serial.println(" reintentando...");
    }
  }
}

void gestionarUsuarios(byte* uidLeido) {
    int indice = buscarUsuario(uidLeido);
    if (indice != -1) {
      usuarios[indice].esValido = false; 
      Serial.println(F("Usuario borrado."));
    } else {
      int slotVacio = buscarSlotVacio();
      if (slotVacio != -1) {
        memcpy(usuarios[slotVacio].uid, uidLeido, 4);
        usuarios[slotVacio].esValido = true;
        Serial.println(F("Usuario agregado."));
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
