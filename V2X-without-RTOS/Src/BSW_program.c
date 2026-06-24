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

/* Sender flag broadcast over DSRC: 0=None, 1=LEFT, 2=RIGHT (this car's own side) */
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
 *  - SENDER role: decide our own bsw_flag from the FRONT-side sensors
 *    (a car alongside-ahead of us → we broadcast that side).
 *  - Reset the receiver-side alert accumulators.
 */
void BSW_voidBeginCycle(float front_left, float front_right,
                        float rear_left, float rear_right)
{
  BSW_FrontLeft  = front_left;
  BSW_FrontRight = front_right;
  BSW_RearLeft   = rear_left;
  BSW_RearRight  = rear_right;

  /* ---- Sender: detect a vehicle alongside-ahead using FRONT-side US ---- */
  /* Left takes priority if both are present (single-byte flag). */
  if (front_left > 0.0f && front_left < BSW_SIDE_THRESHOLD)
  {
    BSW_SenderFlag = BSW_FLAG_LEFT;
  }
  else if (front_right > 0.0f && front_right < BSW_SIDE_THRESHOLD)
  {
    BSW_SenderFlag = BSW_FLAG_RIGHT;
  }
  else
  {
    BSW_SenderFlag = BSW_FLAG_NONE;
  }

  /* ---- Receiver: reset per-side alert accumulators ---- */
  BSW_AlertLeft  = 0;
  BSW_AlertRight = 0;
}

/**
 * @brief Process one DSRC neighbor for BSW (receiver role)
 *
 * A neighbor that saw a car on its FRONT side broadcasts bsw_flag. The car it
 * saw sits BEHIND it on the MIRRORED side, so we check the opposite rear:
 *   sender LEFT  (front-left)  -> check our REAR-RIGHT
 *   sender RIGHT (front-right) -> check our REAR-LEFT
 * If something is there, we are the car in that neighbor's blind spot.
 */
void BSW_voidProcessNeighbor(const Neighbor *n, Direction_t dir)
{
  (void)dir; /* blind-spot pairing is resolved by the mirrored side check */

  if (n->bsw_flag == BSW_FLAG_LEFT)
  {
    /* Sender saw us on its left → we are on its rear → check our rear-right */
    if (BSW_RearRight > 0.0f && BSW_RearRight < BSW_SIDE_THRESHOLD)
    {
      BSW_AlertRight = 1;
    }
  }
  else if (n->bsw_flag == BSW_FLAG_RIGHT)
  {
    /* Sender saw us on its right → check our rear-left */
    if (BSW_RearLeft > 0.0f && BSW_RearLeft < BSW_SIDE_THRESHOLD)
    {
      BSW_AlertLeft = 1;
    }
  }
}

/**
 * @brief End cycle — activate/deactivate alerts (receiver role)
 *
 * The sender flag (BSW_SenderFlag) is already set in BeginCycle and is read
 * by BSW_u8GetFlag() for the DSRC broadcast. The alert shown on THIS car is
 * the receiver decision confirmed against a neighbor's broadcast.
 */
void BSW_voidEndCycle(void)
{
  if (BSW_AlertLeft)
  {
    BSW_ActivateAlert(BSW_LEFT);
  }
  else
  {
    BSW_DeactivateAlert(BSW_LEFT);
  }

  if (BSW_AlertRight)
  {
    BSW_ActivateAlert(BSW_RIGHT);
  }
  else
  {
    BSW_DeactivateAlert(BSW_RIGHT);
  }
}

/* ============================================================ */
/* ============ Standalone Update (wrapper) =================== */
/* ============================================================ */

/**
 * @brief Standalone BSW update — iterates neighbor table internally
 *        Equivalent to calling BeginCycle + ProcessNeighbor(all) + EndCycle
 */
void BSW_voidUpdate(void)
{
  Neighbor *table = DSRC_GetTable();
  uint8_t count = DSRC_GetCount();

  BSW_voidBeginCycle(US_Distances[US_FRONT_LEFT], US_Distances[US_FRONT_RIGHT],
                     US_Distances[US_REAR_LEFT],  US_Distances[US_REAR_RIGHT]);

  for (uint8_t i = 0; i < count; i++)
  {
    Direction_t dir = SafetyEngine_DetectDirection(Host_Heading, table[i].heading);
    BSW_voidProcessNeighbor(&table[i], dir);
  }

  BSW_voidEndCycle();
}

/* ============ Public Getter ============ */
uint8_t BSW_u8GetFlag(void)
{
  return BSW_SenderFlag;
}

/* ============================================================ */
/* =================== Internal Functions ===================== */
/* ============================================================ */

/**
 * @brief Activate blind-spot alert on specified side
 */
static void BSW_ActivateAlert(uint8_t side)
{
#if BSW_ENABLE_LED_ALERT
  if (side == BSW_LEFT)
  {
    /* LED_LEFT_ON(); */
    /* LCD_Print("! Vehicle in LEFT blind spot"); */
  }
  else if (side == BSW_RIGHT)
  {
    /* LED_RIGHT_ON(); */
    /* LCD_Print("! Vehicle in RIGHT blind spot"); */
  }
  else
  {
  }
#endif

#if BSW_ENABLE_BUZZER
  /* BUZZER_SHORT_BEEP(); */
#endif
}

/**
 * @brief Deactivate blind-spot alert on specified side
 */
static void BSW_DeactivateAlert(uint8_t side)
{
#if BSW_ENABLE_LED_ALERT
  if (side == BSW_LEFT)
  {
    /* LED_LEFT_OFF(); */
  }
  else
  {
    /* LED_RIGHT_OFF(); */
  }
#endif

#if BSW_ENABLE_BUZZER
  /* BUZZER_OFF(); */
#endif
}
