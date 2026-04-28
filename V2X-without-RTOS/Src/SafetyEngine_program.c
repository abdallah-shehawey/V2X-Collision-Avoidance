/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<    SafetyEngine_program.c   >>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : V2X Safety Engine (Single-Pass)                 **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/FCW/FCW_interface.h"
#include "../Inc/EEBL/EEBL_interface.h"
#include "../Inc/BSW/BSW_interface.h"
#include "../Inc/DNPW/DNPW_interface.h"
#include "../Inc/DSRC.h"
#include "../Inc/System.h"

float US_Distances[US_SENSOR_COUNT];
float Host_Speed   = 0.0f;
float Host_Heading = 0.0f;

/* ============ Init ============ */
void SafetyEngine_voidInit(void)
{
  FCW_voidInit();
  EEBL_voidInit();
  BSW_voidInit();
  DNPW_voidInit();
}

/* ============ Single-Pass Update ============ */
/*
 * Flow:
 *   1. BeginCycle → each module resets its accumulators
 *   2. For each neighbor → call ProcessNeighbor on all modules
 *   3. EndCycle → each module finalizes (set flags, activate alerts)
 */
void SafetyEngine_voidUpdate(void)
{
  /* Host_Speed, Host_Heading, US_Distances → shared globals (System.h) */

  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();

  /* Read all US distances ONCE per cycle */
  float front_dist = US_Distances[US_FRONT];
  float rear_dist  = US_Distances[US_REAR];
  float left_dist  = US_Distances[US_FRONT_LEFT];
  float right_dist = US_Distances[US_FRONT_RIGHT];

  /* Use min of front+rear sensor per side for BSW */
  float left_rear  = US_Distances[US_REAR_LEFT];
  float right_rear = US_Distances[US_REAR_RIGHT];
  if (left_rear < left_dist)   { left_dist  = left_rear;  }
  if (right_rear < right_dist) { right_dist = right_rear; }

  /* Read host vehicle data (from sensors/modules) */
  /* HostSpeed   = ...get from speedometer... */
  /* HostHeading = ...get from compass/IMU...  */


  /* 1. Begin cycle for all modules */
  FCW_voidBeginCycle();
  EEBL_voidBeginCycle();
  BSW_voidBeginCycle(left_dist, right_dist);
  DNPW_voidBeginCycle(front_dist);

  /* 2. Single pass over neighbor table */
  for (uint8_t i = 0; i < count; i++)
  {
    /* Compute direction ONCE per neighbor — shared by all modules */
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);

    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
    EEBL_voidProcessNeighbor(&table[i], rear_dist, dir);
    BSW_voidProcessNeighbor(&table[i], dir);
    DNPW_voidProcessNeighbor(&table[i], dir);
  }

  /* 3. End cycle for all modules */
  FCW_voidEndCycle();
  EEBL_voidEndCycle();
  BSW_voidEndCycle();
  DNPW_voidEndCycle();
}

/* ============ Shared Direction Detection ============ */

/**
 * @brief Calculate absolute heading difference, normalized to [0, 180]
 */
static float CalcHeadingDiff(float h1, float h2)
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

Direction_t SafetyEngine_DetectDirection(float my_heading, float other_heading)
{
  float diff = CalcHeadingDiff(my_heading, other_heading);

  if (diff <= HEADING_SAME_THRESHOLD)
  {
    return DIR_SAME;
  }

  if (diff >= (180.0f - HEADING_OPPOSITE_THRESHOLD))
  {
    return DIR_OPPOSITE;
  }

  return DIR_UNKNOWN;
}

/* ============ Shared TTC & Risk Evaluation ============ */

float SafetyEngine_CalcTTC(float distance, float relative_speed)
{
  if (relative_speed <= 0.0f)
  {
    return -1.0f;
  }

  return distance / relative_speed;
}

RiskLevel_t SafetyEngine_EvaluateRisk(float ttc, float warning_ttc, float critical_ttc)
{
  if (ttc < 0.0f)
  {
    return RISK_SAFE;
  }

  if (ttc <= critical_ttc)
  {
    return RISK_CRITICAL;
  }

  if (ttc <= warning_ttc)
  {
    return RISK_WARNING;
  }

  return RISK_SAFE;
}
