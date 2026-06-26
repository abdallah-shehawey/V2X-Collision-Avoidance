/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<   DNPW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : DNPW (Do Not Pass Warning)                      **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/DNPW/DNPW_interface.h"
#include "../Inc/DNPW/DNPW_config.h"
#include "../Inc/DNPW/DNPW_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t DNPW_CurrentFlag = 0; /* 0=Safe, 1=Warning, 2=Critical */

/* Cycle accumulators (reset in BeginCycle, used in ProcessNeighbor/EndCycle) */
static uint8_t DNPW_FrontVehicle      = 0; /* US: vehicle ahead (FCW_Local part 1) */
static uint8_t DNPW_OppositeDetected  = 0; /* DSRC: oncoming vehicle  (FCW_Local part 2) */
static uint8_t DNPW_OncomingFollowing = 0; /* DSRC: oncoming car reports fcw_flag (FCW_DSRC) */
static float   DNPW_FrontDist         = 0.0f; /* Front US distance (saved from BeginCycle) */

/* ============ Init ============ */
void DNPW_voidInit(void)
{
  DNPW_CurrentFlag      = 0;
  DNPW_FrontVehicle     = 0;
  DNPW_OppositeDetected = 0;
  DNPW_OncomingFollowing = 0;
  DNPW_FrontDist        = 0.0f;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new DNPW processing cycle
 *
 *  Step 1: Check if there is a vehicle ahead (front US gate)
 *  Step 2: Reset DSRC accumulators
 *
 * @param front_dist Front ultrasonic distance (cm)
 */
void DNPW_voidBeginCycle(float front_dist)
{
  /* Save front distance for the distance-based risk assessment in EndCycle */
  DNPW_FrontDist = front_dist;

  /* Gate: is there a vehicle ahead? (driver is behind someone, considering overtaking) */
  if (front_dist > 0.0f && front_dist < DNPW_FRONT_THRESHOLD)
  {
    DNPW_FrontVehicle = 1;
  }
  else
  {
    DNPW_FrontVehicle = 0;
  }

  /* Reset DSRC accumulators */
  DNPW_OppositeDetected  = 0;
  DNPW_OncomingFollowing = 0;
}

/**
 * @brief Process one DSRC neighbor for DNPW
 *
 * Only oncoming (opposite-direction) neighbors matter. For each one we note:
 *   - that an oncoming vehicle exists                 → FCW_Local part 2
 *   - whether that oncoming vehicle reports an FCW    → FCW_DSRC
 *
 * No relative speed, no closing speed, no TTC. The pass/no-pass decision and
 * its severity are taken in EndCycle from host speed + front distance.
 *
 * @param n   Pointer to neighbor data
 * @param dir Pre-computed direction (from SafetyEngine_DetectDirection)
 */
void DNPW_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  /* Only opposite-direction vehicles are relevant to an overtaking decision */
  if (dir != DIR_OPPOSITE)
  {
    return;
  }

  /* An oncoming vehicle exists (completes FCW_Local) */
  DNPW_OppositeDetected = 1;

  /* If this oncoming car is itself following someone (fcw_flag set), then
   * it is NOT just driving in its lane → FCW_DSRC = 1 → not a pure overtake. */
  if (n->fcw_flag > 0)
  {
    DNPW_OncomingFollowing = 1;
  }
}

/**
 * @brief End cycle — overtaking decision (FCW_Local && !FCW_DSRC)
 *
 * FCW_Local = vehicle ahead (DNPW_FrontVehicle) AND oncoming vehicle
 *             (DNPW_OppositeDetected) — this car is considering a pass.
 * FCW_DSRC  = the oncoming vehicle also reports an FCW flag
 *             (DNPW_OncomingFollowing).
 *
 *   Case 1: FCW_Local && FCW_DSRC  → both cars see a car ahead, this is not
 *           a mutual overtake → DNPW stays OFF.
 *   Case 2: FCW_Local && !FCW_DSRC → only this car is overtaking → DNPW ON,
 *           severity from host speed + front distance.
 */
void DNPW_voidEndCycle(void)
{
  uint8_t fcw_local = (DNPW_FrontVehicle && DNPW_OppositeDetected);

  if (fcw_local && !DNPW_OncomingFollowing)
  {
    /* Case 2 — this car is the one overtaking. Grade the danger by how
     * little room is left to complete the pass (speed + front distance). */
    RiskLevel_t level = SafetyEngine_AssessDistanceRisk(Host_Speed, DNPW_FrontDist,
                                                        DNPW_SAFE_DIST_PER_MS,
                                                        DNPW_MIN_SAFE_DISTANCE,
                                                        DNPW_CRITICAL_RATIO);

    /* DNPW is active whenever the overtaking scenario holds; even at SAFE
     * level we still warn "do not pass", so floor the alert at WARNING. */
    if (level < RISK_WARNING)
    {
      level = RISK_WARNING;
    }

    DNPW_CurrentFlag = (uint8_t)level;
    DNPW_ActivateAlert(level);
  }
  else
  {
    /* Case 1 or no overtaking scenario — safe to pass / nothing to warn */
    DNPW_CurrentFlag = 0;
    DNPW_DeactivateAlert();
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone DNPW update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void DNPW_voidUpdate(void)
{
  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();
  float front_dist = US_Distances[US_FRONT];

  DNPW_voidBeginCycle(front_dist);

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
    DNPW_voidProcessNeighbor(&table[i], dir);
  }

  DNPW_voidEndCycle();
}

/* ============ Public Getter ============ */
uint8_t DNPW_u8GetFlag(void)
{
  return DNPW_CurrentFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Activate DNPW alert — warn driver NOT to overtake
 */
static void DNPW_ActivateAlert(RiskLevel_t level)
{
#if DNPW_ENABLE_LED_ALERT
  /* LED_DNPW_ON(); */
  /* LCD_Print("! DO NOT PASS - Oncoming vehicle"); */
#endif

#if DNPW_ENABLE_BUZZER
  if (level == RISK_CRITICAL)
  {
    /* BUZZER_CONTINUOUS(); */
  }
  else
  {
    /* BUZZER_SHORT_BEEP(); */
  }
#endif
}

/**
 * @brief Deactivate DNPW alert — safe to pass
 */
static void DNPW_DeactivateAlert(void)
{
#if DNPW_ENABLE_LED_ALERT
  /* LED_DNPW_OFF(); */
#endif

#if DNPW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
