/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    SYSCFG_prog.c     >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : SYSCFG                                          **
 **                                                                           **
 **===========================================================================**
 */

#include <stdint.h>
#include "STM32F446xx.h"

#include "STD_MACROS.h"
#include "ErrTypes.h"

#include "../Inc/SYSCFG_private.h"
#include "../Inc/SYSCFG_config.h"
#include "../Inc/SYSCFG_interface.h"

/**
 * @SYSCFG_vSetEXTIConfig function:
 * @brief: Configure EXTI line mapping to GPIO port
 * @details: Maps a specific EXTI line to a GPIO port in the SYSCFG peripheral
 * @param: Copy_u8EXTI - EXTI line number (0-15)
 *         Copy_u8Port - GPIO port identifier (A-H)
 * @return: void
 * @note:
 *   1. Each EXTI line is mapped to a 4-bit field in one of four EXTICR registers
 *   2. The mapping is done in groups of 4 EXTI lines per register
 *   3. Must be called before configuring EXTI line
 * @example:
 *   SYSCFG_vSetEXTIConfig(SYSCFG_EXTI0, SYSCFG_PORTA); // Map EXTI0 to Port A
 * @internal:
 *   - Local_u8RegNum: Calculates which EXTICR register to use (0-3)
 *   - Local_u8BitNum: Calculates the bit position within the register (0-12)
 *   - 0x0F mask: Clears the current port mapping (4 bits)
 */
void SYSCFG_vSetEXTIConfig(SYSCFG_EXTI_t Copy_u8EXTI, SYSCFG_Port_t Copy_u8Port)
{
  /* Calculate register number (0-3) based on EXTI line number */
  uint8_t Local_u8RegNum = Copy_u8EXTI / EXTI_CTRL_REG_LINEBITS;

  /* Calculate bit position within register (0-12) */
  uint8_t Local_u8BitNum = (Copy_u8EXTI % EXTI_CTRL_REG_LINEBITS) * EXTI_CTRL_REG_LINEBITS;

  /* Clear current port mapping for this EXTI line */
  MSYSCFG->EXTICR[Local_u8RegNum] &= ~(EXTI_CTRL_REG_MASK << Local_u8BitNum);

  /* Set new port mapping for this EXTI line */
  MSYSCFG->EXTICR[Local_u8RegNum] |= (Copy_u8Port << Local_u8BitNum);
}
