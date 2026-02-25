/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCDT_prog.c    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCDT (LCD with TIM5 Delay)                    **
 **                                                                           **
 **  @note: This driver is identical to CLCD but uses TIM5 (hardware timer)  **
 **         for delays instead of SysTick or software loops.                  **
 **         TIM5 clock must be enabled via RCC before calling CLCDT_vInit().  **
 **         TIM5 is a 32-bit General Purpose Timer on APB1 bus.              **
 **                                                                           **
 **===========================================================================**
 */

/*
 Connections (same as CLCD):
 ###########  4 Bits Mode
 -------------                 ------------
 | STM32     |                 |   LCD    |
 |           |                 |          |
 |        PA7|---------------->|D7        |
 |        PA6|---------------->|D6        |
 |        PA5|---------------->|D5        |
 |        PA4|---------------->|D4        |
 |           |                 |          |
 |        PB2|---------------->|E         |
 |        PB1|---------------->|RW        |
 |        PB0|---------------->|RS        |
 -----------                   ------------
 */

#include "STD_Macros.h"
#include "ErrTypes.h"
#include "STM32F446xx.h"

#include "GPIO_interface.h"
#include "TIM_interface.h"

#include "CLCDT_interface.h"
#include "CLCDT_config.h"
#include "CLCDT_private.h"

/*******************************************************************************
 *                        Private Delay via TIM5                               *
 *******************************************************************************/

/**
 * @fn    _delay_us_tim5
 * @brief Generate microsecond delay using TIM5 hardware timer
 * @param us: Number of microseconds to delay
 * @note  TIM5 Prescaler must produce a 1us tick (1MHz).
 *        With HSI 16MHz: PSC = 15 (16-1) --> 1us tick.
 *        This is a blocking delay.
 */
static void _delay_us_tim5(uint32_t us)
{
    /* Disable TIM5 */
    CLR_BIT(TIM5->CR1, 0); /* CEN = bit 0 */

    /* Load Prescaler: 1us tick at 16MHz --> PSC = 15 */
    TIM5->PSC = CLCDT_TIM5_PRESCALER;

    /* Load ARR with the required delay in us */
    TIM5->ARR = (us > 0u) ? us : 1u;

    /* Reset counter */
    TIM5->CNT = 0;

    /* Clear Update Interrupt Flag */
    CLR_BIT(TIM5->SR, 0); /* UIF = bit 0 */

    /* Generate Update Event to load PSC and ARR immediately */
    SET_BIT(TIM5->EGR, 0); /* UG = bit 0 */
    CLR_BIT(TIM5->SR,  0); /* Clear UIF again after UG */

    /* Enable TIM5 */
    SET_BIT(TIM5->CR1, 0); /* CEN = bit 0 */

    /* Wait for Update Flag (timer overflow) */
    while (READ_BIT(TIM5->SR, 0) == 0); /* Poll UIF */

    /* Disable TIM5 and clear flag */
    CLR_BIT(TIM5->CR1, 0);
    CLR_BIT(TIM5->SR,  0);
}

/**
 * @fn    _delay_ms_tim5
 * @brief Generate millisecond delay using TIM5
 * @param ms: Number of milliseconds to delay
 * @note  Calls _delay_us_tim5(1000) for each millisecond.
 */
static void _delay_ms_tim5(uint32_t ms)
{
    /* For delays longer than 1ms, use TIM_vDelayMs for efficiency */
    /* Each call: PSC=15 (1us tick), ARR=1000 -> 1ms */
    while (ms--)
    {
        _delay_us_tim5(1000u);
    }
}

/*******************************************************************************
 *                        Private GPIO Helpers                                  *
 *******************************************************************************/

/**
 * @fn    CLCDT_InitPin
 * @brief Initialize a single GPIO pin for LCD interface
 */
