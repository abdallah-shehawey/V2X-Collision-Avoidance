/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<<    EEBL_program.c     >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : EEBL (Electronic Emergency Brake Lights)        **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/EEBL/EEBL_interface.h"
#include "../Inc/EEBL/EEBL_config.h"
#include "../Inc/EEBL/EEBL_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static float EEBL_PrevSpeed = 0.0f; /* Previous speed for deceleration detection */

/* Cycle state (set during BeginCycle, used during ProcessNeighbor) */
static uint8_t     EEBL_BrakingDetected = 0;
static RiskLevel_t EEBL_WorstLevel      = RISK_SAFE;

/* ============ Init ============ */
void EEBL_voidInit(void)
{
  EEBL_PrevSpeed       = 0.0f;
  EEBL_BrakingDetected = 0;
  EEBL_WorstLevel      = RISK_SAFE;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new EEBL processing cycle
 *        Checks braking gate and resets accumulators.
 *        If not braking suddenly, ProcessNeighbor will skip all neighbors.
 */
void EEBL_voidBeginCycle(void)
{
  /* Detect sudden braking */
  float decel = Host_Speed - EEBL_PrevSpeed;
  EEBL_BrakingDetected = (decel <= EEBL_DECEL_THRESHOLD) ? 1U : 0U;

  /* Save current speed for next cycle */
  EEBL_PrevSpeed = Host_Speed;

  /* Reset worst level */
  EEBL_WorstLevel = RISK_SAFE;
}

/**
 * @brief Process one DSRC neighbor for EEBL
 *
 * Uses pre-computed direction from SafetyEngine.
 * Risk is based purely on the host's own speed (safe-distance model),
 * NOT on relative speed. Once the host brakes suddenly, any vehicle behind
 * that is closer than the speed-dependent safe distance is flagged.
 *
 * Skips immediately if:
 *   - Not same direction
 *   - Not braking (gate from BeginCycle)
 *   - Rear distance out of range
 */
void EEBL_voidProcessNeighbor(const Neighbor *n, float rear_distance, Direction_t dir)
{
  (void)n; /* Neighbor speed no longer used — gap is judged by host speed only */

  /* Only care about same-direction vehicles */
  if (dir != DIR_SAME)
  {
    return;
  }

  /* Gate: skip if not braking */
  if (!EEBL_BrakingDetected)
  {
    return;
  }

  /* Gate: skip if no vehicle behind or out of range */
  if (rear_distance <= 0.0f || rear_distance > EEBL_MAX_DETECTION_RANGE)
  {
    return;
  }

  RiskLevel_t level = EEBL_EvaluateGap(rear_distance);

  if (level > EEBL_WorstLevel)
  {
    EEBL_WorstLevel = level;
  }
}

/**
 * @brief End cycle — activate/deactivate alerts based on results
 */
void EEBL_voidEndCycle(void)
{
  if (EEBL_WorstLevel > RISK_SAFE)
  {
    EEBL_ActivateAlert(EEBL_WorstLevel);
  }
  else
  {
    EEBL_DeactivateAlert();
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone EEBL update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void EEBL_voidUpdate(void)
{
  Neighbor *table = DSRC_GetTable();
  uint8_t count   = DSRC_GetCount();
  float rear_dist = US_Distances[US_REAR];

  EEBL_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
    EEBL_voidProcessNeighbor(&table[i], rear_dist, dir);
  }

  EEBL_voidEndCycle();
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Compute the speed-dependent safe distance (cm) from host speed.
 *        Linear model: safe_cm = speed(m/s) * EEBL_SAFE_DIST_PER_MS.
 *        Clamped to a minimum floor so a near-stopped car still has a gap
 *        (and to stay above the ultrasonic's reliable near limit).
 */
static float EEBL_SafeDistance(void)
{
  float safe = Host_Speed * EEBL_SAFE_DIST_PER_MS;

  if (safe < EEBL_MIN_SAFE_DISTANCE)
  {
    safe = EEBL_MIN_SAFE_DISTANCE;
  }

  return safe;
}

/**
 * @brief Evaluate rear-gap risk by comparing the actual rear distance
 *        against the speed-dependent safe distance.
 *          distance >= safe              -> RISK_SAFE
 *          crit*safe <= distance < safe  -> RISK_WARNING
 *          distance < crit*safe          -> RISK_CRITICAL
 */
static RiskLevel_t EEBL_EvaluateGap(float rear_distance)
{
  float safe_dist     = EEBL_SafeDistance();
  float critical_dist = safe_dist * EEBL_CRITICAL_RATIO;

  if (rear_distance < critical_dist)
  {
    return RISK_CRITICAL;
  }

  if (rear_distance < safe_dist)
  {
    return RISK_WARNING;
  }

  return RISK_SAFE;
}

/**
 * @brief Activate rear alerts (LED, Buzzer) based on risk level
 */
static void EEBL_ActivateAlert(RiskLevel_t level)
{
#if EEBL_ENABLE_LED_ALERT
  /* LED_REAR_ON(); — activate rear brake/hazard LEDs */
#endif

#if EEBL_ENABLE_BUZZER
  if (level == RISK_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif
}

/**
 * @brief Deactivate all EEBL alerts
 */
static void EEBL_DeactivateAlert(void)
{
#if EEBL_ENABLE_LED_ALERT
  /* LED_REAR_OFF(); */
#endif

#if EEBL_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}