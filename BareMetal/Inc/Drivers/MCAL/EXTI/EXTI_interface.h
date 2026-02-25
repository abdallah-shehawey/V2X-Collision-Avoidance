/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<   EXTI_interface.h   >>>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : EXTI                                            **
 **                                                                           **
 **===========================================================================**
 */
#ifndef MCAL_EXTI_INTERFACE_H_
#define MCAL_EXTI_INTERFACE_H_

#include <stdint.h>
#include "../../LIB/ErrTypes.h"

/**
 * @EXTI_TriggerSrc_t enum:
 * @brief: External interrupt trigger source configuration
 * @details: Defines possible trigger configurations for EXTI lines
 * @param: EXTI_NO_TRIGGER - No interrupt trigger
 *         EXTI_RISING_EDGE - Trigger on rising edge
 *         EXTI_FALLING_EDGE - Trigger on falling edge
 *         EXTI_RISING_FALLING_EDGE - Trigger on both edges
 * @return: Selected trigger configuration
 */
typedef enum
{
  EXTI_NO_TRIGGER,
  EXTI_RISING_EDGE,
  EXTI_FALLING_EDGE,
  EXTI_RISING_FALLING_EDGE
} EXTI_TriggerSrc_t;

/**
 * @EXTI_Enable_t enum:
 * @brief: External interrupt enable/disable configuration
 * @details: Controls the enable/disable state of EXTI lines
 * @param: EXTI_DIS - Disable EXTI line
 *         EXTI_EN - Enable EXTI line
 * @return: Enable/disable state of EXTI line
 */
typedef enum
{
  EXTI_DIS,
  EXTI_EN
} EXTI_Enable_t;

/**
 * @EXTI_Line_t enum:
 * @brief: External interrupt line identifiers
 * @details: Defines all possible EXTI lines (0-15)
 * @param: EXTI_LINE0-EXTI_LINE15 - EXTI line numbers
 * @return: Selected EXTI line number
 */
typedef enum
{
  EXTI_LINE0 = 0,
  EXTI_LINE1,
  EXTI_LINE2,
  EXTI_LINE3,
  EXTI_LINE4,
  EXTI_LINE5,
  EXTI_LINE6,
  EXTI_LINE7,
  EXTI_LINE8,
  EXTI_LINE9,
  EXTI_LINE10,
  EXTI_LINE11,
  EXTI_LINE12,
  EXTI_LINE13,
  EXTI_LINE14,
  EXTI_LINE15
} EXTI_Line_t;

/**
 * @EXTI_LineConfig_t struct:
 * @brief: Configuration structure for EXTI line
 * @details: Holds all configuration parameters for an EXTI line
 * @param: Line - EXTI line number (0-15)
 *         TrigSrc - Trigger source configuration
 *         Enable - Enable/disable state
 *         Copy_pvCallBack - Pointer to callback function
 * @return: Configuration structure for EXTI line
 */
typedef struct
{
  EXTI_Line_t Line;
  EXTI_TriggerSrc_t TrigSrc;
  EXTI_Enable_t Enable;
  void (*Copy_pvCallBack)(void);
} EXTI_LineConfig_t;

/**
 * @EXTI_vLineInit function:
 * @brief: Initialize EXTI line with configuration
 * @details: Configures EXTI line based on provided configuration structure
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 * @note: Must be called after SYSCFG configuration
 * @example
 * EXTI_LineConfig_t cfg = {EXTI_LINE0, EXTI_RISING_EDGE, EXTI_EN, MyCallback};
 * EXTI_vLineInit(&cfg);
 */
ErrorState_t EXTI_vLineInit(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @EXTI_vEnableInterrupt function:
 * @brief: Enable EXTI interrupt
 * @details: Enables the specified EXTI line interrupt
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 * @example EXTI_vEnableInterrupt(&cfg);
 */
ErrorState_t EXTI_vEnableInterrupt(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @EXTI_vSetTrigSrc function:
 * @brief: Set EXTI trigger source
 * @details: Configures how the EXTI line is triggered
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 *         Copy_u8Trigger - New trigger source configuration
 * @return: ErrorState_t - Error state of the operation
 * @example EXTI_vSetTrigSrc(&cfg, EXTI_FALLING_EDGE);
 */
ErrorState_t EXTI_vSetTrigSrc(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig, EXTI_TriggerSrc_t Copy_u8Trigger);

/**
 * @EXTI_vSetPendFlag function:
 * @brief: Set EXTI pending flag
 * @details: Sets the pending flag for the specified EXTI line
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 * @example EXTI_vSetPendFlag(&cfg);
 */
ErrorState_t EXTI_vSetPendFlag(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @EXTI_vDisableInterrupt function:
 * @brief: Disable EXTI interrupt
 * @details: Disables the specified EXTI line interrupt
 * @param: Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return: ErrorState_t - Error state of the operation
 * @example EXTI_vDisableInterrupt(&cfg);
 */
ErrorState_t EXTI_vDisableInterrupt(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @EXTI_vClearPendFlag function:
 * @brief: Clear EXTI pending flag
 * @details: Clears the pending flag for the specified EXTI line
 * @param: Copy_u8Line - EXTI line number
 * @return: void
 * @example EXTI_vClearPendFlag(EXTI_LINE0);
 */
void EXTI_vClearPendFlag(EXTI_Line_t Copy_u8Line);

/**
 * @EXTI_u8ReadPendFlag function:
 * @brief: Read EXTI pending flag status
 * @details: Reads the status of the pending flag for specified EXTI line
 * @param: Copy_u8Line - EXTI line number
 * @return: uint8_t - Status of the pending flag (0: not pending, 1: pending)
 * @example
 * uint8_t status = EXTI_u8ReadPendFlag(EXTI_LINE0);
 */
uint8_t EXTI_u8ReadPendFlag(EXTI_Line_t Copy_u8Line);

#endif /* MCAL_EXTI_INTERFACE_H_ */
