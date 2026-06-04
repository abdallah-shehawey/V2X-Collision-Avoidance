#ifndef EEBL_INTERFACE_H
#define EEBL_INTERFACE_H

#include <stdint.h>
#include "../DSRC/DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/**
 * @brief Initialize the EEBL module
 */
void EEBL_voidInit(void);

/**
 * @brief Standalone EEBL update (iterates neighbor table internally)
 *        Use this OR the BeginCycle/ProcessNeighbor/EndCycle API, not both.
 *        (Local-only: no flag broadcast via DSRC)
 */
void EEBL_voidUpdate(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — check braking gate, reset accumulators
 */
void EEBL_voidBeginCycle(void);

/**
 * @brief Process one DSRC neighbor for EEBL
 * @param n             Pointer to neighbor data
 * @param rear_distance Rear ultrasonic distance (cm)
 * @param dir           Pre-computed direction (from SafetyEngine_DetectDirection)
 */
void EEBL_voidProcessNeighbor(const Neighbor *n, float rear_distance, Direction_t dir);

/**
 * @brief End cycle — activate/deactivate alerts based on results
 */
void EEBL_voidEndCycle(void);

#endif
