/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<    SYSCFG_interface.h    >>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SYSCFG                                          **
 **                                                                           **
 **===========================================================================**
 */

#ifndef SYSCFG_INTERFACE_H_
#define SYSCFG_INTERFACE_H_

#include "stdint.h"
#include "ErrTypes.h"

/**
 * @SYSCFG_EXTI_t enum:
 * @brief: System configuration for EXTI lines
 * @details: Defines EXTI line numbers for SYSCFG configuration
 * @param: SYSCFG_EXTI0-SYSCFG_EXTI15 - EXTI line numbers (0-15)
 * @return: Selected EXTI line number
 */
typedef enum
{
  SYSCFG_EXTI0,
  SYSCFG_EXTI1,
  SYSCFG_EXTI2,
  SYSCFG_EXTI3,
  SYSCFG_EXTI4,
  SYSCFG_EXTI5,
  SYSCFG_EXTI6,
  SYSCFG_EXTI7,
  SYSCFG_EXTI8,
  SYSCFG_EXTI9,
  SYSCFG_EXTI10,
  SYSCFG_EXTI11,
  SYSCFG_EXTI12,
  SYSCFG_EXTI13,
  SYSCFG_EXTI14,
  SYSCFG_EXTI15,
} SYSCFG_EXTI_t;

/**
 * @SYSCFG_Port_t enum:
 * @brief: System configuration for GPIO ports
 * @details: Defines available GPIO ports for SYSCFG configuration
 * @param: SYSCFG_PORTA-SYSCFG_PORTH - GPIO port identifiers (A-H)
 * @return: Selected GPIO port
 */
typedef enum
{
  SYSCFG_PORTA = 0, /* GPIO Port A */
  SYSCFG_PORTB,     /* GPIO Port B */
  SYSCFG_PORTC,     /* GPIO Port C */
  SYSCFG_PORTD,     /* GPIO Port D */
  SYSCFG_PORTE,     /* GPIO Port E */
  SYSCFG_PORTF,     /* GPIO Port F */
  SYSCFG_PORTG,     /* GPIO Port G */
  SYSCFG_PORTH      /* GPIO Port H */
} SYSCFG_Port_t;

/**
 * @SYSCFG_vSetEXTIConfig function:
 * @brief: Configure EXTI line mapping to GPIO port
 * @details: Maps a specific EXTI line to a GPIO port in the SYSCFG peripheral
 * @param: Copy_u8EXTI - EXTI line number (0-15)
 *         Copy_u8Port - GPIO port identifier (A-H)
 * @return: void
 * @note: Must be called before configuring EXTI line
 * @example SYSCFG_vSetEXTIConfig(SYSCFG_EXTI0, SYSCFG_PORTA);
 */
void SYSCFG_vSetEXTIConfig(SYSCFG_EXTI_t Copy_u8EXTI, SYSCFG_Port_t Copy_u8Port);

#endif /* SYSCFG_INTERFACE_H_ */