/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    BSW_program.c   >>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : APP                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SW     : BSW (Cooperative Blind Spot Warning)            **
 **                                                                           **
 **===========================================================================**
 */

#include "../Inc/BSW/BSW_config.h"
#include "../Inc/BSW/BSW_interface.h"
#include "../Inc/BSW/BSW_private.h"
#include "../Inc/DSRC.h"
#include "../Inc/SafetyEngine/SafetyEngine_interface.h"
#include "../Inc/System.h"

/* ============ Module State ============ */

/* Sender flag broadcast over DSRC (this car's own side): bit0=LEFT, bit1=RIGHT */
static uint8_t BSW_SenderFlag = BSW_FLAG_NONE;

/* Cycle distances (latched in BeginCycle) */
static float BSW_FrontLeft  = 0.0f;
static float BSW_FrontRight = 0.0f;
static float BSW_RearLeft   = 0.0f;
static float BSW_RearRight  = 0.0f;

/* Receiver-side alert decision (set during ProcessNeighbor) */
static uint8_t BSW_AlertLeft  = 0;
static uint8_t BSW_AlertRight = 0;

/* ============ Init ============ */
void BSW_voidInit(void)
{
  BSW_SenderFlag = BSW_FLAG_NONE;
  BSW_AlertLeft  = 0;
  BSW_AlertRight = 0;
}

/* ============================================================ */
/* ============ Per-Neighbor API (for SafetyEngine) ============ */
/* ============================================================ */

/**
 * @brief Begin a new BSW processing cycle
 *
 *  - Latch the four side distances.
 *  - Sender role: build our bsw_flag from the front-side sensors. Each side is
 *    an independent bit, so a vehicle on both sides sets BSW_FLAG_BOTH.
 *  - Reset the receiver-side alert accumulators.
 */
void BSW_voidBeginCycle(float front_left, float front_right, float rear_left, float rear_right)
{
  BSW_FrontLeft  = front_left;
  BSW_FrontRight = front_right;
  BSW_RearLeft   = rear_left;
  BSW_RearRight  = rear_right;

  /* ---- Sender: front-side detection, one bit per side (no priority) ---- */
  BSW_SenderFlag = BSW_FLAG_NONE;

  if (front_left > 0.0f && front_left < BSW_SIDE_THRESHOLD)
  {
    BSW_SenderFlag |= BSW_FLAG_LEFT;
  }
  if (front_right > 0.0f && front_right < BSW_SIDE_THRESHOLD)
  {
    BSW_SenderFlag |= BSW_FLAG_RIGHT;
  }

  /* ---- Receiver: reset per-side alert accumulators ---- */
  BSW_AlertLeft  = 0;
  BSW_AlertRight = 0;
}

/**
 * @brief Process one DSRC neighbor for BSW (receiver role)
 *
 * A neighbor broadcasts the front side(s) on which it sees a car. That car sits
 * behind the neighbor on the mirrored side, so each side bit maps to our
 * opposite rear sensor:
 *   sender LEFT  (its front-left)  -> check our REAR-RIGHT
 *   sender RIGHT (its front-right) -> check our REAR-LEFT
 * The bits are tested independently, so BSW_FLAG_BOTH runs both checks.
 */
void BSW_voidProcessNeighbor(const Neighbor *n)
{
  if (n->bsw_flag & BSW_FLAG_LEFT)
  {
    if (BSW_RearRight > 0.0f && BSW_RearRight < BSW_SIDE_THRESHOLD)
    {
      BSW_AlertRight = 1;
    }
  }
  if (n->bsw_flag & BSW_FLAG_RIGHT)
  {
    if (BSW_RearLeft > 0.0f && BSW_RearLeft < BSW_SIDE_THRESHOLD)
    {
      BSW_AlertLeft = 1;
    }
  }
}

/* ============ Public Getters ============ */

/**
 * @brief Get the sender flag to broadcast over DSRC (my own front side(s)).
 * @return bit0 = LEFT, bit1 = RIGHT (0=none, 1=LEFT, 2=RIGHT, 3=both)
 */
uint8_t BSW_u8GetFlag(void)
{
  return BSW_SenderFlag;
}

/**
 * @brief Get the receiver-side blind-spot result for THIS car. The LED/buzzer
 *        is driven elsewhere by the caller — this module only computes which
 *        side(s) have a vehicle in the blind spot.
 * @return bit0 = LEFT, bit1 = RIGHT
 *         (0=none, 1=LEFT, 2=RIGHT, 3=both)
 */
uint8_t BSW_u8GetBlindSpot(void)
{
  return (uint8_t)((BSW_AlertLeft ? 0x01U : 0x00U) | (BSW_AlertRight ? 0x02U : 0x00U));
}
