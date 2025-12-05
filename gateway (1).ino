Gateway
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <PubSubClient.h>

// ======= WIFI ========
const char* ssid = "Test";
const char* password = "123456789";

// ======= MQTT ========
const char* mqtt_server = "broker.hivemq.com";
WiFiClient espClient;
PubSubClient mqtt(espClient);

// ======= ESPNOW PACKET ========
typedef struct {
  uint8_t  id[6];
  float    t;
  float    h;
  uint8_t  alert;   // 0=Normal  1=High  2=Low  3=Sensor Error
  uint32_t ts;
} pkt_t;

pkt_t lastPkt;
volatile bool newMsg = false;

// ======= GLOBAL ALERT PACKET ========
typedef struct {
  uint8_t cmd;   // 1 = GLOBAL RED ALERT
} alert_cmd_t;

alert_cmd_t alertCmd = {1};

// ======= MQTT RECONNECT ========
void mqttReconnect() {
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Connecting...");
    if (mqtt.connect("UniversalGateway01")) {
      Serial.println("OK");
    } else {
      Serial.print("FAIL rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }
}

// ======= ESPNOW RX ========
void onEspNowRecv(const esp_now_recv_info_t *info,
                  const uint8_t *incomingData,
                  int len) {

  if (len != sizeof(pkt_t)) return;

  memcpy(&lastPkt, incomingData, sizeof(pkt_t));
  memcpy(lastPkt.id, info->src_addr, 6);

  Serial.println("\n[ESPNOW] Packet received");
  Serial.printf(" MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
    lastPkt.id[0], lastPkt.id[1], lastPkt.id[2],
    lastPkt.id[3], lastPkt.id[4], lastPkt.id[5]);

  Serial.printf(" T=%.2f  H=%.2f  Alert=%u  TS=%lu\n",
    lastPkt.t, lastPkt.h, lastPkt.alert, lastPkt.ts);

  // 🚨 IF ANY NODE ALERTS → GLOBAL ALERT
  if (lastPkt.alert != 0) {
    Serial.println("🚨 GLOBAL ALERT TRIGGERED!");
    esp_now_send(NULL, (uint8_t*)&alertCmd, sizeof(alertCmd)); // BROADCAST
  }

  newMsg = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== UNIVERSAL GATEWAY START ===");

  // ===== FIX CHANNEL 6 =====
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);

  // ===== CONNECT WIFI =====
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }

  Serial.print("\n[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("[WiFi] MAC: ");
  Serial.println(WiFi.macAddress());

  mqtt.setServer(mqtt_server, 1883);
  mqttReconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] Init FAILED");
    while(true);
  }

  esp_now_register_recv_cb(onEspNowRecv);

  Serial.println("=== GATEWAY READY (GLOBAL ALERT ENABLED) ===");
}

void loop() {
  mqtt.loop();

  if (newMsg) {
    newMsg = false;

    char topic[64];
    sprintf(topic, "iot/nodes/%02X%02X%02X",
      lastPkt.id[3], lastPkt.id[4], lastPkt.id[5]);

    char payload[150];
    sprintf(payload,
      "{\"t\":%.2f,\"h\":%.2f,\"a\":%u,\"ts\":%lu}",
      lastPkt.t, lastPkt.h, lastPkt.alert, lastPkt.ts);

    Serial.print("[MQTT] Publish → ");
    Serial.println(topic);
    Serial.println(payload);

    if (!mqtt.connected()) mqttReconnect();
    mqtt.publish(topic, payload);
  }
}