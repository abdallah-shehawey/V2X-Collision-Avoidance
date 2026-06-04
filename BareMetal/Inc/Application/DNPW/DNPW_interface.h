#ifndef DNPW_INTERFACE_H
#define DNPW_INTERFACE_H

#include <stdint.h>
#include "../DSRC/DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/**
 * @brief Initialize the DNPW module
 */
void DNPW_voidInit(void);

/**
 * @brief Standalone DNPW update (iterates neighbor table internally)
 *        Use this OR the BeginCycle/ProcessNeighbor/EndCycle API, not both.
 */
void DNPW_voidUpdate(void);

/**
 * @brief Get current DNPW flag (for DSRC flag broadcast)
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t DNPW_u8GetFlag(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — save front distance, reset accumulators
 * @param front_dist Front ultrasonic distance (cm)
 */
void DNPW_voidBeginCycle(float front_dist);

/**
 * @brief Process one DSRC neighbor for DNPW
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 *
 * Only opposite-direction neighbors are evaluated (oncoming traffic threat).
 */
void DNPW_voidProcessNeighbor(const Neighbor *n, Direction_t dir);

/**
 * @brief End cycle — apply 3-gate decision and activate/deactivate alerts
 */
void DNPW_voidEndCycle(void);

#endif