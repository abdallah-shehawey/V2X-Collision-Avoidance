/**
 ******************************************************************************
 * @file           : DSRC.c
 * @author         : Abdallah Shehawey
 * @brief          : DSRC module implementation
 ******************************************************************************
 **/

#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "FreeRTOS.h"
#include "task.h"

// ====== External USART handle from system ======
extern USART_Handle_t USART_1;

// ====== Neighbor Table ======
static Neighbor neighbor_table[MAX_NEIGHBORS];
static uint8_t neighbor_count = 0;

// ====== Queue ======
static Neighbor rx_queue[QUEUE_SIZE];
static volatile uint8_t queue_head = 0;
static volatile uint8_t queue_tail = 0;

// ====== State Machine ======
typedef enum
{
  WAIT_START,
  READ_DATA,
  READ_CHECKSUM,
  READ_END
} ParseState;

static ParseState parse_state = WAIT_START;
static uint8_t rx_buf[sizeof(Neighbor)];
static uint8_t rx_idx = 0;
static uint8_t rx_checksum = 0;

// ============================================================
// Private - Checksum
// ============================================================
static uint8_t calc_checksum(uint8_t *data, uint8_t len)
{
  uint8_t sum = 0;
  for (uint8_t i = 0; i < len; i++)
  {
    sum ^= data[i];
  }
  return sum;
}

// ============================================================
// Private - Queue
// ============================================================
static void queue_push(Neighbor *n)
{
  uint8_t next = (queue_tail + 1) % QUEUE_SIZE;
  if (next != queue_head) // not full
  {
    rx_queue[queue_tail] = *n;
    queue_tail = next;
  }
}

static uint8_t queue_pop(Neighbor *out)
{
  if (queue_head == queue_tail)
  {
    // reset to zero when empty
    queue_head = 0;
    queue_tail = 0;
    return 0; // empty
  }
  *out = rx_queue[queue_head];
  queue_head = (queue_head + 1) % QUEUE_SIZE;
  return 1;
}

// ============================================================
// Private - Neighbor Table
// ============================================================
static void update_neighbor(const Neighbor *msg)
{
  /* Stamp with LOCAL FreeRTOS tick so DSRC_RemoveStale compares apples to apples.
   * The sender's last_update is from their own clock and is meaningless here. */
  uint32_t local_time = (uint32_t)xTaskGetTickCount();

  // if exists → update only
  for (uint8_t i = 0; i < neighbor_count; i++)
  {
    if (neighbor_table[i].vehicle_id == msg->vehicle_id)
    {
      neighbor_table[i] = *msg;
      neighbor_table[i].last_update = local_time;
      return;
    }
  }

  // not found → add
  if (neighbor_count < MAX_NEIGHBORS)
  {
    neighbor_table[neighbor_count] = *msg;
    neighbor_table[neighbor_count].last_update = local_time;
    neighbor_count++;
    return;
  }

  // table full → replace oldest
  uint8_t oldest_i = 0;
  uint32_t oldest_time = neighbor_table[0].last_update;
  for (uint8_t i = 1; i < neighbor_count; i++)
  {
    if (neighbor_table[i].last_update < oldest_time)
    {
      oldest_time = neighbor_table[i].last_update;
      oldest_i = i;
    }
  }
  neighbor_table[oldest_i] = *msg;
  neighbor_table[oldest_i].last_update = local_time;
}

// ============================================================
// Public API
// ============================================================
void DSRC_Init(void)
{
  memset(neighbor_table, 0, sizeof(neighbor_table));
  neighbor_count = 0;
  parse_state = WAIT_START;
  queue_head = 0;
  queue_tail = 0;
}

void DSRC_SendNeighbor(Neighbor *n)
{
  uint8_t raw[sizeof(Neighbor)];
  memcpy(raw, n, sizeof(Neighbor));
  uint8_t chk = calc_checksum(raw, sizeof(Neighbor));

  USART_enumTransmit((USART_Config_t*)&USART_1, START_BYTE);
  for (uint8_t i = 0; i < sizeof(Neighbor); i++)
  {
    USART_enumTransmit((USART_Config_t*)&USART_1, raw[i]);
  }
  USART_enumTransmit((USART_Config_t*)&USART_1, chk);
  USART_enumTransmit((USART_Config_t*)&USART_1, END_BYTE);
}

// call this in main loop to process received packets
void DSRC_Update(void)
{
  Neighbor received;
  while (queue_pop(&received))
  {
    update_neighbor(&received);
  }
}

void DSRC_RemoveStale(uint32_t current_time)
{
  for (int i = neighbor_count - 1; i >= 0; i--)
  {
    if (current_time - neighbor_table[i].last_update > NEIGHBOR_TIMEOUT)
    {
      for (uint8_t j = i; j < neighbor_count - 1; j++)
      {
        neighbor_table[j] = neighbor_table[j + 1];
      }
      neighbor_count--;
    }
  }
}

uint8_t DSRC_GetCount(void)
{
  return neighbor_count;
}

/*
  *How to get table to check in program ?
  *Just call DSRC_GetTable() to get pointer to the table, and DSRC_GetCount() to get number of valid entries.
  *For example:
  Neighbor *table = DSRC_GetTable();
  uint8_t   count = DSRC_GetCount();

  for (uint8_t i = 0; i < count; i++)
  {
      table[i].vehicle_id;
      table[i].speed;
      table[i].heading;
      table[i].fcw_headon_flag;
      table[i].bsw_flag;
      table[i].ima_flag;
      // etc...
  }
*/

Neighbor *DSRC_GetTable(void)
{
  return neighbor_table;
}

// Feed one received byte into the parser state machine.
// NOTE: call from task context (vTask_ESP_Comm), NOT from the ISR —
// queue_push() below mutates the non-atomic rx_queue.
void DSRC_RxCallback(uint8_t byte)
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
        // or you can make casting if you prefer for example : queue_push((Neighbor*)rx_buf);
        queue_push(&n);
      }
    }
    parse_state = WAIT_START;
    break;
  }
}
