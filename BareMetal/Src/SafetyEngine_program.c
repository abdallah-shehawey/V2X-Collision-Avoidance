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

#include "../Inc/Application/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/Application/FCW/FCW_interface.h"
#include "../Inc/Application/EEBL/EEBL_interface.h"
#include "../Inc/Application/BSW/BSW_interface.h"
#include "../Inc/Application/DNPW/DNPW_interface.h"
#include "../Inc/Application/IMA/IMA_interface.h"
#include "../Inc/Application/DSRC/DSRC.h"
#include "../Inc/System/System.h"

/* Shared globals read by ADAS modules during ProcessNeighbor */
float Host_Speed   = 0.0f;
float Host_Heading = 0.0f;

/* ============ Init ============ */
void SafetyEngine_voidInit(void)
{
  FCW_voidInit();
  EEBL_voidInit();
  BSW_voidInit();
  DNPW_voidInit();
  IMA_voidInit();
}

/* ============ Single-Pass Update ============ */
void SafetyEngine_voidUpdate(void)
{
  Neighbor *table  = DSRC_GetTable();
  uint8_t count    = DSRC_GetCount();

  Host_Speed   = G_stHostVehicleState.Speed;
  Host_Heading = G_stHostVehicleState.Heading;

  float front_dist = G_stHostVehicleState.FrontCenterUS;
  float rear_dist  = G_stHostVehicleState.BackCenterUS;
  float left_dist  = G_stHostVehicleState.FrontLeftUS;
  float right_dist = G_stHostVehicleState.FrontRightUS;

  float left_rear  = G_stHostVehicleState.BackLeftUS;
  float right_rear = G_stHostVehicleState.BackRightUS;
  if (left_rear  < left_dist)  left_dist  = left_rear;
  if (right_rear < right_dist) right_dist = right_rear;

  /* Single pass: all modules share the same neighbor loop */
  FCW_voidBeginCycle();
  EEBL_voidBeginCycle();
  BSW_voidBeginCycle(left_dist, right_dist);
  DNPW_voidBeginCycle(front_dist, G_stHostVehicleState.FrontLeftUS);
  IMA_voidBeginCycle();

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
    EEBL_voidProcessNeighbor(&table[i], rear_dist, dir);
    BSW_voidProcessNeighbor(&table[i], dir);
    DNPW_voidProcessNeighbor(&table[i], dir);
    IMA_voidProcessNeighbor(&table[i], dir);
  }

  FCW_voidEndCycle();
  EEBL_voidEndCycle();
  BSW_voidEndCycle();
  DNPW_voidEndCycle();
  IMA_voidEndCycle();

  /* Status word: 2 bits per module (00 safe / 01 warning / 10 critical).
   * RiskLevel maps directly: SAFE=0, WARNING=1, CRITICAL=2. BSW is binary
   * (blind-spot present → WARNING). */
  uint8_t bsw = (BSW_u8GetLeftFlag() || BSW_u8GetRightFlag()) ? SYS_WARNING : SYS_SAFE;

  uint16_t flags = 0;
  flags |= ((uint16_t)(FCW_u8GetAlertLevel()  & SYS_MASK)) << SYS_FCW_POS;
  flags |= ((uint16_t)(EEBL_u8GetAlertLevel() & SYS_MASK)) << SYS_EEBL_POS;
  flags |= ((uint16_t)(bsw                    & SYS_MASK)) << SYS_BSW_POS;
  flags |= ((uint16_t)(DNPW_u8GetFlag()       & SYS_MASK)) << SYS_DNPW_POS;
  flags |= ((uint16_t)(IMA_u8GetFlag()        & SYS_MASK)) << SYS_IMA_POS;

  /* Publish the full 16-bit word (must be uint16: IMA lives at bit 8). */
  G_u16SystemFlags = flags;
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

  if (diff >= (90.0f - HEADING_CROSS_THRESHOLD) && diff <= (90.0f + HEADING_CROSS_THRESHOLD))
  {
    return DIR_CROSSING;
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
