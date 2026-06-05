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

#include "../Inc/Application/EEBL/EEBL_interface.h"
#include "../Inc/Application/EEBL/EEBL_config.h"
#include "../Inc/Application/EEBL/EEBL_private.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"
#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"

/* ============ Module State ============ */
static float       EEBL_PrevSpeed       = 0.0f;
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
  float decel = G_stHostVehicleState.Speed - EEBL_PrevSpeed;
  EEBL_BrakingDetected = (decel <= EEBL_DECEL_THRESHOLD) ? 1U : 0U;

  /* Save current speed for next cycle */
  EEBL_PrevSpeed = G_stHostVehicleState.Speed;

  EEBL_WorstLevel = RISK_SAFE;
}

/**
 * @brief Process one DSRC neighbor for EEBL
 *
 * Uses pre-computed direction from SafetyEngine.
 * Skips immediately if:
 *   - Not same direction
 *   - Not braking (gate from BeginCycle)
 *   - Rear distance out of range
 *   - Neighbor is not closing in
 */
void EEBL_voidProcessNeighbor(const Neighbor *n, float rear_distance, Direction_t dir)
{
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

  /*
   * Relative speed: other vehicle is faster → closing in on us
   * (We are braking, they haven't yet)
   */
  float rel_speed = n->speed - G_stHostVehicleState.Speed;

  if (rel_speed <= 0.0f)
  {
    /* Other vehicle is slower or same speed → no rear collision risk */
    return;
  }

  float ttc = SafetyEngine_CalcTTC(rear_distance, rel_speed);
  RiskLevel_t level = SafetyEngine_EvaluateRisk(ttc, EEBL_WARNING_TTC, EEBL_CRITICAL_TTC);

  if (level > EEBL_WorstLevel)
    EEBL_WorstLevel = level;
}

/**
 * @brief End cycle — activate/deactivate alerts based on results
 */
void EEBL_voidEndCycle(void)
{
  if (EEBL_WorstLevel > RISK_SAFE)
    EEBL_ActivateAlert(EEBL_WorstLevel);
  else
    EEBL_DeactivateAlert();
}

uint8_t EEBL_u8GetAlertLevel(void)
{
  return (uint8_t)EEBL_WorstLevel;
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
  float rear_dist = G_stHostVehicleState.BackCenterUS;

  EEBL_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(G_stHostVehicleState.Heading, table[i].heading);
    EEBL_voidProcessNeighbor(&table[i], rear_dist, dir);
  }

  EEBL_voidEndCycle();
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

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
