/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    FCW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : FCW (Cooperative Forward Collision Warning)     **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/FCW/FCW_interface.h"
#include "../Inc/FCW/FCW_config.h"
#include "../Inc/FCW/FCW_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static uint8_t    FCW_CurrentFlag = 0; /* 0=Safe, 1=Warning, 2=Critical */

/* Cycle accumulators (set during BeginCycle, used during ProcessNeighbor) */
static RiskLevel_t FCW_LocalWorst = RISK_SAFE; /* local front-US risk → DSRC flag */
static RiskLevel_t FCW_AlertLevel = RISK_SAFE; /* confirmed risk → drives the alert */

/* ============ Init ============ */
void FCW_voidInit(void)
{
  FCW_CurrentFlag = 0;
  FCW_LocalWorst  = RISK_SAFE;
  FCW_AlertLevel  = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new processing cycle
 *
 * Local FCW detection is purely distance-based and needs no neighbor data,
 * so we compute it here from the host speed and front ultrasonic distance.
 *   - FCW_LocalWorst  → broadcast as fcw_flag (so others can confirm)
 *   - FCW_AlertLevel  → for a vehicle AHEAD in the SAME direction the local
 *                       detection is enough to alert immediately.
 *                       For oncoming (opposite) traffic the alert waits for
 *                       cooperative confirmation in ProcessNeighbor.
 *
 * @param front_distance Front ultrasonic distance (cm)
 */
void FCW_voidBeginCycle(float front_distance)
{
  FCW_LocalWorst = SafetyEngine_AssessDistanceRisk(Host_Speed, front_distance,
                                                   FCW_SAFE_DIST_PER_MS,
                                                   FCW_MIN_SAFE_DISTANCE,
                                                   FCW_CRITICAL_RATIO);

  /* Local detection drives the alert on its own (object directly ahead).
   * Opposite-direction cooperative checks can only raise it further. */
  FCW_AlertLevel = FCW_LocalWorst;
}

/**
 * @brief Process one DSRC neighbor for FCW
 *
 * The local risk is already computed in BeginCycle (distance-based).
 * Here we only add cooperative confirmation for ONCOMING traffic:
 * if we detect danger ahead AND an opposite-direction neighbor also
 * reports an FCW flag, keep/raise the alert. Same-direction and
 * crossing/unknown neighbors need no extra processing.
 */
void FCW_voidProcessNeighbor(const Neighbor *n, float front_distance, Direction_t dir)
{
  (void)front_distance; /* local risk already assessed in BeginCycle */

  /* Cooperative confirmation only applies to oncoming traffic */
  if (dir != DIR_OPPOSITE)
  {
    return;
  }

  /*
   * Cooperative Confirmation (opposite direction):
   * Trust a cooperative alert only when BOTH vehicles see danger.
   * Our own flag (FCW_LocalWorst) is broadcast, so the oncoming car can
   * confirm us next cycle; here we check that it has also reported one.
   */
  if (FCW_LocalWorst > RISK_SAFE && n->fcw_flag > 0)
  {
    if (FCW_LocalWorst > FCW_AlertLevel)
    {
      FCW_AlertLevel = FCW_LocalWorst;
    }
  }
}

/**
 * @brief End cycle — set FCW flag and activate/deactivate alerts
 */
void FCW_voidEndCycle(void)
{
  /*
   * CRITICAL: Always update the flag from local detection.
   * This flag is read by FCW_u8GetFlag() and sent in every DSRC message.
   * Without this, the cooperative confirmation would deadlock.
   */
  FCW_CurrentFlag = (uint8_t)FCW_LocalWorst;

  /* Activate or deactivate alert based on confirmed level */
  if (FCW_AlertLevel > RISK_SAFE)
  {
    FCW_ActivateAlert(FCW_AlertLevel);
  }
  else
  {
    FCW_DeactivateAlert();
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone FCW update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void FCW_voidUpdate(void)
{
  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();
  float front_dist = US_Distances[US_FRONT];

  FCW_voidBeginCycle(front_dist);

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
  }

  FCW_voidEndCycle();
}

/* ============ Public Getter ============ */
uint8_t FCW_u8GetFlag(void)
{
  return FCW_CurrentFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Activate alerts (LED, Buzzer, ADAS) based on risk level
 */
static void FCW_ActivateAlert(RiskLevel_t level)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_ON(); */
#endif

#if FCW_ENABLE_BUZZER
  if (level == RISK_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif

#if FCW_ENABLE_ADAS_REQUEST
  if (level == RISK_CRITICAL)
  {
    /* ADAS_RequestBrake(); */
  }
#endif
}

/**
 * @brief Deactivate all alerts when risk returns to SAFE
 */
static void FCW_DeactivateAlert(void)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_OFF(); */
#endif

#if FCW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
