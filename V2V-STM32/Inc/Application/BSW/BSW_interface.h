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
 * @brief Get current BSW flag to broadcast over DSRC (sender's own front side(s)).
 * @return bit0 = LEFT, bit1 = RIGHT (0=none, 1=LEFT, 2=RIGHT, 3=both)
 */
uint8_t BSW_u8GetFlag(void);

/**
 * @brief Get the receiver-side blind-spot result for THIS car (which side has
 *        a vehicle in the blind spot). The LED/buzzer is driven elsewhere by
 *        the caller — this module only computes the result.
 * @return bit0 = LEFT, bit1 = RIGHT (0=none, 1=LEFT, 2=RIGHT, 3=both)
 */
uint8_t BSW_u8GetBlindSpot(void);

/**
 * @brief Get the receiver-side blind-spot SEVERITY for THIS car (worst side),
 *        graded by how close the car behind us is.
 * @return 0 = safe, 1 = warning (< BSW_SIDE_THRESHOLD), 2 = critical (< BSW_SIDE_CRITICAL)
 */
uint8_t BSW_u8GetSeverity(void);

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
 * For each side bit the neighbor broadcasts, check our mirrored rear sensor to
 * see whether this car sits in that neighbor's blind spot. The bits are tested
 * independently, so a both-sides flag runs both checks.
 *
 * @param n   Pointer to neighbor data
 */
void BSW_voidProcessNeighbor(const Neighbor *n);

#endif
