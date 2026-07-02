#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

#define ESPNOW_CHANNEL 6

// ====== Struct ======
// MUST match the STM32 Neighbor struct byte-for-byte (V2V-STM32/Inc/Application/DSRC/DSRC.h).
// This node only listens on ESP-NOW and prints, so field order/size/types
// have to be identical or every field will be misread.
typedef struct __attribute__((packed))
{
  uint8_t  vehicle_id;
  float    speed;
  float    heading;                  /* 0 to 360 degrees */
  uint32_t last_update;
  uint8_t  fcw_headon_flag;          /* cooperative head-on candidate: 0/1 */
  uint8_t  bsw_flag;                 /* sender front side(s): bit0=LEFT, bit1=RIGHT */
  float    distance_to_intersection; /* distance to nearest intersection (cm), 0 = not near */
  uint8_t  ima_flag;                 /* 0=Safe, 1=Warning, 2=Critical */
} Neighbor;

// ====== Globals ======
uint8_t broadcast_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================
// ESP-NOW Callbacks
// ============================================================
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  if (len < (int)sizeof(Neighbor))
  {
    return;
  }

  // Sniffer: print every vehicle that transmits, no MAC/ID filtering.
  Neighbor n;
  memcpy(&n, data, sizeof(Neighbor));

  const uint8_t *mac = info->src_addr;

  // print received data from ESP-NOW
  Serial.println("====== [ESP-NOW RX] ======");
  Serial.printf("From MAC   : %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("Vehicle ID : %d\n", n.vehicle_id);
  Serial.printf("Speed      : %.2f\n", n.speed);
  Serial.printf("Heading    : %.2f\n", n.heading);
  Serial.printf("FCW Head-on: %d\n", n.fcw_headon_flag);
  Serial.printf("BSW Flag   : %d\n", n.bsw_flag);
  Serial.printf("Dist Inter : %.2f\n", n.distance_to_intersection);
  Serial.printf("IMA Flag   : %d\n", n.ima_flag);
  Serial.println("==========================");
}

// ============================================================
// Setup & Loop
// ============================================================
void setup()
{
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  // add broadcast peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcast_addr, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.printf("sizeof(Neighbor) = %d\n", sizeof(Neighbor));
  Serial.println("V2V Sniffer Ready - listening to ALL vehicles");
}

void loop()
{
}
