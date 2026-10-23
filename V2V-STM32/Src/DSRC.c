/**
 ******************************************************************************
 * @file    DSRC.c
 * @author  Abdallah Shehawey
 * @brief   Neighbor table, frame parser and transmitter for the V2V link.
 * @ingroup app_dsrc
 *
 * @details
 * Three pieces live here:
 *
 * 1. a **byte-at-a-time frame parser** (@ref DSRC_RxCallback), written as an
 *    explicit state machine so a garbled or half-received frame can never leave
 *    the parser stuck — it simply falls back to @ref WAIT_START;
 * 2. a small **queue** of successfully parsed frames, which decouples parsing
 *    from the table update;
 * 3. the **neighbor table** itself, one row per vehicle heard from recently.
 *
 * See @ref dsrc_frame in DSRC.h for the frame layout.
 ******************************************************************************
 **/

#include "../Inc/Drivers/MCAL/USART/USART_intreface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "FreeRTOS.h"
#include "task.h"

/** @brief USART1 — the link to the ESP32 radio bridge. Defined in `System.c`. */
extern USART_Handle_t USART_1;

/** @brief The neighbor table: one row per vehicle heard from recently. */
static Neighbor neighbor_table[MAX_NEIGHBORS];

/** @brief How many rows of @ref neighbor_table are in use. */
static uint8_t neighbor_count = 0;

/**
 * @name Parsed-frame queue
 *
 * A single-producer / single-consumer ring buffer sitting between the parser and
 * @ref DSRC_Update. It is deliberately allowed to *drop* a frame when full rather
 * than block or overwrite: losing one 100 ms broadcast is harmless, since the
 * sender repeats it, whereas stalling the parser would back up the UART.
 * @{
 */
static Neighbor         rx_queue[QUEUE_SIZE]; /**< The queued frames. */
static volatile uint8_t queue_head = 0;       /**< Read index — advanced by @ref queue_pop. */
static volatile uint8_t queue_tail = 0;       /**< Write index — advanced by @ref queue_push. */
/** @} */

/**
 * @brief States of the receive state machine, in the order a good frame walks them.
 */
typedef enum
{
  WAIT_START,    /**< Discarding bytes until @ref START_BYTE shows up. */
  READ_DATA,     /**< Collecting the `sizeof(Neighbor)` payload bytes. */
  READ_CHECKSUM, /**< Reading the checksum byte and comparing it with the running XOR. */
  READ_END       /**< Expecting @ref END_BYTE; only now is the frame accepted. */
} ParseState;

/** @brief Where the frame parser currently is. */
static ParseState parse_state = WAIT_START;

/** @brief Payload bytes collected so far during @ref READ_DATA. */
static uint8_t rx_buf[sizeof(Neighbor)];

/** @brief How many payload bytes @ref rx_buf currently holds. */
static uint8_t rx_idx = 0;

/** @brief Checksum byte received, held until the frame is validated. */
static uint8_t rx_checksum = 0;

/**
 * @brief XOR checksum over a byte range — the same one the ESP32 computes.
 * @param[in] data First byte to fold in.
 * @param     len  Number of bytes.
 * @return The XOR of all @p len bytes.
 *
 * @note An XOR is weak as error detection goes: it catches any single-bit error
 *       and any odd number of flipped bits, but two bits flipped in the same
 *       column cancel out. It is what the ESP32 side computes, so the two must
 *       match.
 */
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

/**
 * @brief Append a parsed frame to @ref rx_queue.
 * @param[in] n The frame to enqueue.
 * @note If the queue is full the frame is **dropped silently**. That is the
 *       intended behaviour — see the note on the queue itself.
 */
static void queue_push(Neighbor *n)
{
  uint8_t next = (queue_tail + 1) % QUEUE_SIZE;
  if (next != queue_head) // not full
  {
    rx_queue[queue_tail] = *n;
    queue_tail = next;
  }
}

/**
 * @brief Take the oldest frame out of @ref rx_queue.
 * @param[out] out Receives the frame; untouched when the queue is empty.
 * @retval 1 A frame was dequeued into @p out.
 * @retval 0 The queue was empty.
 */
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
/**
 * @brief Fold one received frame into the neighbor table.
 *
 * A sender already in the table overwrites its own row; a new sender takes the
 * next free slot. Once the table holds @ref MAX_NEIGHBORS vehicles, further new
 * senders are ignored.
 *
 * @param[in] msg The frame to merge in.
 *
 * @note The row is stamped with the **local** FreeRTOS tick, not the sender's
 *       `last_update`. The sender's timestamp comes from its own clock, which has
 *       no fixed relationship to ours, so it cannot be compared against anything
 *       here — @ref DSRC_RemoveStale would either purge everyone or no one.
 */
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
  uint8_t  oldest_i = 0;
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

  USART_enumTransmit((USART_Config_t *)&USART_1, START_BYTE);
  for (uint8_t i = 0; i < sizeof(Neighbor); i++)
  {
    USART_enumTransmit((USART_Config_t *)&USART_1, raw[i]);
  }
  USART_enumTransmit((USART_Config_t *)&USART_1, chk);
  USART_enumTransmit((USART_Config_t *)&USART_1, END_BYTE);
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
