/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SSD_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : SSD                                             **
 **                                                                           **
 **===========================================================================**
 */

#include <stdint.h>

#include "ErrTypes.h"
#include "STD_MACROS.h"
#include "GPIO_interface.h"
#include "SSD_interface.h"
#include "SSD_private.h"
#include "SSD_config.h"

/*******************************************************************************
 *                           Hardware Connection Diagram                         *
 *******************************************************************************/
/*
 * Seven-Segment Display Pin Connections:
 *
 * Data Pins (8 bits starting from StartPin):
 * Pin 0 -> Segment a
 * Pin 1 -> Segment b
 * Pin 2 -> Segment c
 * Pin 3 -> Segment d
 * Pin 4 -> Segment e
 * Pin 5 -> Segment f
 * Pin 6 -> Segment g
 * Pin 7 -> Decimal point (optional)
 *
 * Enable Pin:
 * Common Cathode: Active LOW
 * Common Anode: Active HIGH
 *
 * Segment Pattern:
 *      a
 *    f   b
 *      g
 *    e   c
 *      d    dp
 */

/**
 * @brief Array of segment patterns for numbers 0-9
 * @details Each byte represents the segments to be lit for each number:
 *          - Bit 0: Segment a
 *          - Bit 1: Segment b
 *          - Bit 2: Segment c
 *          - Bit 3: Segment d
 *          - Bit 4: Segment e
 *          - Bit 5: Segment f
 *          - Bit 6: Segment g
 *          - Bit 7: Decimal point (not used)
 */
static const uint8_t LOCAL_u8SSDNumbers[10] = SSD_NUMBER_PATTERNS;

/*******************************************************************************
 *                           Function Implementations                            *
 *******************************************************************************/

/**
 * @fn    SSD_enumInit
 * @brief Initialize a seven-segment display
 * @details This function:
 *          1. Validates input configuration
 *          2. Configures data port pins as outputs
 *          3. Configures enable pin as output
 *          4. Sets initial pin states
 * @param Configuration: Pointer to SSD configuration structure
 * @return ErrorState_t:
 *         - OK: Initialization successful
 *         - NOK: Invalid configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumInit(const SSD_Config_t *Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (Configuration != NULL)
  {
    /* Configure data port pins */
    GPIO_8BinsConfig_t Local_DataConfig = {
        .Port = Configuration->DataPort,
        .StartPin = Configuration->StartPin,
        .Mode = SSD_DATA_MODE,
        .Otype = SSD_DATA_TYPE,
        .Speed = SSD_DATA_SPEED,
        .PullType = SSD_DATA_PULL};

    Local_ErrorState = GPIO_enumPort8BitsInit(&Local_DataConfig);

    if (Local_ErrorState == OK)
    {
      /* Configure enable pin */
      GPIO_PinConfig_t Local_EnableConfig = {
          .Port = Configuration->EnablePort,
          .PinNum = Configuration->EnablePin,
          .Mode = SSD_ENABLE_MODE,
          .Otype = SSD_ENABLE_TYPE,
          .Speed = SSD_ENABLE_SPEED,
          .PullType = SSD_ENABLE_PULL};

      Local_ErrorState = GPIO_enumPinInit(&Local_EnableConfig);
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }

  return Local_ErrorState;
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    SSD_enumDisplayNumber
 * @brief Display a number on the seven-segment display
 * @details This function:
 *          1. Validates input number (0-9)
 *          2. Gets segment pattern from lookup table
 *          3. Inverts pattern for common anode displays
 *          4. Writes pattern to data pins
 * @param Configuration: Pointer to SSD configuration structure
 * @param Copy_u8Number: Number to display (0-9)
 * @return ErrorState_t:
 *         - OK: Number displayed successfully
 *         - NOK: Invalid number or configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumDisplayNumber(const SSD_Config_t *Configuration, uint8_t Copy_u8Number)
{
  ErrorState_t Local_ErrorState = OK;

  if (Configuration != NULL && Copy_u8Number <= 9)
  {
    if (Configuration->Type == SSD_COMMON_CATHODE)
    {
      Local_ErrorState = GPIO_enumWrite8BitsVal(Configuration->DataPort,
                                                Configuration->StartPin,
                                                LOCAL_u8SSDNumbers[Copy_u8Number]);
    }
    else if (Configuration->Type == SSD_COMMON_ANODE)
    {
      Local_ErrorState = GPIO_enumWrite8BitsVal(Configuration->DataPort,
                                                Configuration->StartPin,
                                                ~(LOCAL_u8SSDNumbers[Copy_u8Number]));
    }
    else
    {
      Local_ErrorState = NOK;
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }

  return Local_ErrorState;
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    SSD_enumEnable
 * @brief Enable the seven-segment display
 * @details Sets the enable pin according to display type:
 *          - Common Cathode: Enable pin LOW
 *          - Common Anode: Enable pin HIGH
 * @param Configuration: Pointer to SSD configuration structure
 * @return ErrorState_t:
 *         - OK: Display enabled successfully
 *         - NOK: Invalid configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumEnable(const SSD_Config_t *Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (Configuration != NULL)
  {
    if (Configuration->Type == SSD_COMMON_CATHODE)
    {
      Local_ErrorState = GPIO_enumWritePinVal(Configuration->EnablePort,
                                              Configuration->EnablePin,
                                              GPIO_PIN_LOW);
    }
    else if (Configuration->Type == SSD_COMMON_ANODE)
    {
      Local_ErrorState = GPIO_enumWritePinVal(Configuration->EnablePort,
                                              Configuration->EnablePin,
                                              GPIO_PIN_HIGH);
    }
    else
    {
      Local_ErrorState = NOK;
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }

  return Local_ErrorState;
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    SSD_enumDisable
 * @brief Disable the seven-segment display
 * @details Sets the enable pin according to display type:
 *          - Common Cathode: Enable pin HIGH
 *          - Common Anode: Enable pin LOW
 * @param Configuration: Pointer to SSD configuration structure
 * @return ErrorState_t:
 *         - OK: Display disabled successfully
 *         - NOK: Invalid configuration
 *         - NULL_POINTER: Null configuration pointer
 */
ErrorState_t SSD_enumDisable(const SSD_Config_t *Configuration)
{
  ErrorState_t Local_ErrorState = OK;

  if (Configuration != NULL)
  {
    if (Configuration->Type == SSD_COMMON_CATHODE)
    {
      Local_ErrorState = GPIO_enumWritePinVal(Configuration->EnablePort,
                                              Configuration->EnablePin,
                                              GPIO_PIN_HIGH);
    }
    else if (Configuration->Type == SSD_COMMON_ANODE)
    {
      Local_ErrorState = GPIO_enumWritePinVal(Configuration->EnablePort,
                                              Configuration->EnablePin,
                                              GPIO_PIN_LOW);
    }
    else
    {
      Local_ErrorState = NOK;
    }
  }
  else
  {
    Local_ErrorState = NULL_POINTER;
  }

  return Local_ErrorState;
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
