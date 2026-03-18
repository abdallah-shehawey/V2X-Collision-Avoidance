#include <stdint.h>
#include <math.h>

#include "STM32F446xx.h"
#include "STD_MACROS.h"
#include "ErrTypes.h"
#include "SYSTIC_interface.h"
#include "DSRC.h"
#include "System.h"

// ====== Simulate Own Data ======
static float sim_angle = 0.0f;

Neighbor simulate_self(uint8_t id)
{
  Neighbor n;
  n.vehicle_id = id;
  n.pos_x = 50.0f + 20.0f * cosf(sim_angle);
  n.pos_y = 50.0f + 20.0f * sinf(sim_angle);
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

  while (1)
  {
    // process received packets
    DSRC_Update();

    // send own data
    Neighbor self = simulate_self(VEHICLE_ID);
    DSRC_SendNeighbor(&self);

    SYSTIC_vDelayMs(500);
  }
}
