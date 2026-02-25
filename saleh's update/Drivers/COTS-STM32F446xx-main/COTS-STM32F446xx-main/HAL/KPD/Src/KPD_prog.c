/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    KPD_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : KPD                                             **
 **                                                                           **
 **===========================================================================**
 */

#include <stdint.h>

#include "STD_MACROS.h"
#include "ErrTypes.h"

#include "GPIO_interface.h"

#include "KPD_interface.h"
#include "KPD_config.h"
#include "KPD_private.h"

/*******************************************************************************
 *                           Hardware Connection Diagram                         *
 *******************************************************************************/
/*
 * 4x4 Matrix Keypad Connection:
 *
 * Rows (Input Pull-up):
 * R0 -> GPIO Pin KPD_R0
 * R1 -> GPIO Pin KPD_R1
 * R2 -> GPIO Pin KPD_R2
 * R3 -> GPIO Pin KPD_R3
 *
 * Columns (Output Push-pull):
 * C0 -> GPIO Pin KPD_C0
 * C1 -> GPIO Pin KPD_C1
 * C2 -> GPIO Pin KPD_C2
 * C3 -> GPIO Pin KPD_C3
 *
 * Operation:
 * 1. Columns are normally HIGH
 * 2. To scan, each column is set LOW one at a time
 * 3. When a key is pressed, the corresponding row goes LOW
 * 4. The key value is determined by the active row and column
 */

/*******************************************************************************
 *                           Function Implementations                            *
 *******************************************************************************/

/**
 * @fn    KPD_vInit
 * @brief Initialize keypad GPIO pins
 * @details This function:
 *          1. Configures row pins as input pull-up
 *          2. Configures column pins as output push-pull
 *          3. Sets initial column states to HIGH
 */
void KPD_vInit(void)
{
  GPIO_PinConfig_t PinConfig;

  /* Configure Row Pins as Input Pull-up */
  PinConfig.Port = KPD_PORT;
  PinConfig.Mode = GPIO_INPUT;
  PinConfig.PullType = GPIO_PULL_UP;
  PinConfig.Speed = GPIO_MEDIUM_SPEED;

  PinConfig.PinNum = KPD_R0;
  GPIO_enumPinInit(&PinConfig);

  PinConfig.PinNum = KPD_R1;
  GPIO_enumPinInit(&PinConfig);

  PinConfig.PinNum = KPD_R2;
  GPIO_enumPinInit(&PinConfig);

  PinConfig.PinNum = KPD_R3;
  GPIO_enumPinInit(&PinConfig);

  /* Configure Column Pins as Output Push-pull */
  PinConfig.Mode = GPIO_OUTPUT;
  PinConfig.Otype = GPIO_PUSH_PULL;
  PinConfig.PullType = GPIO_NO_PULL;

  PinConfig.PinNum = KPD_C0;
  GPIO_enumPinInit(&PinConfig);

  PinConfig.PinNum = KPD_C1;
  GPIO_enumPinInit(&PinConfig);

  PinConfig.PinNum = KPD_C2;
  GPIO_enumPinInit(&PinConfig);

  PinConfig.PinNum = KPD_C3;
  GPIO_enumPinInit(&PinConfig);

  /* Set Column Pins High initially */
  GPIO_enumWritePinVal(KPD_PORT, KPD_C0, GPIO_PIN_HIGH);
  GPIO_enumWritePinVal(KPD_PORT, KPD_C1, GPIO_PIN_HIGH);
  GPIO_enumWritePinVal(KPD_PORT, KPD_C2, GPIO_PIN_HIGH);
  GPIO_enumWritePinVal(KPD_PORT, KPD_C3, GPIO_PIN_HIGH);
}

/**
 * @fn    KPD_u8GetPressed
 * @brief Get the currently pressed key value
 * @details This function:
 *          1. Scans each column by setting it LOW
 *          2. Checks each row for a LOW state
 *          3. Includes debouncing delay
 *          4. Waits for key release
 *          5. Returns key value from lookup table
 * @return uint8_t:
 *         - Key value from the keypad matrix if pressed
 *         - NOTPRESSED (0xFF) if no key is pressed
 * @note  The function implements the following scanning algorithm:
 *        1. Set current column LOW
 *        2. Check all rows
 *        3. If row is LOW:
 *           - Wait for debounce
 *           - Recheck to confirm
 *           - Get key value from lookup table
 *           - Wait for key release
 *        4. Set column back HIGH
 *        5. Move to next column
 */
uint8_t KPD_u8GetPressed(void)
{
  uint8_t LOC_u8ReturnData = NOTPRESSED;
  GPIO_PinValue_t LOC_u8GetPressed;

  /* iterators */
  uint8_t LOC_u8Row, LOC_u8Col;

  for (LOC_u8Col = KPD_COL_INIT; LOC_u8Col <= KPD_COL_END; LOC_u8Col++)
  {
    /* Set current column to LOW */
    GPIO_enumWritePinVal(KPD_PORT, LOC_u8Col, GPIO_PIN_LOW);

    for (LOC_u8Row = KPD_ROW_INIT; LOC_u8Row <= KPD_ROW_END; LOC_u8Row++)
    {
      /* Read current row value */
      GPIO_enumReadPinVal(KPD_PORT, LOC_u8Row, &LOC_u8GetPressed);

      /* If row is LOW, button is pressed */
      if (LOC_u8GetPressed == GPIO_PIN_LOW)
      {
        /* Delay for debouncing */
        for (uint32_t i = 0; i < 5000; i++)
          ;

        /* Read again to confirm */
        GPIO_enumReadPinVal(KPD_PORT, LOC_u8Row, &LOC_u8GetPressed);

        if (LOC_u8GetPressed == GPIO_PIN_LOW)
        {
          /* Get the button value from the lookup table */
          LOC_u8ReturnData = KPD_u8Buttons[LOC_u8Row - KPD_ROW_INIT][LOC_u8Col - KPD_COL_INIT];

          /* Wait for button release */
          do
          {
            GPIO_enumReadPinVal(KPD_PORT, LOC_u8Row, &LOC_u8GetPressed);
          } while (LOC_u8GetPressed == GPIO_PIN_LOW);

          break;
        }
      }
    }

    /* Set current column back to HIGH */
    GPIO_enumWritePinVal(KPD_PORT, LOC_u8Col, GPIO_PIN_HIGH);
  }

  return LOC_u8ReturnData;
}
