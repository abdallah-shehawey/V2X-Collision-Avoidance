/**
 ******************************************************************************
 * @file    SYSCFG_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the SYSCFG driver — the system configuration controller.
 * @ingroup mcal_syscfg
 ******************************************************************************
 */

#ifndef SYSCFG_INTERFACE_H_
#define SYSCFG_INTERFACE_H_

#include "stdint.h"
#include "../../LIB/ErrTypes.h"

/**
 * @brief System configuration for EXTI lines
 * @details Defines EXTI line numbers for SYSCFG configuration
 * - SYSCFG_EXTI0-SYSCFG_EXTI15 - EXTI line numbers (0-15)
 */
typedef enum
{
  SYSCFG_EXTI0,  /**< Route EXTI line 0 to pin 0 of the chosen port. */
  SYSCFG_EXTI1,  /**< Route EXTI line 1 to pin 1 of the chosen port. */
  SYSCFG_EXTI2,  /**< Route EXTI line 2 to pin 2 of the chosen port. */
  SYSCFG_EXTI3,  /**< Route EXTI line 3 to pin 3 of the chosen port. */
  SYSCFG_EXTI4,  /**< Route EXTI line 4 to pin 4 of the chosen port. */
  SYSCFG_EXTI5,  /**< Route EXTI line 5 to pin 5 of the chosen port. */
  SYSCFG_EXTI6,  /**< Route EXTI line 6 to pin 6 of the chosen port. */
  SYSCFG_EXTI7,  /**< Route EXTI line 7 to pin 7 of the chosen port. */
  SYSCFG_EXTI8,  /**< Route EXTI line 8 to pin 8 of the chosen port. */
  SYSCFG_EXTI9,  /**< Route EXTI line 9 to pin 9 of the chosen port. */
  SYSCFG_EXTI10, /**< Route EXTI line 10 to pin 10 of the chosen port. */
  SYSCFG_EXTI11, /**< Route EXTI line 11 to pin 11 of the chosen port. */
  SYSCFG_EXTI12, /**< Route EXTI line 12 to pin 12 of the chosen port. */
  SYSCFG_EXTI13, /**< Route EXTI line 13 to pin 13 of the chosen port. */
  SYSCFG_EXTI14, /**< Route EXTI line 14 to pin 14 of the chosen port. */
  SYSCFG_EXTI15, /**< Route EXTI line 15 to pin 15 of the chosen port. */
} SYSCFG_EXTI_t;

/**
 * @brief System configuration for GPIO ports
 * @details Defines available GPIO ports for SYSCFG configuration
 * - SYSCFG_PORTA-SYSCFG_PORTH - GPIO port identifiers (A-H)
 */
typedef enum
{
  SYSCFG_PORTA = 0, /**< GPIO Port A */
  SYSCFG_PORTB,     /**< GPIO Port B */
  SYSCFG_PORTC,     /**< GPIO Port C */
  SYSCFG_PORTD,     /**< GPIO Port D */
  SYSCFG_PORTE,     /**< GPIO Port E */
  SYSCFG_PORTF,     /**< GPIO Port F */
  SYSCFG_PORTG,     /**< GPIO Port G */
  SYSCFG_PORTH      /**< GPIO Port H */
} SYSCFG_Port_t;

/**
 * @brief Choose which GPIO port drives a given EXTI line.
 *
 * EXTI line @e N is shared by pin @e N of every port — PA3, PB3 and PC3 all map to
 * EXTI3 — and only one of them can own the line. This is where that choice is made.
 *
 * @param Copy_u8EXTI EXTI line to route, 0..15.
 * @param Copy_u8Port The port whose pin should drive it.
 *
 * @note Without this call an EXTI line defaults to port A, so an interrupt
 *       configured on, say, PB4 is simply never delivered. It has to run **before**
 *       @ref EXTI_vLineInit.
 *
 * @code
 * SYSCFG_vSetEXTIConfig(SYSCFG_EXTI0, SYSCFG_PORTA);
 * @endcode
 */
void SYSCFG_vSetEXTIConfig(SYSCFG_EXTI_t Copy_u8EXTI, SYSCFG_Port_t Copy_u8Port);

#endif /* SYSCFG_INTERFACE_H_ */