static ErrorState_t CLCDT_InitPin(GPIO_Port_t Port, GPIO_Pin_t Pin)
{
    GPIO_PinConfig_t PinConfig = {
        .Port      = Port,
        .PinNum    = Pin,
        .Mode      = CLCDT_GPIO_MODE,
        .Otype     = CLCDT_GPIO_OTYPE,
        .Speed     = CLCDT_GPIO_SPEED,
        .PullType  = CLCDT_GPIO_PULL
    };
    return GPIO_enumPinInit(&PinConfig);
}

/**
 * @fn    CLCDT_InitPort8Bits
 * @brief Initialize 8 consecutive GPIO pins for LCD data bus (8-bit mode)
 */
static ErrorState_t CLCDT_InitPort8Bits(GPIO_Port_t Port, GPIO_Pin_t StartPin)
{
    GPIO_8BinsConfig_t PortConfig = {
        .Port      = Port,
        .StartPin  = StartPin,
        .Mode      = CLCDT_GPIO_MODE,
        .Otype     = CLCDT_GPIO_OTYPE,
        .Speed     = CLCDT_GPIO_SPEED,
        .PullType  = CLCDT_GPIO_PULL
    };
    return GPIO_enumPort8BitsInit(&PortConfig);
}

/*******************************************************************************
 *                        Enable Pulse (Falling Edge)                           *
 *******************************************************************************/

/**
 * @fn    CLCDT_vSendFallingEdge
 * @brief Generate LCD enable pulse using TIM5 delay
 */
static void CLCDT_vSendFallingEdge(void)
{
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_EN, GPIO_PIN_HIGH);
    _delay_ms_tim5(1);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_EN, GPIO_PIN_LOW);
    _delay_ms_tim5(1);
}

/*******************************************************************************
 *                        Public Functions                                       *
 *******************************************************************************/

/**
 * @fn    CLCDT_vInit
 * @brief Initialize LCD with TIM5-based delays
 * @note  Call RCC_enumABPPerSts(RCC_APB1, RCC_TIM5EN, RCC_PER_ON) before this.
 */
void CLCDT_vInit(void)
{
#if CLCDT_MODE == CLCDT_EIGHT_BITS_MODE
    /* Initialize control pins */
    CLCDT_InitPin(CLCDT_CONTROL_PORT, CLCDT_RS);
    CLCDT_InitPin(CLCDT_CONTROL_PORT, CLCDT_RW);
    CLCDT_InitPin(CLCDT_CONTROL_PORT, CLCDT_EN);

    /* Initialize data port (8 pins) */
    CLCDT_InitPort8Bits(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN);

    _delay_ms_tim5(50); /* Wait >30ms for VDD rise */

    CLCDT_vSendCommand(CLCDT_HOME);
    _delay_ms_tim5(10);

    CLCDT_vSendCommand(EIGHT_BITS);
    _delay_ms_tim5(1);

    CLCDT_vSendCommand(CLCDT_DISPLAY_CURSOR);
    _delay_ms_tim5(1);

    CLCDT_vClearScreen();

    CLCDT_vSendCommand(CLCDT_ENTRY_MODE);
    _delay_ms_tim5(1);

#elif CLCDT_MODE == CLCDT_FOUR_BITS_MODE
    /* Initialize control pins */
    CLCDT_InitPin(CLCDT_CONTROL_PORT, CLCDT_RS);
    CLCDT_InitPin(CLCDT_CONTROL_PORT, CLCDT_RW);
    CLCDT_InitPin(CLCDT_CONTROL_PORT, CLCDT_EN);

    /* Initialize data pins D4..D7 (High Nibble: PA4..PA7) */
    CLCDT_InitPin(CLCDT_DATA_PORT, GPIO_PIN4);
    CLCDT_InitPin(CLCDT_DATA_PORT, GPIO_PIN5);
    CLCDT_InitPin(CLCDT_DATA_PORT, GPIO_PIN6);
    CLCDT_InitPin(CLCDT_DATA_PORT, GPIO_PIN7);

    _delay_ms_tim5(50); /* Wait >30ms for VDD rise */

    CLCDT_vSendCommand(CLCDT_HOME);
    _delay_ms_tim5(10);

    CLCDT_vSendCommand(FOUR_BITS);
    _delay_ms_tim5(1);

    CLCDT_vSendCommand(CLCDT_DISPLAY_CURSOR);
    _delay_ms_tim5(1);

    CLCDT_vSendCommand(CLCDT_ENTRY_MODE);
    _delay_ms_tim5(1);

#else
#error "Wrong CLCDT_MODE Config"
#endif
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSendData
 * @brief Send data byte to LCD
 */
void CLCDT_vSendData(uint8_t Copy_u8Data)
{
#if CLCDT_MODE == CLCDT_EIGHT_BITS_MODE
    GPIO_enumWrite8BitsVal(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN, Copy_u8Data);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RS, GPIO_PIN_HIGH);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RW, GPIO_PIN_LOW);
    CLCDT_vSendFallingEdge();

