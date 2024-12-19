#include "Secrets.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <MFRC522.h> // Librería para el módulo RFID

// Pines para el módulo RFID
#define SDA_PIN 5
#define RST_PIN 0

// Definición de los temas MQTT
#define AWS_IOT_PUBLISH_TOPIC "esp32/esp32-to-aws"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32/aws-to-esp32"

// Intervalo de publicación (en milisegundos)
#define PUBLISH_INTERVAL 4000

// Configuración de las credenciales WiFi
const char* ssid = "iPhonedeAlvaro";
const char* password = "alvaro2003";

// Configuración del endpoint AWS IoT
const char* aws_endpoint = "a2ca81ztsaj9ty-ats.iot.us-east-1.amazonaws.com";

WiFiClientSecure net;              // Cliente seguro para conexión Wi-Fi
MQTTClient client(256);            // Cliente MQTT con un buffer de 256 bytes
MFRC522 rfid(SDA_PIN, RST_PIN);    // Objeto para manejar el módulo RFID

unsigned long lastPublishTime = 0; // Última vez que se envió un mensaje

// UID autorizado
const String tarjetaAutorizada = "C3B09618"; // UID permitido

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("ESP32 conectándose a Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado.");
}

void connectToAWS() {
  // Configurar WiFiClientSecure con las credenciales de AWS IoT
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Conectar al endpoint de AWS IoT
  client.begin(aws_endpoint, 8883, net);

  // Configurar la función para manejar mensajes entrantes
  client.onMessage(messageHandler);

  Serial.print("ESP32 conectándose a AWS IoT");
  while (!client.connect("ESP32_ThingName")) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!client.connected()) {
    Serial.println("ESP32 - ¡Tiempo de espera para conectar con AWS IoT!");
    return;
  }

  // Suscribirse al tema
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
  Serial.println("ESP32 - ¡AWS IoT conectado!");
}

void sendToAWS(const String& rfidID, bool accessGranted) {
  // Crear el mensaje JSON
  StaticJsonDocument<200> message;
  message["timestamp"] = millis();
  message["rfid_id"] = rfidID; // Añadir el ID de la tarjeta RFID
  message["access"] = accessGranted ? "granted" : "denied"; // Estado de acceso

  // Serializar el JSON
  char messageBuffer[512];
  serializeJson(message, messageBuffer);

  // Publicar en el tema de AWS IoT
  client.publish(AWS_IOT_PUBLISH_TOPIC, messageBuffer);

  // Mostrar en el monitor serial
  Serial.println("Enviado:");
  Serial.print("- Tema: ");
  Serial.println(AWS_IOT_PUBLISH_TOPIC);
  Serial.print("- Carga útil: ");
  Serial.println(messageBuffer);
}

void messageHandler(String &topic, String &payload) {
  // Procesar mensajes entrantes
  Serial.println("Recibido:");
  Serial.print("- Tema: ");
  Serial.println(topic);
  Serial.print("- Carga útil: ");
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  SPI.begin(); // Iniciar comunicación SPI
  rfid.PCD_Init(); // Inicializar el módulo RFID
  Serial.println("Módulo RFID inicializado.");

  connectToWiFi(); // Conectar al Wi-Fi
  connectToAWS();  // Conectar a AWS IoT
}

void loop() {
  client.loop(); // Mantener la conexión MQTT

  // Leer tarjeta RFID
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String rfidID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      rfidID += String(rfid.uid.uidByte[i], HEX); // Leer ID en hexadecimal
    }
    rfidID.toUpperCase(); // Convertir a mayúsculas para un formato consistente
    Serial.print("Tarjeta detectada con ID: ");
    Serial.println(rfidID);

    // Verificar si la tarjeta es autorizada
    bool accessGranted = (rfidID == tarjetaAutorizada);
    if (accessGranted) {
      Serial.println("Acceso permitido.");
    } else {
      Serial.println("Acceso denegado.");
    }

    // Enviar ID y estado a AWS
    sendToAWS(rfidID, accessGranted);

    // Detener la lectura de la tarjeta actual
    rfid.PICC_HaltA();
  }

  // Publicar un mensaje periódicamente (si se requiere)
  if (millis() - lastPublishTime > PUBLISH_INTERVAL) {
    lastPublishTime = millis();
  }
}
