/**
 ******************************************************************************
 * @file           : DSRC.h
 * @author         : Abdallah Shehawey
 * @brief          : DSRC module implementation
 ******************************************************************************
 **/

#ifndef DSRC_H
#define DSRC_H

#include <stdint.h>
#include <string.h>
#include <math.h>

// ====== Config ======
#define MAX_NEIGHBORS 20
#define NEIGHBOR_TIMEOUT 2000
#define VEHICLE_ID 1
#define START_BYTE 0xAA
#define END_BYTE 0x55
#define PACKET_SIZE (1 + sizeof(Neighbor) + 1 + 1)
#define QUEUE_SIZE 10

// ====== Struct ======
typedef struct
{
  uint8_t vehicle_id;
  float pos_x;
  float pos_y;
  float pos_z;
  float speed;
  float heading;
  uint32_t last_update;
} Neighbor;

// ====== Public API ======
void DSRC_Init(void);
void DSRC_SendNeighbor(Neighbor *n);
void DSRC_Update(uint32_t current_time);
void DSRC_RemoveStale(uint32_t current_time);
uint8_t DSRC_GetCount(void);
Neighbor *DSRC_GetTable(void);

// ====== UART RX Callback - call from USART_RXCMP ======
void DSRC_RxCallback(uint8_t byte);

#endif // DSRC_H
