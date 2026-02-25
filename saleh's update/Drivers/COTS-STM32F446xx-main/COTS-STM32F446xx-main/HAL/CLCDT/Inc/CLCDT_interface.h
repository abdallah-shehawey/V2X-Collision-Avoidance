/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCDT_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCDT (LCD with TIM5 Delay)                    **
 **                                                                           **
 **===========================================================================**
 */

#ifndef CLCDT_INTERFACE_H_
#define CLCDT_INTERFACE_H_

/********************************
 * @brief LCD Command Definitions
 ********************************/
#define EIGHT_BITS                    0x38
#define FOUR_BITS                     0x28
#define CLCDT_DISPLAYON_CURSOROFF     0x0c
#define CLCDT_DISPLAYOFF_CURSOROFF    0x08
#define CLCDT_DISPLAYON_CURSORON      0x0e
#define CLCDT_DISPLAYON_CURSORON_BLINK 0x0f
#define CLCDT_ClEAR                   0x01
#define CLCDT_ENTRY_MODE              0x06
#define CLCDT_HOME                    0x02
#define CLCDT_SET_CURSOR              0x80
#define CLCDT_CGRAM                   0x40
#define CLCDT_SHIFT_DISPLAY_RIGHT     0x1c
#define CLCDT_SHIFT_DISPLAY_LEFT      0x18
#define CLCDT_SHIFT_CURSOR_RIGHT      0x14
#define CLCDT_SHIFT_CURSOR_LEFT       0x10

/********************************
 * @brief LCD Row and Column Definitions
 ********************************/
#define CLCDT_ROW_1  1
#define CLCDT_ROW_2  2
#define CLCDT_ROW_3  3
#define CLCDT_ROW_4  4

#define CLCDT_COL_1   1
#define CLCDT_COL_2   2
#define CLCDT_COL_3   3
#define CLCDT_COL_4   4
#define CLCDT_COL_5   5
#define CLCDT_COL_6   6
#define CLCDT_COL_7   7
#define CLCDT_COL_8   8
#define CLCDT_COL_9   9
#define CLCDT_COL_10  10
#define CLCDT_COL_11  11
#define CLCDT_COL_12  12
#define CLCDT_COL_13  13
#define CLCDT_COL_14  14
#define CLCDT_COL_15  15
#define CLCDT_COL_16  16
#define CLCDT_COL_17  17
#define CLCDT_COL_18  18
#define CLCDT_COL_19  19
#define CLCDT_COL_20  20

/********************************
 * @brief LCD Mode Definitions
 ********************************/
#define CLCDT_FOUR_BITS_MODE   4
#define CLCDT_EIGHT_BITS_MODE  8

/********************************
 * @brief Function Declarations
 ********************************/

/**
 * @fn    CLCDT_vInit
 * @brief Initialize LCD with TIM5-based delays
 * @note  Must be called before using any other CLCDT functions.
 *        TIM5 clock must be enabled before calling this (RCC_TIM5EN).
 */
void CLCDT_vInit(void);

/**
 * @fn    CLCDT_vSendData
 * @brief Send data byte to LCD
 * @param Copy_u8Data: Character or custom pattern to display
 */
void CLCDT_vSendData(uint8_t Copy_u8Data);

/**
 * @fn    CLCDT_vSendCommand
 * @brief Send command byte to LCD
 * @param Copy_u8Command: Command code to execute
 */
void CLCDT_vSendCommand(uint8_t Copy_u8Command);

/**
 * @fn    CLCDT_vClearScreen
 * @brief Clear LCD display and return cursor home
 */
void CLCDT_vClearScreen(void);

/**
 * @fn    CLCDT_vSetPosition
 * @brief Set cursor to specific position
 * @param Copy_u8ROW: Row number (1-4)
 * @param Copy_u8Col: Column number (1-20)
 */
void CLCDT_vSetPosition(uint8_t Copy_u8ROW, uint8_t Copy_u8Col);

/**
 * @fn    CLCDT_vSendString
 * @brief Display string on LCD
 * @param Copy_u8PrtString: Pointer to null-terminated string
 */
void CLCDT_vSendString(const uint8_t *Copy_u8PrtString);

/**
 * @fn    CLCDT_vSendIntNumber
 * @brief Display integer number on LCD
 * @param Copy_s32Number: Integer number to display
 */
void CLCDT_vSendIntNumber(int32_t Copy_s32Number);

/**
 * @fn    CLCDT_vSendFloatNumber
 * @brief Display floating point number on LCD
 * @param Copy_f64Number: Float number to display
 */
void CLCDT_vSendFloatNumber(double Copy_f64Number);

/**
 * @fn    CLCDT_voidShiftDisplayRight
 * @brief Shift entire display content right
 */
void CLCDT_voidShiftDisplayRight(void);

/**
 * @fn    CLCDT_voidShiftDisplayLeft
 * @brief Shift entire display content left
 */
void CLCDT_voidShiftDisplayLeft(void);

/**
 * @fn    CLCDT_vSendExtraChar
 * @brief Display custom character from CGRAM
 * @param Copy_u8Row: Row position (1-4)
 * @param Copy_u8Col: Column position (1-20)
 */
void CLCDT_vSendExtraChar(uint8_t Copy_u8Row, uint8_t Copy_u8Col);

#endif /* CLCDT_INTERFACE_H_ */
