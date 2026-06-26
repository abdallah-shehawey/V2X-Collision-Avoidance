#ifndef FCW_DNPW_INTERFACE_H
#define FCW_DNPW_INTERFACE_H

#include <stdint.h>
#include "../DSRC/DSRC.h"
#include "../SafetyEngine/SafetyEngine_interface.h"

/*
 * Cooperative FCW + DNPW module.
 *
 * The SafetyEngine feeds it the cycle's signals by direction; the getters
 * derive the three flags on demand (no EndCycle). Of the three, only
 * fcw_headon_flag is broadcast over DSRC — it is the cooperative signal.
 */

/**
 * @brief Initialize the FCW/DNPW module.
 */
void FCW_DNPW_voidInit(void);

/* ===== Per-Neighbor API (used by SafetyEngine) ===== */

/**
 * @brief Begin a cycle: latch the front distance and reset the signals.
 *        The SafetyEngine sets the cycle safe/critical gaps beforehand.
 * @param front_distance Front ultrasonic distance (cm)
 */
void FCW_DNPW_voidBeginCycle(float front_distance);

/**
 * @brief Feed one same-direction neighbor (confirms a vehicle is ahead).
 */
void FCW_DNPW_voidProcessSameDirection(void);

/**
 * @brief Feed one opposite-direction neighbor (records it and its head-on flag).
 * @param n Pointer to neighbor data
 */
void FCW_DNPW_voidProcessOppositeDirection(const Neighbor *n);

/* ===== Public Getters (derive the results on demand) ===== */

/**
 * @brief Forward-collision flag (vehicle ahead, same lane). Local only.
 * @return 0=Safe, 1=Warning, 2=Critical
 */
uint8_t FCW_GetFrontFlag(void);

/**
 * @brief Head-on candidate flag (vehicle ahead + oncoming vehicle). Broadcast
 *        as fcw_headon_flag for the cooperative decision.
 * @return 0=no candidate, 1=candidate
 */
uint8_t FCW_GetHeadonFlag(void);

/**
 * @brief Confirmed head-on collision (our candidate and the oncoming car's).
 * @return 0=Safe, else the front-distance severity (1=Warning, 2=Critical)
 */
uint8_t FCW_GetHeadonConfirmed(void);

/**
 * @brief Do-Not-Pass flag (candidate but the oncoming car is in another lane).
 *        Presence only — no severity (the front distance is not the oncoming car).
 * @return 0=no warning, 1=do not pass
 */
uint8_t DNPW_GetFlag(void);

#endif
