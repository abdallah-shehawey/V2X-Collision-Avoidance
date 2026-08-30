/**
 ******************************************************************************
 * @file    DSRC.h
 * @author  Abdallah Shehawey
 * @brief   The V2V transport: the neighbor table and the over-the-air frame
 *          format shared with the ESP32 radio bridge.
 * @ingroup app_dsrc
 *
 * @details
 * DSRC is the layer every ADAS module sits on. It answers one question — *which
 * other vehicles are near me right now, and what are they doing* — and keeps the
 * answer in a table of at most @ref MAX_NEIGHBORS entries, one per vehicle heard
 * from recently.
 *
 * @section dsrc_flow The path a message takes
 *
 * The STM32 has no radio of its own. An ESP32 bridges the air interface:
 *
 * @verbatim
 *   other car --ESP-NOW--> our ESP32 --UART1--> STM32   (receive)
 *   STM32 --UART1--> our ESP32 --ESP-NOW--> other cars  (transmit)
 * @endverbatim
 *
 * On the way in, USART1's ISR only enqueues raw bytes; @ref DSRC_RxCallback is
 * then driven from `vTask_ESP_Comm` in task context, one byte at a time, and
 * reassembles frames. On the way out, @ref DSRC_SendNeighbor frames this
 * vehicle's own @ref Neighbor struct and hands it to the ESP32 every 100 ms.
 *
 * @section dsrc_frame Frame layout
 *
 * @verbatim
 *   +------------+---------------------------+----------+----------+
 *   | START_BYTE |   Neighbor (21 bytes)     | checksum | END_BYTE |
 *   |    0xAA    |        packed             |  1 byte  |   0x55   |
 *   +------------+---------------------------+----------+----------+
 * @endverbatim
 *
 * A frame is only accepted if the start byte, the checksum and the end byte all
 * agree, so a truncated or corrupted frame is dropped rather than acted on.
 *
 * @warning The @ref Neighbor layout is a **wire format**, mirrored byte for byte
 *          in the ESP32 sketches (`esp32/master`, `esp32/sniffer`). Reordering,
 *          resizing or inserting a field on one side and not the other silently
 *          misaligns every field after it — the link keeps working and the data
 *          becomes nonsense. Change both sides together, always.
 ******************************************************************************
 **/

#ifndef DSRC_H
#define DSRC_H

#include <math.h>
#include <stdint.h>
#include <string.h>

/**
 * @addtogroup app_dsrc
 * @{
 */

/**
 * @name Protocol configuration
 * @{
 */
/** @brief Maximum number of vehicles tracked at once; further senders are ignored until a slot frees up. */
#define MAX_NEIGHBORS 20

/** @brief How long a neighbor may stay silent before it is purged from the table [ms]. */
#define NEIGHBOR_TIMEOUT 2000

/**
 * @brief This vehicle's own identity on the air.
 * @warning Must be **unique across the fleet**. Two cars sharing an ID overwrite
 *          each other's row in every other car's neighbor table.
 */
#define VEHICLE_ID 2

#define START_BYTE 0xAA /**< First byte of every frame — see @ref dsrc_frame. */
#define END_BYTE   0x55 /**< Last byte of every frame. */

/** @brief Total frame length: start byte + payload + checksum + end byte. */
#define PACKET_SIZE (1 + sizeof(Neighbor) + 1 + 1)

/** @brief Depth of the parser's internal byte queue. */
#define QUEUE_SIZE 10
/** @} */

/**
 * @brief One vehicle's broadcast state — this is the wire format itself.
 *
 * Packed to exactly 21 bytes and sent as raw bytes, so the compiler must not
 * insert padding and both sides of the link must agree on the layout. See the
 * warning in the file header before touching any field.
 *
 * The struct does double duty: it is both the row type of the neighbor table
 * (what we know about *them*) and the payload this vehicle broadcasts (what we
 * tell *them* about us).
 */
