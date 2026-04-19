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
#include "../Inc/DSRC.h"
#include "../Inc/System.h"

/* ============ Init ============ */
void SafetyEngine_voidInit(void)
{
  FCW_voidInit();
  EEBL_voidInit();
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
  float front_dist = US_Distances[US_FRONT];
  float rear_dist  = US_Distances[US_REAR];
  /* Read host vehicle data (from sensors/modules) */
  /* HostSpeed   = ...get from speedometer... */
  /* HostHeading = ...get from compass/IMU...  */


  /* 1. Begin cycle for all modules */
  FCW_voidBeginCycle();
  EEBL_voidBeginCycle();

  /* 2. Single pass over neighbor table */
  for (uint8_t i = 0; i < count; i++)
  {
    /* Compute direction ONCE per neighbor — shared by all modules */
    Direction_t dir = System_DetectDirection(Host_Heading, table[i].heading);

    FCW_voidProcessNeighbor(&table[i], front_dist, dir);
    EEBL_voidProcessNeighbor(&table[i], rear_dist, dir);
  }

  /* 3. End cycle for all modules */
  FCW_voidEndCycle();
  EEBL_voidEndCycle();
}
