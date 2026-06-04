#ifndef IMA_INTERFACE_H
#define IMA_INTERFACE_H

#include <stdint.h>
#include "../DSRC/DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/**
 * @brief Initialize the IMA module
 */
void IMA_voidInit(void);

/**
 * @brief Standalone IMA update (iterates neighbor table internally)
 *        Use this OR the BeginCycle/ProcessNeighbor/EndCycle API, not both.
 */
void IMA_voidUpdate(void);

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
 * @brief Process one DSRC neighbor for IMA
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 *
 * Only crossing-direction neighbors are evaluated (cross-traffic at intersection).
 */
void IMA_voidProcessNeighbor(const Neighbor *n, Direction_t dir);

/**
 * @brief End cycle — apply decision logic and activate/deactivate alerts
 */
void IMA_voidEndCycle(void);

#endif
