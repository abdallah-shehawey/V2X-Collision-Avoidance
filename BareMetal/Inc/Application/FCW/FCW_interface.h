#ifndef FCW_INTERFACE_H
#define FCW_INTERFACE_H

#include <stdint.h>
#include "../DSRC/DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

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
 * @brief Get current FCW risk level (for DSRC flag broadcast — local detection)
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t FCW_u8GetFlag(void);

/**
 * @brief Get the confirmed FCW alert level (for feedback: LED/Buzzer/Motor).
 *        For opposite-direction traffic this is only set after cooperative
 *        confirmation (both vehicles report danger).
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t FCW_u8GetAlertLevel(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a new processing cycle — reset accumulators
 */
void FCW_voidBeginCycle(void);

/**
 * @brief Process one DSRC neighbor for FCW
 * @param n              Pointer to neighbor data
 * @param front_distance Front ultrasonic distance (cm)
 * @param dir            Pre-computed direction (from SafetyEngine_DetectDirection)
 */
void FCW_voidProcessNeighbor(const Neighbor *n, float front_distance, Direction_t dir);

/**
 * @brief Local (US-only) forward-obstacle detection — works WITHOUT V2X.
 *        Derives closing speed from the change in front distance, so it reacts
 *        whether the host moves toward an object OR an object approaches the host.
 *        Updates the confirmed alert (drives feedback) only; does NOT touch the
 *        cooperative DSRC broadcast flag. Call once per cycle, between
 *        BeginCycle and EndCycle.
 * @param front_distance Front-center ultrasonic distance (cm)
 * @param dt             Elapsed time since last call (seconds)
 */
void FCW_voidProcessLocal(float front_distance, float dt);

/**
 * @brief End cycle — set FCW flag and activate/deactivate alerts
 */
void FCW_voidEndCycle(void);

#endif
