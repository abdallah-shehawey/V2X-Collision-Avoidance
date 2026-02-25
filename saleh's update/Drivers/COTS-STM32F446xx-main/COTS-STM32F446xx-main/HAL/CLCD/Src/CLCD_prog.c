/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCD_program.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCD                                            **
 **                                                                           **
 **===========================================================================**
 */

#include <SYSTIC_interface.h>

#include "STD_Macros.h"
#include "ErrTypes.h"

#include "GPIO_interface.h"

#include "CLCD_interface.h"
#include "CLCD_config.h"
#include "CLCD_private.h"
#include "CLCD_extrachar.h"

/********************************
 * @file    CLCD_prog.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Character LCD (CLCD) Implementation File
 * @details This file contains the implementations for Character LCD control functions
 *          Supports both 4-bit and 8-bit modes of operation
 ********************************/

/*___________________________________________________________________________________________________________________*/
/*
###########  8 Bits Mode                                 ###########  4 Bits Mode
-------------                 ------------               -------------                 ------------
| ATmega32  |                 |   LCD    |               | ATmega32  |                 |   LCD    |
|           |                 |          |               |           |                 |          |
|        PA7|---------------->|D7        |               | PA3 or PA7|---------------->|D7        |
|        PA6|---------------->|D6        |               | PA2 or PA6|---------------->|D6        |
|        PA5|---------------->|D5        |               | PA1 or PA5|---------------->|D5        |
|        PA4|---------------->|D4        |               | PA0 or PA4|---------------->|D4        |
|        PA3|---------------->|D3        |               |           |                 |          |
|        PA2|---------------->|D2        |               |        PB2|---------------->|E         |
|        PA1|---------------->|D1        |               |        PB1|---------------->|RW        |
|        PA0|---------------->|D0        |               |        PB0|---------------->|RS        |
|           |                 |          |               |           |                 |          |
|        PB2|---------------->|E         |               |           |                 |          |
|        PB1|---------------->|RW        |               |           |                 |          |
|        PB0|---------------->|RS        |               |           |                 |          |
-----------                   ------------               -------------                 ------------
 */

/*___________________________________________________________________________________________________________________*/

/*******************************************************************************
 *                           Function Implementations                            *
 *******************************************************************************/

/**
 * @fn    CLCD_vInit
 * @brief Initialize LCD with configured settings
 * @details This function:
 *          1. Initializes GPIO pins for control and data
 *          2. Waits for LCD power-up (>30ms)
 *          3. Configures 4-bit or 8-bit mode
 *          4. Sets up display parameters (cursor, lines, etc)
 * @note  Must be called before using any other LCD functions
 */
void CLCD_vInit(void)
{
#if CLCD_MODE == CLCD_EIGHT_BITS_MODE
  /* Initialize control pins */
  CLCD_InitPin(CLCD_CONTROL_PORT, CLCD_RS);
  CLCD_InitPin(CLCD_CONTROL_PORT, CLCD_RW);
  CLCD_InitPin(CLCD_CONTROL_PORT, CLCD_EN);

  /* Initialize data port */
  CLCD_InitPort8Bits(CLCD_DATA_PORT, CLCD_DATA_START_PIN);

  SYSTIC_vDelayMs(50); // Must wait more than 30 ms before any action (VDD rises to 4.5v)

  CLCD_vSendCommand(CLCD_HOME);
  SYSTIC_vDelayMs(10);

  CLCD_vSendCommand(EIGHT_BITS);
  SYSTIC_vDelayMs(1);

  CLCD_vSendCommand(CLCD_DISPLAY_CURSOR);
  SYSTIC_vDelayMs(1);

  CLCD_vClearScreen();

  CLCD_vSendCommand(CLCD_ENTRY_MODE);
  SYSTIC_vDelayMs(1);

#elif CLCD_MODE == CLCD_FOUR_BITS_MODE
  /* Initialize control pins */
  CLCD_InitPin(CLCD_CONTROL_PORT, CLCD_RS);
  CLCD_InitPin(CLCD_CONTROL_PORT, CLCD_RW);
  CLCD_InitPin(CLCD_CONTROL_PORT, CLCD_EN);

  /* Initialize data pins */
#if CLCD_DATA_NIBBLE == CLCD_HIGH_NIBBLE
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN4);
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN5);
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN6);
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN7);
#elif CLCD_DATA_NIBBLE == CLCD_LOW_NIBBLE
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN0);
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN1);
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN2);
  CLCD_InitPin(CLCD_DATA_PORT, GPIO_PIN3);
