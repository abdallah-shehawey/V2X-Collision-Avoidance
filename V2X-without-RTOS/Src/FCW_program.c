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

/* ============ Module State ============ */
static float   FCW_HostSpeed   = 0.0f;
static float   FCW_HostHeading = 0.0f;
static uint8_t FCW_CurrentFlag = 0; /* 0=Safe, 1=Warning, 2=Critical */

/* Ultrasonic sensor distances (cm) — assumed updated externally */
extern float US_Distances[US_SENSOR_COUNT];


/* ============ Init ============ */
void FCW_voidInit(void)
{
  FCW_HostSpeed   = 0.0f;
  FCW_HostHeading = 0.0f;
  FCW_CurrentFlag = 0;
}

/* ============ Main Update ============ */
/*
 * Call this in the main loop.
 * Two-stage cooperative FCW:
 *   Stage 1 → Local TTC detection, sets FCW_CurrentFlag for DSRC broadcast
 *   Stage 2 → Alert decision using cooperative confirmation (opposite dir)
 */
void FCW_voidUpdate(void)
{
  /* 1. Read host vehicle data (from sensors/modules) */
  /* FCW_HostSpeed   = ...get from speedometer... */
  /* FCW_HostHeading = ...get from compass/IMU...  */
  /* US_Distances[US_FRONT] = ...get from ultrasonic sensors... */

  Neighbor *table = DSRC_GetTable();
  uint8_t count   = DSRC_GetCount();

  /*
   * Single pass: compute both local_worst (for DSRC flag broadcast)
   * and alert_level (for alert activation) in the same loop.
   *
   * local_worst → always set from local TTC detection → broadcast via DSRC
   * alert_level → cooperative check for opposite dir, local for same dir
   */
  FCW_RiskLevel_t local_worst = FCW_SAFE;
  FCW_RiskLevel_t alert_level = FCW_SAFE;

  for (uint8_t i = 0; i < count; i++)
  {
    FCW_Direction_t dir = FCW_DetectDirection(FCW_HostHeading, table[i].heading);
    float rel_speed;
    float distance = US_Distances[US_FRONT];

    if (dir == FCW_DIR_OPPOSITE)
    {
      /* Both approaching → speeds add up */
      rel_speed = FCW_HostSpeed + table[i].speed;
    }
    else if (dir == FCW_DIR_SAME)
    {
      /* Following → closing speed = difference */
      rel_speed = FCW_HostSpeed - table[i].speed;
    }
    else
    {
      /* FCW_DIR_UNKNOWN → perpendicular or irrelevant, skip */
      continue;
    }

    if (rel_speed > 0.0f && distance > 0.0f)
    {
      float ttc = FCW_CalcTTC(distance, rel_speed);
      FCW_RiskLevel_t level = FCW_EvaluateRisk(ttc);

      /* Track worst local risk → this becomes the DSRC broadcast flag */
      if (level > local_worst)
      {
        local_worst = level;
      }

      /* Alert decision depends on direction */
      if (dir == FCW_DIR_OPPOSITE)
      {
        /*
         * Cooperative Confirmation (opposite direction):
         * Only trigger alert if BOTH vehicles detect danger.
         * My flag will be broadcast (local_worst), so the other
         * vehicle sees it next cycle. Here we check if the other
         * vehicle has also reported danger.
         */
        if (level > FCW_SAFE && table[i].fcw_flag > 0)
        {
          if (level > alert_level)
          {
            alert_level = level;
          }
        }
      }
      else /* FCW_DIR_SAME */
      {
        /*
         * Local detection only (same direction):
         * No cooperative confirmation needed.
         */
        if (level > alert_level)
        {
          alert_level = level;
        }
      }
    }
  }

  /*
   * CRITICAL: Always update the flag from local detection.
   * This flag is read by FCW_u8GetFlag() and sent in every DSRC message.
   * Without this, the cooperative confirmation would deadlock
   * (both vehicles waiting for each other's flag).
   */
  FCW_CurrentFlag = (uint8_t)local_worst;

  /* Activate or deactivate alert based on confirmed level */
  if (alert_level > FCW_SAFE)
  {
    FCW_ActivateAlert(alert_level);
  }
  else
  {
    FCW_DeactivateAlert();
  }
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
 * @brief Calculate absolute heading difference, normalized to [0, 180]
 */
static float FCW_CalcHeadingDiff(float h1, float h2)
{
  float diff = h1 - h2;

  /* Normalize to [-180, 180] */
  if (diff > 180.0f)
  {
    diff -= 360.0f;
  }
  if (diff < -180.0f)
  {
    diff += 360.0f;
  }

  /* Return absolute value */
  return (diff < 0.0f) ? -diff : diff;
}

/**
 * @brief Detect if two vehicles are going same direction, opposite, or unknown
 * @param my_heading     Host vehicle heading (0-360)
 * @param other_heading  Neighbor vehicle heading (0-360)
 * @return FCW_DIR_SAME, FCW_DIR_OPPOSITE, or FCW_DIR_UNKNOWN
 */
static FCW_Direction_t FCW_DetectDirection(float my_heading, float other_heading)
{
  float diff = FCW_CalcHeadingDiff(my_heading, other_heading);

  if (diff <= FCW_SAME_HEADING_THRESHOLD)
  {
    return FCW_DIR_SAME;
  }

  if (diff >= (180.0f - FCW_OPPOSITE_HEADING_THRESHOLD))
  {
    return FCW_DIR_OPPOSITE;
  }

  return FCW_DIR_UNKNOWN;
}

/**
 * @brief Calculate Time To Collision
 * @param distance        Distance to obstacle (from ultrasonic)
 * @param relative_speed  Relative closing speed
 * @return TTC in seconds, or -1.0 if no collision risk
 */
static float FCW_CalcTTC(float distance, float relative_speed)
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
static FCW_RiskLevel_t FCW_EvaluateRisk(float ttc)
{
  if (ttc < 0.0f)
  {
    return FCW_SAFE;
  }

  if (ttc <= FCW_CRITICAL_TTC)
  {
    return FCW_CRITICAL;
  }

  if (ttc <= FCW_WARNING_TTC)
  {
    return FCW_WARNING;
  }

  return FCW_SAFE;
}

/**
 * @brief Activate alerts (LED, Buzzer, ADAS) based on risk level
 */
static void FCW_ActivateAlert(FCW_RiskLevel_t level)
{
#if FCW_ENABLE_LED_ALERT
  /* LED_FRONT_ON(); */
#endif

#if FCW_ENABLE_BUZZER
  if (level == FCW_CRITICAL)
  {
    /* BUZZER_ON(); */
  }
#endif

#if FCW_ENABLE_ADAS_REQUEST
  if (level == FCW_CRITICAL)
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
