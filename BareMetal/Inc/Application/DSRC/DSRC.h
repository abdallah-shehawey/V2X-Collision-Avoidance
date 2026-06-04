/**
 ******************************************************************************
 * @file           : DSRC.h
 * @author         : Abdallah Shehawey
 * @brief          : DSRC module implementation
 ******************************************************************************
 **/

#ifndef DSRC_H
#define DSRC_H

#include <math.h>
#include <stdint.h>
#include <string.h>

// ====== Config ======
#define MAX_NEIGHBORS 20
#define NEIGHBOR_TIMEOUT 2000 /* ms (ticks @1000Hz): purge a neighbor after 2s of silence */
#define VEHICLE_ID 1
#define START_BYTE 0xAA
#define END_BYTE 0x55
#define PACKET_SIZE (1 + sizeof(Neighbor) + 1 + 1)
#define QUEUE_SIZE 10

// ====== Struct ======
typedef struct
{
  uint8_t vehicle_id;
  float speed;
  float heading;       /* 0 to 360 degrees */
  uint32_t last_update;
  uint8_t fcw_flag;    /* 0=Safe, 1=Warning, 2=Critical */
  uint8_t dnpw_flag;   /* 0=Safe, 1=Warning, 2=Critical */
  float distance_to_intersection; /* Distance to nearest intersection (cm), 0 = not near */
  uint8_t ima_flag;    /* 0=Safe, 1=Warning, 2=Critical */
} Neighbor;

// ====== Public API ======
void DSRC_Init(void);
void DSRC_SendNeighbor(Neighbor *n);
void DSRC_Update(void); // call in main loop
void DSRC_RemoveStale(uint32_t current_time);
uint8_t DSRC_GetCount(void);
Neighbor *DSRC_GetTable(void);

// ====== UART RX Callback — feed ONE received byte into the parser ======
// IMPORTANT: call from TASK context (vTask_ESP_Comm), NOT from the ISR.
// It touches the non-atomic internal rx_queue; calling it from an ISR
// while the task also calls it creates a data race.
void DSRC_RxCallback(uint8_t byte);

#endif // DSRC_H