#else
#error "Wrong CLCD_DATA_NIBBLE Config"
#endif

  SYSTIC_vDelayMs(50); // Must wait more than 30 ms before any action (VDD rises to 4.5v)

  CLCD_vSendCommand(CLCD_HOME);
  SYSTIC_vDelayMs(10);

  CLCD_vSendCommand(FOUR_BITS);
  SYSTIC_vDelayMs(1);

  CLCD_vSendCommand(CLCD_DISPLAY_CURSOR);
  SYSTIC_vDelayMs(1);

  CLCD_vSendCommand(CLCD_ENTRY_MODE);
  SYSTIC_vDelayMs(1);

#else
#error "Wrong CLCD_MODE Config"
#endif
}

/**
 * @fn    CLCD_InitPin
 * @brief Initialize a single GPIO pin for LCD interface
 * @param Port: GPIO port to initialize
 * @param Pin: GPIO pin number to initialize
 * @return ErrorState_t: OK if successful, error code otherwise
 * @note  Private helper function
 */
static ErrorState_t CLCD_InitPin(GPIO_Port_t Port, GPIO_Pin_t Pin)
{
  GPIO_PinConfig_t PinConfig = {
      .Port = Port,
      .PinNum = Pin,
      .Mode = CLCD_GPIO_MODE,
      .Otype = CLCD_GPIO_OTYPE,
      .Speed = CLCD_GPIO_SPEED,
      .PullType = CLCD_GPIO_PULL
    };

  return GPIO_enumPinInit(&PinConfig);
}

/**
 * @fn    CLCD_InitPort8Bits
 * @brief Initialize 8 consecutive GPIO pins for LCD data bus
 * @param Port: GPIO port to initialize
 * @param StartPin: First pin number of the 8-bit group
 * @return ErrorState_t: OK if successful, error code otherwise
 * @note  Private helper function for 8-bit mode
 */
static ErrorState_t CLCD_InitPort8Bits(GPIO_Port_t Port, GPIO_Pin_t StartPin)
{
  GPIO_8PinsConfig_t PortConfig = {
      .Port = Port,
      .StartPin = StartPin,
      .Mode = CLCD_GPIO_MODE,
      .Otype = CLCD_GPIO_OTYPE,
      .Speed = CLCD_GPIO_SPEED,
      .PullType = CLCD_GPIO_PULL};

  return GPIO_enumPort8PinsInit(&PortConfig);
}
/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_vSendData
 * @brief Send data byte to LCD
 * @details This function:
 *          1. Sets RS=1 for data mode
 *          2. Sets RW=0 for write mode
 *          3. Sends data in either 8-bit or 4-bit mode
 *          4. Generates enable pulse
 * @param Copy_u8Data: Character or custom pattern to display
 */
void CLCD_vSendData(uint8_t Copy_u8Data)
{
#if CLCD_MODE == CLCD_EIGHT_BITS_MODE
  GPIO_enumWrite8BitsVal(CLCD_DATA_PORT, CLCD_DATA_START_PIN, Copy_u8Data);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RS, GPIO_PIN_HIGH);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RW, GPIO_PIN_LOW);
  CLCD_vSendFallingEdge();

#elif CLCD_MODE == CLCD_FOUR_BITS_MODE
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RS, GPIO_PIN_HIGH);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RW, GPIO_PIN_LOW);

  GPIO_enumWrite4PinsVal(CLCD_DATA_PORT, CLCD_DATA_START_PIN, (Copy_u8Data >> 4));
  CLCD_vSendFallingEdge();
  GPIO_enumWrite4PinsVal(CLCD_DATA_PORT, CLCD_DATA_START_PIN, Copy_u8Data);
  CLCD_vSendFallingEdge();

#else
#error "Wrong CLCD_MODE Config"
#endif
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_vSendCommand
 * @brief Send command byte to LCD
 * @details This function:
 *          1. Sets RS=0 for command mode
 *          2. Sets RW=0 for write mode
 *          3. Sends command in either 8-bit or 4-bit mode
 *          4. Generates enable pulse
 * @param Copy_u8Command: Command code to execute
 */
void CLCD_vSendCommand(uint8_t Copy_u8Command)
{
#if CLCD_MODE == CLCD_EIGHT_BITS_MODE
  GPIO_enumWrite8BitsVal(CLCD_DATA_PORT, CLCD_DATA_START_PIN, Copy_u8Command);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RS, GPIO_PIN_LOW);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RW, GPIO_PIN_LOW);
  CLCD_vSendFallingEdge();

