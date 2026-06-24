/*
 **==========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    main.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                          **
 **                  Author : Abdallah Abdelmoemen Shehawey                  **
 **                  Layer  : APP                                            **
 **                  CPU    : Cortex-M4                                      **
 **                  MCU    : NUCLEO-F446RE                                  **
 **                  SW     : V2X Collision Avoidance System                 **
 **                                                                          **
 **==========================================================================**
 */

#include <math.h>
#include <stdint.h>

#include "../Inc/ErrTypes.h"
#include "../Inc/STD_MACROS.h"
#include "../Inc/STM32F446xx.h"
#include "../Inc/DSRC.h"
#include "../Inc/SYSTIC_interface.h"
#include "../Inc/System.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/FCW/FCW_interface.h"  /* for FCW_u8GetFlag()  */
#include "../Inc/DNPW/DNPW_interface.h" /* for DNPW_u8GetFlag() */
#include "../Inc/BSW/BSW_interface.h"   /* for BSW_u8GetFlag()  */
#include "../Inc/IMA/IMA_interface.h"   /* for IMA_u8GetFlag()  */

// ====== Simulate Own Data ======
static float sim_angle = 0.0f;

Neighbor simulate_self(uint8_t id)
{
  Neighbor n;
  n.vehicle_id = id;
  n.speed = 40.0f + 10.0f * sinf(sim_angle * 0.5f);
  n.heading = fmodf(sim_angle * 57.295f, 360.0f);
  n.last_update = 0;
  sim_angle += 0.05f;
  return n;
}

// ============================================================
// Main
// ============================================================
int main(void)
{
  System_Init();
  SafetyEngine_voidInit();

  while (1)
  {
    // process received DSRC packets
    DSRC_Update();

    // single-pass safety processing (FCW + EEBL in one loop)
    SafetyEngine_voidUpdate();

    // send own data with local FCW flag attached
    Neighbor self = simulate_self(VEHICLE_ID);
    self.fcw_flag  = FCW_u8GetFlag();
    self.dnpw_flag = DNPW_u8GetFlag();
    self.bsw_flag  = BSW_u8GetFlag();
    self.ima_flag  = IMA_u8GetFlag();
    self.distance_to_intersection = Host_DistToIntersection;
    DSRC_SendNeighbor(&self);

    SYSTIC_vDelayMs(500);
  }
}

