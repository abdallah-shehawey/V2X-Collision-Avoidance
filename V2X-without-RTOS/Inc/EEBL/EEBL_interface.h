#ifndef EEBL_INTERFACE_H
#define EEBL_INTERFACE_H

#include <stdint.h>
#include "../DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/**
 * @brief Initialize the EEBL module
 */
void EEBL_voidInit(void);

/**
 * @brief Get current EEBL risk level. The LED/buzzer is driven by the caller.
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t EEBL_u8GetFlag(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — check braking gate, reset accumulator
 */
void EEBL_voidBeginCycle(void);

/**
 * @brief Process one SAME-direction DSRC neighbor for EEBL.
 *        The SafetyEngine only dispatches same-direction neighbors here.
 * @param rear_distance Rear ultrasonic distance (cm)
 */
void EEBL_voidProcessNeighbor(float rear_distance);

#endif
