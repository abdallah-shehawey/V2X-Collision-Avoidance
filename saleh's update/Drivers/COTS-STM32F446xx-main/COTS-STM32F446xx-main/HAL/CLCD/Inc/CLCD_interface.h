/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    CLCD_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : CLCD                                            **
 **                                                                           **
 **===========================================================================**
 */

/********************************
 * @file    CLCD_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Character LCD (CLCD) Interface Header File
 * @details This file contains the declarations for Character LCD control functions
 *          Supports both 4-bit and 8-bit modes of operation
 ********************************/

#ifndef CLCD_INTERFACE_H_
#define CLCD_INTERFACE_H_

/********************************
 * @brief LCD Command Definitions
 * @details These commands control various LCD operations
 ********************************/
#define EIGHT_BITS 0x38                    /**< 8-bit mode: 8-bit data, 2-line display, 5x7 font */
#define FOUR_BITS 0x28                     /**< 4-bit mode: 4-bit data, 2-line display, 5x7 font */
#define CLCD_DISPLAYON_CURSOROFF 0x0c      /**< Display ON, cursor OFF */
#define CLCD_DISPLAYOFF_CURSOROFF 0x08     /**< Display OFF, cursor OFF */
#define CLCD_DISPLAYON_CURSORON 0x0e       /**< Display ON, cursor ON */
#define CLCD_DISPLAYON_CURSORON_BLINK 0x0f /**< Display ON, cursor ON with blink */
#define CLCD_ClEAR 0x01                    /**< Clear display and return home */
#define CLCD_ENTRY_MODE 0x06               /**< Auto-increment cursor position */
#define CLCD_HOME 0x02                     /**< Return cursor to home position */
#define CLCD_SET_CURSOR 0x80               /**< Set DDRAM address for cursor */
#define CLCD_CGRAM 0x40                    /**< Set CGRAM address for custom chars */
#define CLCD_SHIFT_DISPLAY_RIGHT 0x1c      /**< Shift entire display right */
#define CLCD_SHIFT_DISPLAY_LEFT 0x18       /**< Shift entire display left */
#define CLCD_SHIFT_CURSOR_RIGHT 0x14       /**< Move cursor right */
#define CLCD_SHIFT_CURSOR_LEFT 0x10        /**< Move cursor left */

/*___________________________________________________________________________________________________________________*/

/********************************
 * @brief LCD Row and Column Definitions
 * @details Physical layout positions for 20x4 LCD
 ********************************/
#define CLCD_ROW_1 1
#define CLCD_ROW_2 2
#define CLCD_ROW_3 3
#define CLCD_ROW_4 4

#define CLCD_COL_1 1
#define CLCD_COL_2 2
#define CLCD_COL_3 3
#define CLCD_COL_4 4
#define CLCD_COL_5 5
#define CLCD_COL_6 6
#define CLCD_COL_7 7
#define CLCD_COL_8 8
#define CLCD_COL_9 9
#define CLCD_COL_10 10
#define CLCD_COL_11 11
#define CLCD_COL_12 12
#define CLCD_COL_13 13
#define CLCD_COL_14 14
#define CLCD_COL_15 15
#define CLCD_COL_16 16
#define CLCD_COL_17 17
#define CLCD_COL_18 18
#define CLCD_COL_19 19
#define CLCD_COL_20 20

/********************************
 * @brief LCD Mode Definitions
 * @details Interface data width selection
 ********************************/
#define CLCD_FOUR_BITS_MODE 4  /**< 4-bit data interface mode */
#define CLCD_EIGHT_BITS_MODE 8 /**< 8-bit data interface mode */

/*___________________________________________________________________________________________________________________*/

/********************************
 * @brief Function Declarations
 ********************************/

/**
 * @fn    CLCD_vInit
 * @brief Initialize LCD with configured settings
 * @note  Must be called before using any other LCD functions
 * @example CLCD_vInit();
 */
void CLCD_vInit(void);

/**
 * @fn    CLCD_vSendData
 * @brief Send data byte to LCD
 * @param Copy_u8Data: Character or custom pattern to display
 * @example CLCD_vSendData('A');
 */
void CLCD_vSendData(uint8_t Copy_u8Data);

/**
 * @fn    CLCD_vSendCommand
 * @brief Send command byte to LCD
 * @param Copy_u8Command: Command code to execute
 * @example CLCD_vSendCommand(CLCD_ClEAR);
 */
void CLCD_vSendCommand(uint8_t Copy_u8Command);

/**
 * @fn    CLCD_vClearScreen
 * @brief Clear LCD display and return cursor home
 * @example CLCD_vClearScreen();
 */
void CLCD_vClearScreen(void);

/**
 * @fn    CLCD_vSetPosition
 * @brief Set cursor to specific position
 * @param Copy_u8ROW: Row number (1-4)
 * @param Copy_u8Col: Column number (1-20)
 * @example CLCD_vSetPosition(CLCD_ROW_1, CLCD_COL_5);
 */
void CLCD_vSetPosition(uint8_t Copy_u8ROW, uint8_t Copy_u8Col);

/**
 * @fn    CLCD_vSendString
 * @brief Display string on LCD
 * @param Copy_u8PrtString: Pointer to null-terminated string
 * @example CLCD_vSendString("Hello");
 */
void CLCD_vSendString(const uint8_t *Copy_u8PrtString);

/**
 * @fn    CLCD_vSendIntNumber
 * @brief Display integer number on LCD
 * @param Copy_s32Number: Integer number to display
 * @example CLCD_vSendIntNumber(1234);
 */
void CLCD_vSendIntNumber(int32_t Copy_s32Number);

/**
 * @fn    CLCD_vSendFloatNumber
 * @brief Display floating point number on LCD
 * @param Copy_f64Number: Float number to display
 * @example CLCD_vSendFloatNumber(3.14);
 */
void CLCD_vSendFloatNumber(double Copy_f64Number);

/**
 * @fn    CLCD_voidShiftDisplayRight
 * @brief Shift entire display content right
 */
void CLCD_voidShiftDisplayRight(void);

/**
 * @fn    CLCD_voidShiftDisplayLeft
 * @brief Shift entire display content left
 */
void CLCD_voidShiftDisplayLeft(void);

/**
 * @fn    CLCD_vSendExtraChar
 * @brief Display custom character from CGRAM
 * @param Copy_u8Row: Row position (1-4)
 * @param Copy_u8Col: Column position (1-20)
 */
void CLCD_vSendExtraChar(uint8_t Copy_u8Row, uint8_t Copy_u8Col);

#endif /* CLCD_INTERFACE_H_ */
