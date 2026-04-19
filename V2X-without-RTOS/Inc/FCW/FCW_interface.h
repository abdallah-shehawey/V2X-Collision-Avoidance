#ifndef FCW_INTERFACE_H
#define FCW_INTERFACE_H

#include <stdint.h>
#include "../DSRC.h"
#include "../System.h"

/**
 * @brief Initialize the FCW module
 */
void FCW_voidInit(void);

/**
 * @brief Standalone FCW update (iterates neighbor table internally)
 *        Use this OR the BeginCycle/ProcessNeighbor/EndCycle API, not both.
 */
void FCW_voidUpdate(void);

/**
 * @brief Get current FCW risk level (for DSRC flag broadcast)
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t FCW_u8GetFlag(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — reset accumulators
 */
void FCW_voidBeginCycle(void);

/**
 * @brief Process one DSRC neighbor for FCW
 * @param n              Pointer to neighbor data
 * @param front_distance Front ultrasonic distance (cm)
 * @param dir            Pre-computed direction (from System_DetectDirection)
 */
void FCW_voidProcessNeighbor(const Neighbor *n, float front_distance, Direction_t dir);

/**
 * @brief End cycle — set FCW flag and activate/deactivate alerts
 */
void FCW_voidEndCycle(void);

#endif