#elif CLCD_MODE == CLCD_FOUR_BITS_MODE
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RS, GPIO_PIN_LOW);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_RW, GPIO_PIN_LOW);

  GPIO_enumWrite4PinsVal(CLCD_DATA_PORT, CLCD_DATA_START_PIN, (Copy_u8Command >> 4));
  CLCD_vSendFallingEdge();
  GPIO_enumWrite4PinsVal(CLCD_DATA_PORT, CLCD_DATA_START_PIN, Copy_u8Command);
  CLCD_vSendFallingEdge();

#else
#error "Wrong CLCD_MODE Config"
#endif
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_vSendFallingEdge
 * @brief Generate enable pulse for LCD
 * @details This function:
 *          1. Sets E=1
 *          2. Waits 1ms
 *          3. Sets E=0
 *          4. Waits 1ms
 * @note  Private helper function
 */
static void CLCD_vSendFallingEdge(void)
{
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_EN, GPIO_PIN_HIGH);
  _delay_ms(1);
  GPIO_enumWritePinVal(CLCD_CONTROL_PORT, CLCD_EN, GPIO_PIN_LOW);
  _delay_ms(1);
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_vClearScreen
 * @brief Clear LCD display and return cursor home
 * @details Sends clear display command and waits >1.53ms for execution
 */
