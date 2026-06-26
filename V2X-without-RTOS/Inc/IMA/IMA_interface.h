#ifndef IMA_INTERFACE_H
#define IMA_INTERFACE_H

#include <stdint.h>
#include "../DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/**
 * @brief Initialize the IMA module
 */
void IMA_voidInit(void);

/**
 * @brief Get current IMA flag (for DSRC flag broadcast)
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t IMA_u8GetFlag(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — reset accumulators
 *        No sensor parameters needed; IMA reads Host_Speed and
 *        Host_DistToIntersection from shared globals.
 */
void IMA_voidBeginCycle(void);

/**
 * @brief Process one CROSSING-direction DSRC neighbor for IMA.
 *        The SafetyEngine only dispatches crossing-direction neighbors here.
 * @param n Pointer to neighbor data
 */
void IMA_voidProcessNeighbor(const Neighbor *n);

#endif