#elif CLCDT_MODE == CLCDT_FOUR_BITS_MODE
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RS, GPIO_PIN_HIGH);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RW, GPIO_PIN_LOW);

    /* Send High Nibble first */
    GPIO_enumWrite4BitsVal(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN, (Copy_u8Data >> 4));
    CLCDT_vSendFallingEdge();

    /* Send Low Nibble */
    GPIO_enumWrite4BitsVal(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN, Copy_u8Data);
    CLCDT_vSendFallingEdge();

#else
#error "Wrong CLCDT_MODE Config"
#endif
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSendCommand
 * @brief Send command byte to LCD
 */
void CLCDT_vSendCommand(uint8_t Copy_u8Command)
{
#if CLCDT_MODE == CLCDT_EIGHT_BITS_MODE
    GPIO_enumWrite8BitsVal(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN, Copy_u8Command);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RS, GPIO_PIN_LOW);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RW, GPIO_PIN_LOW);
    CLCDT_vSendFallingEdge();

#elif CLCDT_MODE == CLCDT_FOUR_BITS_MODE
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RS, GPIO_PIN_LOW);
    GPIO_enumWritePinVal(CLCDT_CONTROL_PORT, CLCDT_RW, GPIO_PIN_LOW);

    /* Send High Nibble first */
    GPIO_enumWrite4BitsVal(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN, (Copy_u8Command >> 4));
    CLCDT_vSendFallingEdge();

    /* Send Low Nibble */
    GPIO_enumWrite4BitsVal(CLCDT_DATA_PORT, CLCDT_DATA_START_PIN, Copy_u8Command);
    CLCDT_vSendFallingEdge();

#else
#error "Wrong CLCDT_MODE Config"
#endif
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vClearScreen
 * @brief Clear LCD display and return cursor home
 */
