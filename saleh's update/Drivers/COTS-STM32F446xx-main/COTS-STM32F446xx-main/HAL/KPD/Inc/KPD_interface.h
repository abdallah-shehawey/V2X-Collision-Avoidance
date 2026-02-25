/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    KPD_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : HAL                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : F446xx                                          **
 **                  SWC    : KPD                                             **
 **                                                                           **
 **===========================================================================**
 */

#ifndef KPD_INTERFACE_H_
#define KPD_INTERFACE_H_

/**
 * @brief Value returned when no key is pressed
 * @note This value (0xFF) is chosen because it's outside the valid keypad values
 */
#define NOTPRESSED 0xFF

/**
 * @fn    KPD_vInit
 * @brief Initialize keypad GPIO pins
 * @details Configures:
 *          - Row pins as input pull-up
 *          - Column pins as output push-pull
 *          - Sets initial column states to HIGH
 * @example KPD_vInit();
 */
void KPD_vInit(void);

/**
 * @fn    KPD_u8GetPressed
 * @brief Get the currently pressed key value
 * @return uint8_t:
 *         - Key value from the keypad matrix if pressed
 *         - NOTPRESSED (0xFF) if no key is pressed
 * @note  This function includes debouncing and waits for key release
 * @example
 * uint8_t key = KPD_u8GetPressed();
 * if (key != NOTPRESSED) { ... }
 */
uint8_t KPD_u8GetPressed(void);

#endif /* KPD_INTERFACE_H_ */
