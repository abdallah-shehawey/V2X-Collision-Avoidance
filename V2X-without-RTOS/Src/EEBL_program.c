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

/* ============ Module State ============ */
static float EEBL_HostSpeed   = 0.0f;
static float EEBL_PrevSpeed   = 0.0f;
static float EEBL_HostHeading = 0.0f;

/* Ultrasonic sensor distances (cm) — defined in sysmem.c */
extern float US_Distances[US_SENSOR_COUNT];

/* ============ Init ============ */
void EEBL_voidInit(void)
{
  EEBL_HostSpeed   = 0.0f;
  EEBL_PrevSpeed   = 0.0f;
  EEBL_HostHeading = 0.0f;
}

/* ============ Main Update ============ */
/*
 * Call this in the main loop.
 * EEBL is a LOCAL-ONLY alert system (no flag broadcast via DSRC).
 *
 * Steps:
 *   1. Detect sudden braking (speed drop exceeds threshold)
 *   2. Check rear US sensor for vehicle behind
 *   3. Find same-direction DSRC neighbors behind us
 *   4. Calculate TTC = rear_distance / (other_speed - my_speed)
 *   5. Activate local alert (rear LEDs / hazard) if danger detected
 */
void EEBL_voidUpdate(void)
{
  /* 1. Read host vehicle data (from sensors/modules) */
  /* EEBL_HostSpeed   = ...get from speedometer... */
  /* EEBL_HostHeading = ...get from compass/IMU...  */

  /* ======== Step 1: Detect Sudden Braking ======== */
  float decel = EEBL_HostSpeed - EEBL_PrevSpeed;
  uint8_t braking = (decel <= EEBL_DECEL_THRESHOLD) ? 1U : 0U;

  /* Save current speed for next cycle */
  EEBL_PrevSpeed = EEBL_HostSpeed;

  /* If NOT braking suddenly → no EEBL danger, clear and return */
  if (!braking)
  {
    EEBL_DeactivateAlert();
    return;
  }

  /* ======== Step 2: Check Rear Sensor ======== */
  float rear_dist = US_Distances[US_REAR];

  /* No vehicle behind or out of range → no danger */
  if (rear_dist <= 0.0f || rear_dist > EEBL_MAX_DETECTION_RANGE)
  {
    EEBL_DeactivateAlert();
    return;
  }

  /* ======== Step 3 + 4: DSRC Neighbors → TTC ======== */
  Neighbor *table = DSRC_GetTable();
  uint8_t count   = DSRC_GetCount();

  EEBL_RiskLevel_t worst = EEBL_SAFE;

  for (uint8_t i = 0; i < count; i++)
  {
    /* Only care about same-direction vehicles (behind us) */
    if (!EEBL_IsSameDirection(EEBL_HostHeading, table[i].heading))
    {
      continue;
    }

    /*
     * Relative speed: other vehicle is faster → closing in on us
     * (We are braking, they haven't yet)
     */
    float rel_speed = table[i].speed - EEBL_HostSpeed;

    if (rel_speed <= 0.0f)
    {
      /* Other vehicle is slower or same speed → no rear collision risk */
      continue;
    }

    float ttc = EEBL_CalcTTC(rear_dist, rel_speed);
    EEBL_RiskLevel_t level = EEBL_EvaluateRisk(ttc);

    if (level > worst)
    {
      worst = level;
    }
  }

  /* ======== Step 5: Activate Local Alert ======== */
  if (worst > EEBL_SAFE)
  {
    EEBL_ActivateAlert(worst);
  }
  else
  {
    EEBL_DeactivateAlert();
  }
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Calculate absolute heading difference, normalized to [0, 180]
 */
static float EEBL_CalcHeadingDiff(float h1, float h2)
{
  float diff = h1 - h2;

  if (diff > 180.0f)
  {
    diff -= 360.0f;
  }
  if (diff < -180.0f)
  {
    diff += 360.0f;
  }

  return (diff < 0.0f) ? -diff : diff;
}

/**
 * @brief Check if two vehicles are going in the same direction
 * @return 1 if same direction, 0 otherwise
 */
static uint8_t EEBL_IsSameDirection(float my_heading, float other_heading)
{
  float diff = EEBL_CalcHeadingDiff(my_heading, other_heading);
  return (diff <= EEBL_SAME_HEADING_THRESHOLD) ? 1U : 0U;
}

/**
 * @brief Calculate Time To Collision
 * @param distance        Rear distance from ultrasonic sensor
 * @param relative_speed  Closing speed (other_speed - my_speed)
 * @return TTC in seconds, or -1.0 if no risk
 */
static float EEBL_CalcTTC(float distance, float relative_speed)
{
  if (relative_speed <= 0.0f)
  {
    return -1.0f;
  }

  return distance / relative_speed;
}

/**
 * @brief Evaluate risk level based on TTC value
 */
static EEBL_RiskLevel_t EEBL_EvaluateRisk(float ttc)
{
  if (ttc < 0.0f)
  {
    return EEBL_SAFE;
  }

  if (ttc <= EEBL_CRITICAL_TTC)
  {
    return EEBL_CRITICAL;
  }

  if (ttc <= EEBL_WARNING_TTC)
  {
    return EEBL_WARNING;
  }

  return EEBL_SAFE;
}

/**
 * @brief Activate rear alerts (LED, Buzzer) based on risk level
 */
static void EEBL_ActivateAlert(EEBL_RiskLevel_t level)
{
#if EEBL_ENABLE_LED_ALERT
  /* LED_REAR_ON(); — activate rear brake/hazard LEDs */
#endif

#if EEBL_ENABLE_BUZZER
  if (level == EEBL_CRITICAL)
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