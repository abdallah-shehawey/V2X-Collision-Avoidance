#include <math.h>
#include <stdint.h>

#include "../Inc/ErrTypes.h"
#include "../Inc/STD_MACROS.h"
#include "../Inc/STM32F446xx.h"
#include "../Inc/DSRC.h"
#include "../Inc/SYSTIC_interface.h"
#include "../Inc/System.h"
#include "../Inc/FCW/FCW_interface.h"

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
  FCW_voidInit();

  while (1)
  {
    // process received packets
    DSRC_Update();

    // run FCW (local detection + cooperative alert)
    FCW_voidUpdate();

    // send own data with local FCW flag attached
    Neighbor self = simulate_self(VEHICLE_ID);
    self.fcw_flag = FCW_u8GetFlag();
    DSRC_SendNeighbor(&self);

    SYSTIC_vDelayMs(500);
  }
}