typedef struct __attribute__((packed))
{
  uint8_t  vehicle_id;  /**< Who sent this — see @ref VEHICLE_ID. */
  float    speed;       /**< The sender's ground speed [cm/s]. */
  float    heading;     /**< The sender's compass heading [degrees], 0..360. */
  uint32_t last_update; /**< Local tick at which this row was last refreshed; drives the @ref NEIGHBOR_TIMEOUT purge. */

  /**
   * @brief Head-on candidate flag: 0 = no, 1 = yes.
   *
   * Set when the sender sees a vehicle ahead *and* a neighbor coming the other
   * way. It is broadcast rather than acted on alone, so the *other* car can
   * confirm the same geometry from its own side — a head-on is the one case
   * where a single car's view is not enough to be sure.
   */
  uint8_t fcw_headon_flag;

  /**
   * @brief Which of the sender's **front** sides is occupied.
   *
   * A bit mask: bit 0 = LEFT, bit 1 = RIGHT. So 0 = neither, 1 = left only,
   * 2 = right only, 3 = both. The receiver mirrors this against its own *rear*
   * sensors to work out whether it is sitting in the sender's blind spot.
   */
  uint8_t bsw_flag;

  float   distance_to_intersection; /**< The sender's distance to the nearest intersection [cm]; 0 means it is not near one. */
  uint8_t ima_flag;                 /**< The sender's own IMA verdict: 0 = safe, 1 = warning, 2 = critical. */
} Neighbor;

/*============================================================================*/
/*                                PUBLIC API                                  */
/*============================================================================*/

/** @brief Clear the neighbor table and reset the frame parser. Call once, before the scheduler starts. */
void DSRC_Init(void);

/**
 * @brief Frame and transmit one @ref Neighbor over UART1 to the ESP32 bridge.
 *
 * Wraps @p n in the start byte, checksum and end byte described in
 * @ref dsrc_frame. Called every 100 ms by `vTask_ESP_Comm` with this vehicle's
 * own state.
 *
 * @param[in] n The vehicle state to broadcast.
 */
void DSRC_SendNeighbor(Neighbor *n);

/**
 * @brief Drain the parser's byte queue and fold any complete frames into the table.
 *
 * A sender already in the table updates its existing row; a new sender takes a
 * free slot, if one is left. Call once per `vTask_ESP_Comm` cycle.
 */
void DSRC_Update(void);

/**
 * @brief Drop every neighbor that has been silent for @ref NEIGHBOR_TIMEOUT.
 *
 * Without this, a car that drove away — or whose radio died — would linger in
 * the table forever and keep triggering warnings about a vehicle that is no
 * longer there.
 *
 * @param current_time The current tick count, in the same units as
 *                     `Neighbor::last_update`.
 */
void DSRC_RemoveStale(uint32_t current_time);

/**
 * @brief How many neighbors are currently in the table.
 * @return A count in the range 0..@ref MAX_NEIGHBORS.
 */
uint8_t DSRC_GetCount(void);

/**
 * @brief Borrow the neighbor table itself.
 *
 * Returns a pointer to the live array, not a copy — the first @ref DSRC_GetCount
 * entries are valid.
 *
 * @return Pointer to the first row of the table.
 * @warning The caller must already hold the neighbor-table mutex, and must not
 *          keep the pointer past releasing it: @ref DSRC_Update and
 *          @ref DSRC_RemoveStale both rewrite this array.
 */
Neighbor *DSRC_GetTable(void);

/**
 * @brief Feed one received byte into the frame parser.
 *
 * @param byte The next byte that arrived from the ESP32 on UART1.
 *
 * @warning Call this from **task context only** (`vTask_ESP_Comm`), never from
 *          the USART1 ISR. It mutates the parser's non-atomic internal queue, so
 *          an ISR calling it while the task is mid-parse is a data race. The ISR's
 *          job is only to push the raw byte onto @ref G_xESP_RX_Queue.
 */
void DSRC_RxCallback(uint8_t byte);

/** @} */ /* end of app_dsrc */

#endif // DSRC_H
