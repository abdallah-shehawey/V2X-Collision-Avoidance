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
#include "../Inc/FCW_DNPW/FCW_DNPW_interface.h" /* FCW/DNPW flag getters */
#include "../Inc/EEBL/EEBL_interface.h"          /* EEBL_u8GetFlag()      */
#include "../Inc/BSW/BSW_interface.h"            /* BSW flag + blind spot */
#include "../Inc/IMA/IMA_interface.h"            /* IMA_u8GetFlag()       */

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
    // process received DSRC packets, then run all safety modules in one pass
    DSRC_Update();
    SafetyEngine_voidUpdate();

    /* ================================================================
     * Reading any system's status: just call its getter after Update().
     * Every getter returns 0=Safe, 1=Warning, 2=Critical unless noted.
     * ================================================================ */

    /* --- Local alerts: for THIS car's driver (LED/buzzer/dashboard) --- */
    uint8_t fcw_front   = FCW_GetFrontFlag();       // vehicle ahead, same lane
    uint8_t fcw_headon  = FCW_GetHeadonConfirmed(); // confirmed head-on collision
    uint8_t dnpw        = DNPW_GetFlag();           // do-not-pass: 0/1 (presence only)
    uint8_t eebl        = EEBL_u8GetFlag();          // rear emergency brake
    uint8_t blind_spot  = BSW_u8GetBlindSpot();      // bit0=LEFT, bit1=RIGHT
    uint8_t ima         = IMA_u8GetFlag();           // intersection movement assist

    /* TODO: drive LEDs/buzzer from the values above */
    (void)fcw_front;
    (void)fcw_headon;
    (void)dnpw;
    (void)eebl;
    (void)blind_spot;
    (void)ima;

    /* --- Cooperative flags: broadcast so other cars can use them --- */
    Neighbor self = simulate_self(VEHICLE_ID);
    self.fcw_headon_flag = FCW_GetHeadonFlag(); // head-on candidate (0/1)
    self.bsw_flag        = BSW_u8GetFlag();      // my front side(s): bit0=LEFT, bit1=RIGHT
    self.ima_flag        = IMA_u8GetFlag();      // intersection movement assist
    self.distance_to_intersection = Host_DistToIntersection; // for others' IMA
    DSRC_SendNeighbor(&self);

    SYSTIC_vDelayMs(500);
  }
}

