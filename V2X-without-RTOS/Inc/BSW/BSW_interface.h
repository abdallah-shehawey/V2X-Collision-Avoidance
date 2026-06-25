#ifndef BSW_INTERFACE_H
#define BSW_INTERFACE_H

#include <stdint.h>
#include "../DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/**
 * @brief Initialize the BSW module
 */
void BSW_voidInit(void);

/**
 * @brief Standalone BSW update (iterates neighbor table internally)
 *        Use this OR the BeginCycle/ProcessNeighbor/EndCycle API, not both.
 */
void BSW_voidUpdate(void);

/**
 * @brief Get current BSW flag to broadcast over DSRC (sender's own side).
 * @return 0=None, 1=object on my LEFT (front-left), 2=object on my RIGHT (front-right)
 */
uint8_t BSW_u8GetFlag(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — latch side distances, reset accumulators
 *
 * Sender role uses the FRONT-side sensors; receiver role uses the REAR-side
 * sensors. All four are needed, so they are passed separately (no min()).
 *
 * @param front_left   Front-left ultrasonic distance (cm)
 * @param front_right  Front-right ultrasonic distance (cm)
 * @param rear_left    Rear-left ultrasonic distance (cm)
 * @param rear_right   Rear-right ultrasonic distance (cm)
 */
void BSW_voidBeginCycle(float front_left, float front_right,
                        float rear_left, float rear_right);

/**
 * @brief Process one DSRC neighbor for BSW (receiver role)
 *
 * If the neighbor broadcasts a bsw_flag, check the OPPOSITE rear side to see
 * whether this car is the one sitting in that neighbor's blind spot.
 *
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 */
void BSW_voidProcessNeighbor(const Neighbor *n, Direction_t dir);

/**
 * @brief End cycle — finalize sender flag and activate/deactivate alerts
 */
void BSW_voidEndCycle(void);

#endif
