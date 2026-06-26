#ifndef BSW_INTERFACE_H
#define BSW_INTERFACE_H

#include <stdint.h>
#include "../DSRC/DSRC.h"
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
 * @brief Get current BSW left flag (for DSRC flag broadcast)
 * @return 0=No vehicle, 1=Blind-spot detected
 */
uint8_t BSW_u8GetLeftFlag(void);

/**
 * @brief Get current BSW right flag (for DSRC flag broadcast)
 * @return 0=No vehicle, 1=Blind-spot detected
 */
uint8_t BSW_u8GetRightFlag(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — check side distances, reset accumulators
 * @param left_dist   Min of front-left and rear-left US distance (cm)
 * @param right_dist  Min of front-right and rear-right US distance (cm)
 */
void BSW_voidBeginCycle(float left_dist, float right_dist);

/**
 * @brief Process one DSRC neighbor for BSW
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 *
 * Only same-direction neighbors confirm a blind-spot detection.
 */
void BSW_voidProcessNeighbor(const Neighbor *n, Direction_t dir);

/**
 * @brief End cycle — finalize flags and activate/deactivate alerts
 */
void BSW_voidEndCycle(void);

#endif