void CLCDT_vClearScreen(void)
{
    CLCDT_vSendCommand(CLCDT_ClEAR);
    _delay_ms_tim5(10); /* Clear command takes >1.53ms */
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSetPosition
 * @brief Set cursor to specific position
 * @param Copy_u8ROW: Row number (1-4)
 * @param Copy_u8Col: Column number (1-20)
 */
void CLCDT_vSetPosition(uint8_t Copy_u8ROW, uint8_t Copy_u8Col)
{
    uint8_t LOC_u8Data;

    if ((Copy_u8ROW < CLCDT_ROW_1) || (Copy_u8ROW > CLCDT_ROW_4) ||
        (Copy_u8Col < CLCDT_COL_1) || (Copy_u8Col > CLCDT_COL_20))
    {
        LOC_u8Data = CLCDT_SET_CURSOR;
    }
    else if (Copy_u8ROW == CLCDT_ROW_1)
    {
        LOC_u8Data = (CLCDT_SET_CURSOR) + (Copy_u8Col - 1);
    }
    else if (Copy_u8ROW == CLCDT_ROW_2)
    {
        LOC_u8Data = (CLCDT_SET_CURSOR) + 64 + (Copy_u8Col - 1);
    }
    else if (Copy_u8ROW == CLCDT_ROW_3)
    {
        LOC_u8Data = (CLCDT_SET_CURSOR) + 20 + (Copy_u8Col - 1);
    }
    else /* ROW_4 */
    {
        LOC_u8Data = (CLCDT_SET_CURSOR) + 84 + (Copy_u8Col - 1);
    }

    CLCDT_vSendCommand(LOC_u8Data);
    _delay_ms_tim5(1);
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSendString
 * @brief Display string on LCD
 */
void CLCDT_vSendString(const uint8_t *Copy_u8PrtString)
{
    uint8_t LOC_u8Iterator = 0;
    while (Copy_u8PrtString[LOC_u8Iterator] != '\0')
    {
        CLCDT_vSendData(Copy_u8PrtString[LOC_u8Iterator]);
        LOC_u8Iterator++;
    }
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSendIntNumber
 * @brief Display integer number on LCD
 */
void CLCDT_vSendIntNumber(int32_t Copy_s32Number)
{
    uint32_t LOC_u32Reverse = 1;

    if (Copy_s32Number == 0)
    {
        CLCDT_vSendData('0');
    }
    else
    {
        if (Copy_s32Number < 0)
        {
            CLCDT_vSendData('-');
            Copy_s32Number = (-1 * Copy_s32Number);
        }
        while (Copy_s32Number != 0)
        {
            LOC_u32Reverse = (LOC_u32Reverse * 10) + (Copy_s32Number % 10);
            Copy_s32Number /= 10;
        }
        while (LOC_u32Reverse != 1)
        {
            CLCDT_vSendData((LOC_u32Reverse % 10) + 48);
            LOC_u32Reverse /= 10;
        }
    }
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSendFloatNumber
 * @brief Display floating point number on LCD
 */
void CLCDT_vSendFloatNumber(double Copy_f64Number)
{
    CLCDT_vSendIntNumber((int32_t)Copy_f64Number);
    if (Copy_f64Number < 0)
    {
        Copy_f64Number *= -1;
    }
    Copy_f64Number = (double)Copy_f64Number - (int32_t)Copy_f64Number;
    Copy_f64Number *= 10000;
    if ((int64_t)Copy_f64Number != 0)
    {
        CLCDT_vSendData('.');
        CLCDT_vSendIntNumber((int32_t)Copy_f64Number);
    }
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_voidShiftDisplayRight
 * @brief Shift entire display content right
 */
void CLCDT_voidShiftDisplayRight(void)
{
    CLCDT_vSendCommand(CLCDT_SHIFT_DISPLAY_RIGHT);
    _delay_ms_tim5(1);
}

/**
 * @fn    CLCDT_voidShiftDisplayLeft
 * @brief Shift entire display content left
 */
void CLCDT_voidShiftDisplayLeft(void)
{
    CLCDT_vSendCommand(CLCDT_SHIFT_DISPLAY_LEFT);
    _delay_ms_tim5(1);
}

/*___________________________________________________________________________________________________________________*/

/**
 * @fn    CLCDT_vSendExtraChar
 * @brief Display custom character from CGRAM
 */
void CLCDT_vSendExtraChar(uint8_t Copy_u8Row, uint8_t Copy_u8Col)
{
    /* Custom char array - add your patterns here */
    uint8_t CLCDT_u8ExtraChar[] = { /* empty - add patterns as needed */ };
    uint8_t LOC_u8Iterator = 0;

    /* Go To CGRAM */
    CLCDT_vSendCommand(CLCDT_CGRAM);

    /* Draw Character in CGRAM */
    for (LOC_u8Iterator = 0;
         LOC_u8Iterator < (sizeof(CLCDT_u8ExtraChar) / sizeof(CLCDT_u8ExtraChar[0]));
         LOC_u8Iterator++)
    {
        CLCDT_vSendData(CLCDT_u8ExtraChar[LOC_u8Iterator]);
    }

    /* Back to DDRAM */
    CLCDT_vSetPosition(Copy_u8Row, Copy_u8Col);

    /* Send Character Addresses */
    for (LOC_u8Iterator = 0; LOC_u8Iterator < 8; LOC_u8Iterator++)
    {
        CLCDT_vSendData(LOC_u8Iterator);
    }
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<    END    >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