void CLCD_vClearScreen(void)
{
  CLCD_vSendCommand(CLCD_ClEAR);
  _delay_ms(10); // wait more than 1.53 ms
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_vSendString
 * @brief Display string on LCD
 * @details Sends each character of string to LCD
 * @param Copy_u8PrtStrign: Pointer to null-terminated string
 * @note  Handles strings up to LCD display capacity
 */
void CLCD_vSendString(const uint8_t *Copy_u8PrtStrign)
{
  uint8_t LOC_u8Iterator = 0;
  while (Copy_u8PrtStrign[LOC_u8Iterator] != '\0')
  {
    CLCD_vSendData(Copy_u8PrtStrign[LOC_u8Iterator]);
    LOC_u8Iterator++;
  }
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_vSendIntNumber
 * @brief Display integer number on LCD
 * @details Converts integer to string and displays it
 * @param Copy_s32Number: Integer number to display
 * @note  Handles positive and negative numbers
 */
void CLCD_vSendIntNumber(int32_t Copy_s32Number)
{

  uint32_t LOC_u32Reverse = 1;

  if (Copy_s32Number == 0)
  {
    CLCD_vSendData('0');
  }
  else
  {
    if (Copy_s32Number < 0)
    {
      CLCD_vSendData('-');
      Copy_s32Number = (-1 * Copy_s32Number);
    }
    while (Copy_s32Number != 0)
    {
      LOC_u32Reverse = (LOC_u32Reverse * 10) + (Copy_s32Number % 10);
      Copy_s32Number /= 10;
    }
    while (LOC_u32Reverse != 1)
    {
      CLCD_vSendData((LOC_u32Reverse % 10) + 48);
      LOC_u32Reverse /= 10;
    }
  }
}

/**
 * @fn    CLCD_vSendFloatNumber
 * @brief Display floating point number on LCD
 * @details Converts float to string with decimal places
 * @param Copy_f64Number: Float number to display
 * @note  Displays up to 6 decimal places
 */
void CLCD_vSendFloatNumber(double Copy_f64Number)
{
  CLCD_vSendIntNumber((int32_t)Copy_f64Number);
  if (Copy_f64Number < 0)
  {
    Copy_f64Number *= -1;
  }
  Copy_f64Number = (double)Copy_f64Number - (int32_t)Copy_f64Number;
  Copy_f64Number *= 10000;
  if ((int64_t)Copy_f64Number != 0)
  {
    CLCD_vSendData('.');
    CLCD_vSendIntNumber((int32_t)Copy_f64Number);
  }
}
/*------------------------------------------------------------------------------------------------------------------------------------------------------
 *         	                                      This Function set the cursor position
 *                                            *-------------------------------------------*
 * Parameters :
 *       => Copy_u8Row --> row number (CLCD_ROW_1 or CLCD_ROW_2)
 *		 => Copy_u8Col --> column number (CLCD_COL_1 ... CLCD_COL_16)
 * return     : nothing
 *
 * Hint       :-
 *		In This function we send a command which =0b1xxxxxxx
 *		MSB = 1  ===> refers that it is command to set cursor
 *		xxxxxxx  ===> refers to AC ( Address Counter 7Bits / DDRAM Locations 128Location )
 */

/**
 * @fn    CLCD_vSetPosition
 * @brief Set cursor to specific position
 * @details Calculates DDRAM address and sends cursor command
 * @param Copy_u8ROW: Row number (1-4)
 * @param Copy_u8Col: Column number (1-20)
 * @note  Row and column numbers start from 1
 */
void CLCD_vSetPosition(uint8_t Copy_u8ROW, uint8_t Copy_u8Col)
{
  uint8_t LOC_u8Data;

  if ((Copy_u8ROW < CLCD_ROW_1) || (Copy_u8ROW > CLCD_ROW_4) || (Copy_u8Col < CLCD_COL_1) || (Copy_u8Col > CLCD_COL_20))
  {
    LOC_u8Data = CLCD_SET_CURSOR;
  }
  else if (Copy_u8ROW == CLCD_ROW_1)
  {
    LOC_u8Data = ((CLCD_SET_CURSOR) + (Copy_u8Col - 1));
  }
  else if (Copy_u8ROW == CLCD_ROW_2)
  {
    LOC_u8Data = ((CLCD_SET_CURSOR) + (64) + (Copy_u8Col - 1));
  }
  else if (Copy_u8ROW == CLCD_ROW_3)
  {
    LOC_u8Data = ((CLCD_SET_CURSOR) + (20) + (Copy_u8Col - 1));
  }
  else if (Copy_u8ROW == CLCD_ROW_4)
  {
    LOC_u8Data = ((CLCD_SET_CURSOR) + (84) + (Copy_u8Col - 1));
  }

  CLCD_vSendCommand(LOC_u8Data);
  _delay_ms(1);
}

/**
 * @fn    CLCD_vSendExtraChar
 * @brief Display custom character from CGRAM
 * @details Retrieves custom pattern from CGRAM and displays it
 * @param Copy_u8Row: Row position (1-4)
 * @param Copy_u8Col: Column position (1-20)
 * @note  Custom characters must be previously stored in CGRAM
 */
void CLCD_vSendExtraChar(uint8_t Copy_u8Row, uint8_t Copy_u8Col)
{

  uint8_t LOC_u8Iterator = 0;

  /* 1- Go To CGRAM            */
  CLCD_vSendCommand(CLCD_CGRAM); // Make AC refers to the first Place/Address at CGRAM

  /* 2- Draw Character in CGRAM        */
  /* Hint : it will be copied to DDRAM automatically */
  for (LOC_u8Iterator = 0; LOC_u8Iterator < (sizeof(CLCD_u8ExtraChar) / sizeof(CLCD_u8ExtraChar[0])); LOC_u8Iterator++)
  {
    CLCD_vSendData(CLCD_u8ExtraChar[LOC_u8Iterator]);
  }

  /* 3- Back (AC) To DDRAM          */
  CLCD_vSetPosition(Copy_u8Row, Copy_u8Col);

  /* 4- Send Character Address */
  for (LOC_u8Iterator = 0; LOC_u8Iterator < 8; LOC_u8Iterator++)
  {
    CLCD_vSendData(LOC_u8Iterator);
  }
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCD_voidShiftDisplayRight
 * @brief Shift entire display content right
 * @details Sends shift display command
 */
void CLCD_voidShiftDisplayRight(void)
{
  CLCD_vSendCommand(CLCD_SHIFT_DISPLAY_RIGHT);
  _delay_ms(1);
}

/**
 * @fn    CLCD_voidShiftDisplayLeft
 * @brief Shift entire display content left
 * @details Sends shift display command
 */
void CLCD_voidShiftDisplayLeft(void)
{
  CLCD_vSendCommand(CLCD_SHIFT_DISPLAY_LEFT);
  _delay_ms(1);
}

/* Delay Functions Implementation */
/**
 * @fn    _delay_us
 * @brief Generate microsecond delay
 * @param us: Number of microseconds to delay
 * @note  Private helper function using SYSTICK timer
 */
static void _delay_us(uint32_t us)
{
  /* Calculate number of cycles needed
   * For 16MHz:
   * 1 cycle = 1/16MHz = 0.0625 us
   * We need 16 cycles for 1 us
   * Each loop iteration takes about 4 cycles
   * So we need to loop 4 times for 1 us
   */
  uint32_t cycles = us * 4;

  /* Volatile to prevent optimization */
  volatile uint32_t i;
  for (i = 0; i < cycles; i++)
  {
    __asm("NOP"); // No operation - takes 1 cycle
  }
}

/**
 * @fn    _delay_ms
 * @brief Generate millisecond delay
 * @param ms: Number of milliseconds to delay
 * @note  Private helper function using SYSTICK timer
 */
static void _delay_ms(uint32_t ms)
{
  while (ms--)
  {
    _delay_us(1000); // 1000 us = 1 ms
  }
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
