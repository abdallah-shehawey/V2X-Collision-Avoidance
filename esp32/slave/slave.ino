#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "esp_wifi.h"

#define RXD1 16
#define TXD1 17
#define START_BYTE 0xAA
#define END_BYTE 0x55
#define ESPNOW_CHANNEL 6
#define VEHICLE_ID 2

// ====== Struct ======
typedef struct
{
  uint8_t vehicle_id;
  float pos_x;
  float pos_y;
  float speed;
  float heading;
  uint32_t last_update;
} Neighbor;

// ============================================================
// UART State Machine
// Receives data from STM32 byte by byte
// Once a valid packet is received, forward it via ESP-NOW
// ============================================================
typedef enum
{
  WAIT_START,
  READ_DATA,
  READ_CHECKSUM,
  READ_END
} ParseState;

ParseState parse_state = WAIT_START;
uint8_t rx_buf[sizeof(Neighbor)];
uint8_t rx_idx = 0;
uint8_t rx_checksum = 0;

// ====== Globals ======
uint8_t broadcast_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ====== Checksum ======
uint8_t calc_checksum(uint8_t *data, uint8_t len)
{
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    sum ^= data[i];
  }
  return sum;
}

// ====== Send to STM32 via UART ======
void send_to_stm32(Neighbor *n)
{
  uint8_t raw[sizeof(Neighbor)];
  memcpy(raw, n, sizeof(Neighbor));
  uint8_t chk = calc_checksum(raw, sizeof(Neighbor));

  Serial1.write(START_BYTE);
  Serial1.write(raw, sizeof(Neighbor));
  Serial1.write(chk);
  Serial1.write(END_BYTE);
}

// ====== Send via ESP-NOW ======
void send_espnow(Neighbor *n)
{
  esp_now_send(broadcast_addr, (uint8_t *)n, sizeof(Neighbor));
}

// ============================================================
// ESP-NOW Callbacks
// ============================================================
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "[ESP-NOW] TX: OK" : "[ESP-NOW] TX: FAIL");
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  if (len < (int)sizeof(Neighbor))
    return;

  Neighbor n;
  memcpy(&n, data, sizeof(Neighbor));

  // ignore own vehicle
  if (n.vehicle_id == VEHICLE_ID)
  {
    return;
  }
  // print received data from ESP-NOW
  Serial.println("====== [ESP-NOW RX] ======");
  Serial.printf("Vehicle ID : %d\n", n.vehicle_id);
  Serial.printf("Pos X      : %.2f\n", n.pos_x);
  Serial.printf("Pos Y      : %.2f\n", n.pos_y);
  Serial.printf("Speed      : %.2f\n", n.speed);
  Serial.printf("Heading    : %.2f\n", n.heading);
  Serial.printf("Last Update: %lu\n", n.last_update);
  Serial.println("==========================");

  // forward to STM32 via UART
  send_to_stm32(&n);
  Serial.println("[UART] Forwarded to STM32");
}

void parse_byte(uint8_t byte)
{
  switch (parse_state)
  {
  case WAIT_START:
    if (byte == START_BYTE)
    {
      rx_idx = 0;
      parse_state = READ_DATA;
    }
    break;

  case READ_DATA:
    rx_buf[rx_idx++] = byte;
    if (rx_idx >= sizeof(Neighbor))
    {
      parse_state = READ_CHECKSUM;
    }
    break;

  case READ_CHECKSUM:
    rx_checksum = byte;
    parse_state = READ_END;
    break;

  case READ_END:
    if (byte == END_BYTE)
    {
      uint8_t expected = calc_checksum(rx_buf, sizeof(Neighbor));
      if (expected == rx_checksum)
      {
        Neighbor n;
        memcpy(&n, rx_buf, sizeof(Neighbor));

        // print received data from STM32
        Serial.println("====== [UART RX] ======");
        Serial.printf("Vehicle ID : %d\n", n.vehicle_id);
        Serial.printf("Pos X      : %.2f\n", n.pos_x);
        Serial.printf("Pos Y      : %.2f\n", n.pos_y);
        Serial.printf("Speed      : %.2f\n", n.speed);
        Serial.printf("Heading    : %.2f\n", n.heading);
        Serial.printf("Last Update: %lu\n", n.last_update);
        Serial.println("=======================");

        // forward to other ESP32 via ESP-NOW
        send_espnow(&n);
        Serial.println("[ESP-NOW] Forwarded");
      }
      else
      {
        Serial.printf("[ERR] Checksum mismatch! got 0x%02X expected 0x%02X\n", rx_checksum, calc_checksum(rx_buf, sizeof(Neighbor)));
      }
    }
    else
    {
      Serial.println("[ERR] Bad END byte");
    }
    parse_state = WAIT_START;
    break;
  }
}

// ============================================================
// Setup & Loop
// ============================================================
void setup()
{
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // add broadcast peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcast_addr, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.printf("sizeof(Neighbor) = %d\n", sizeof(Neighbor));
  Serial.printf("V2V Ready - Vehicle %d\n", VEHICLE_ID);
}

void loop()
{
  // read from STM32 and forward via ESP-NOW
  while (Serial1.available())
  {
    parse_byte(Serial1.read());
  }
}