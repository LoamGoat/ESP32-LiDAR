#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Arduino_JSON.h>

const char* ssid = "";
const char* password = "";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HardwareSerial lidarSerial(2);

#define PACKET_LEN 22
uint8_t buf[PACKET_LEN];
uint8_t bufIdx = 0;
bool synced = false;

// Store one full rotation: angle -> distance
float scanDist[360];
bool scanValid[360];
bool scanComplete = false;

void processPacket(uint8_t* pkt) {
  int baseAngle = (pkt[1] - 0xA0) * 4;

  for (int i = 0; i < 4; i++) {
    int angle = baseAngle + i;
    if (angle < 0 || angle >= 360) continue;

    uint16_t raw = pkt[4 + i*4] | (pkt[5 + i*4] << 8);
    bool invalid = (raw >> 15) & 1;
    uint16_t dist_mm = raw & 0x3FFF;

    scanValid[angle] = !invalid && dist_mm > 0;
    scanDist[angle] = dist_mm;
  }

  // Treat angle wraparound (356-359deg packet) as "scan complete"
  if (baseAngle >= 356) {
    scanComplete = true;
  }
}

void readLidar() {
  while (lidarSerial.available()) {
    uint8_t b = lidarSerial.read();
    if (!synced) {
      if (b == 0xFA) { buf[0] = b; bufIdx = 1; synced = true; }
      continue;
    }
    buf[bufIdx++] = b;
    if (bufIdx == PACKET_LEN) {
      if (buf[0] == 0xFA && buf[1] >= 0xA0 && buf[1] <= 0xF9) {
        processPacket(buf);
      }
      synced = false;
      bufIdx = 0;
    }
 
  }
}

String getScanJSON() {
  JSONVar scan;
  JSONVar angles;
  JSONVar dists;

  int idx = 0;
  for (int a = 0; a < 360; a++) {
    if (scanValid[a]) {
      angles[idx] = a;
      dists[idx] = scanDist[a];
      idx++;
    }
  }
  scan["angles"] = angles;
  scan["distances"] = dists;
  return JSON.stringify(scan);
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String json) {
  ws.textAll(json);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client #%u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client #%u disconnected\n", client->id());
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  lidarSerial.begin(115200, SERIAL_8N1, 21, 22);

  initWiFi();
  initLittleFS();
  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });
  server.serveStatic("/", LittleFS, "/");
  server.begin();
}

void loop() {
  readLidar();

  if (scanComplete) {
    notifyClients(getScanJSON());
    String json = getScanJSON();
    //Serial.printf("Sending scan, %d bytes\n", json.length());  // TEMP DEBUG
    scanComplete = false;
  }

  ws.cleanupClients();
}