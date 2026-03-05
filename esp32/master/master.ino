#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

#define ESPNOW_CHANNEL 6
#define VEHICLE_ID 2

typedef struct
{
  uint8_t vehicle_id;
  float pos_x;
  float pos_y;
  float speed;
  float heading;
  uint8_t event_type;
  uint8_t event_level;
} V2V_Message;

V2V_Message tx_msg;
uint8_t broadcast_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  if (len < (int)sizeof(V2V_Message))
    return;

  V2V_Message rx_msg;
  memcpy(&rx_msg, data, sizeof(rx_msg));

  if (rx_msg.vehicle_id == VEHICLE_ID)
    return;

  Serial.printf("RX from car: %d\n", rx_msg.vehicle_id);
  Serial.printf("Speed: %.1f | Heading: %.1f\n", rx_msg.speed, rx_msg.heading);
  Serial.println("-------------------");
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "TX: OK" : "TX: FAIL");
}

void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcast_addr, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.printf("V2V Ready - Vehicle %d\n", VEHICLE_ID);
}

void loop()
{
  tx_msg.vehicle_id = VEHICLE_ID;
  tx_msg.pos_x = random(0, 100);
  tx_msg.pos_y = random(0, 100);
  tx_msg.speed = random(30, 60);
  tx_msg.heading = random(0, 360);
  tx_msg.event_type = 0;
  tx_msg.event_level = 0;

  esp_now_send(broadcast_addr, (uint8_t *)&tx_msg, sizeof(tx_msg));
  delay(1000);
}