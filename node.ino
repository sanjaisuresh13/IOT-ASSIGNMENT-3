#include <WiFi.h>
#include <esp_now.h>
#include "DHT.h"
#include <Adafruit_NeoPixel.h>

#define DHTPIN 4
#define DHTTYPE DHT11
#define PIXEL_PIN 5

DHT dht(DHTPIN, DHTTYPE);
Adafruit_NeoPixel pixel(1, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

// ===== GATEWAY MAC =====
uint8_t gatewayMac[] = {0x08,0xB6,0x1F,0x27,0x7C,0x34};

// ===== TELEMETRY PACKET =====
typedef struct {
  uint8_t  id[6];
  float    t;
  float    h;
  uint8_t  alert;
  uint32_t ts;
} pkt_t;

pkt_t pkt;

// ===== GLOBAL ALERT COMMAND =====
typedef struct {
  uint8_t cmd;
} alert_cmd_t;

// ===== SEND CALLBACK (NEW API SAFE) =====
void onSend(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "[SEND OK]" : "[SEND FAIL]");
}

// ===== RECEIVE GLOBAL ALERT =====
void onRecv(const esp_now_recv_info_t *info,
            const uint8_t *data, int len) {

  if (len == sizeof(alert_cmd_t)) {
    Serial.println("🚨 GLOBAL ALERT RECEIVED!");
    for(int i=0;i<10;i++){
      pixel.setPixelColor(0, pixel.Color(255,0,0));
      pixel.show();
      delay(300);
      pixel.clear();
      pixel.show();
      delay(300);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pixel.begin();
  pixel.clear();
  pixel.show();

  dht.begin();
  delay(1500);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESPNOW FAIL");
    return;
  }

  esp_now_register_send_cb(onSend);
  esp_now_register_recv_cb(onRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, gatewayMac, 6);
  peer.channel = 6;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  pkt.t = t;
  pkt.h = h;
  pkt.ts = millis();

  WiFi.macAddress(pkt.id);

  if (isnan(t) || isnan(h)) pkt.alert = 3;
  else if (t > 30) pkt.alert = 1;
  else if (t < 15) pkt.alert = 2;
  else pkt.alert = 0;

  // ===== LOCAL LED =====
  if(pkt.alert==0) pixel.setPixelColor(0, pixel.Color(0,255,0));
  else pixel.setPixelColor(0, pixel.Color(255,0,0));
  pixel.show();

  esp_now_send(gatewayMac, (uint8_t*)&pkt, sizeof(pkt));
}

void loop() {}