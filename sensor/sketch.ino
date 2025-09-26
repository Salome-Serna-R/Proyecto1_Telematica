#include <WiFi.h>
#include <WiFiUdp.h>
#include "DHT.h"

#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define SERVER_IP   "98.88.32.220"  // IP del servido desplegado
#define SERVER_PORT 5683

#define DHTPIN 15
#define DHTTYPE DHT22

WiFiUDP udp;
DHT dht(DHTPIN, DHTTYPE);

// -----------------------------
// Construcción de paquete CoAP
// -----------------------------
uint16_t message_id = 0;

int buildCoapPOST(const char *uri_path, const char *payload, uint8_t *buffer, size_t maxlen) {
  uint8_t ver = 1;
  uint8_t type = 0;  // CON
  uint8_t tkl = 0;   // sin token
  uint8_t code = 2;  // POST
  message_id++;

  size_t idx = 0;
  buffer[idx++] = (ver << 6) | (type << 4) | tkl;
  buffer[idx++] = code;
  buffer[idx++] = (message_id >> 8) & 0xFF;
  buffer[idx++] = (message_id & 0xFF);

  // Opción: Uri-Path ("data")
  uint8_t delta = 11; // Uri-Path number
  uint8_t length = strlen(uri_path);
  buffer[idx++] = (delta << 4) | length;
  memcpy(&buffer[idx], uri_path, length);
  idx += length;

  // Payload marker
  buffer[idx++] = 0xFF;
  size_t payload_len = strlen(payload);
  memcpy(&buffer[idx], payload, payload_len);
  idx += payload_len;

  return idx;
}

// -----------------------------
// Setup y Loop
// -----------------------------
void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi!");
}

void loop() {
  float temp = dht.readTemperature();
  if (isnan(temp)) {
    Serial.println("Error leyendo DHT22");
    delay(2000);
    return;
  }

  char payload[32];
  snprintf(payload, sizeof(payload), "%.2f", temp);

  uint8_t packet[128];
  int len = buildCoapPOST("data", payload, packet, sizeof(packet));

  udp.beginPacket(SERVER_IP, SERVER_PORT);
  udp.write(packet, len);
  udp.endPacket();

  Serial.print("POST enviado: ");
  Serial.println(payload);

  delay(10000); // enviar cada 10 segundos
}
