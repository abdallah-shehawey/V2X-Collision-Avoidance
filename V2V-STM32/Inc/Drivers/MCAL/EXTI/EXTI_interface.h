/**
 ******************************************************************************
 * @file    EXTI_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the EXTI driver — the external interrupt controller.
 * @ingroup mcal_exti
 ******************************************************************************
 */
#ifndef MCAL_EXTI_INTERFACE_H_
#define MCAL_EXTI_INTERFACE_H_

#include <stdint.h>
#include "../../LIB/ErrTypes.h"

/**
 * @brief External interrupt trigger source configuration
 * @details Defines possible trigger configurations for EXTI lines
 * - EXTI_NO_TRIGGER - No interrupt trigger
 *         EXTI_RISING_EDGE - Trigger on rising edge
 *         EXTI_FALLING_EDGE - Trigger on falling edge
 *         EXTI_RISING_FALLING_EDGE - Trigger on both edges
 */
typedef enum
{
  EXTI_NO_TRIGGER,                /**< Line configured but never fires. */
  EXTI_RISING_EDGE,               /**< Fire on a low-to-high transition. */
  EXTI_FALLING_EDGE,              /**< Fire on a high-to-low transition. */
  EXTI_RISING_FALLING_EDGE        /**< Fire on **both** edges — what each ultrasonic echo pin uses, so one interrupt starts the timing and the next ends it. */
} EXTI_TriggerSrc_t;

/**
 * @brief External interrupt enable/disable configuration
 * @details Controls the enable/disable state of EXTI lines
 * - EXTI_DIS - Disable EXTI line
 *         EXTI_EN - Enable EXTI line
 */
typedef enum
{
  EXTI_DIS,                       /**< Line masked: it will not raise an interrupt. */
  EXTI_EN                         /**< Line unmasked. */
} EXTI_Enable_t;

/**
 * @brief External interrupt line identifiers
 * @details Defines all possible EXTI lines (0-15)
 * - EXTI_LINE0-EXTI_LINE15 - EXTI line numbers
 */
typedef enum
{
  EXTI_LINE0 = 0,                 /**< EXTI line 0 — pin 0 of whichever port SYSCFG has routed to it. */
  EXTI_LINE1,                     /**< EXTI line 1 — pin 1 of whichever port SYSCFG has routed to it. */
  EXTI_LINE2,                     /**< EXTI line 2 — pin 2 of whichever port SYSCFG has routed to it. */
  EXTI_LINE3,                     /**< EXTI line 3 — pin 3 of whichever port SYSCFG has routed to it. */
  EXTI_LINE4,                     /**< EXTI line 4 — pin 4 of whichever port SYSCFG has routed to it. */
  EXTI_LINE5,                     /**< EXTI line 5 — pin 5 of whichever port SYSCFG has routed to it. */
  EXTI_LINE6,                     /**< EXTI line 6 — pin 6 of whichever port SYSCFG has routed to it. */
  EXTI_LINE7,                     /**< EXTI line 7 — pin 7 of whichever port SYSCFG has routed to it. */
  EXTI_LINE8,                     /**< EXTI line 8 — pin 8 of whichever port SYSCFG has routed to it. */
  EXTI_LINE9,                     /**< EXTI line 9 — pin 9 of whichever port SYSCFG has routed to it. */
  EXTI_LINE10,                    /**< EXTI line 10 — pin 10 of whichever port SYSCFG has routed to it. */
  EXTI_LINE11,                    /**< EXTI line 11 — pin 11 of whichever port SYSCFG has routed to it. */
  EXTI_LINE12,                    /**< EXTI line 12 — pin 12 of whichever port SYSCFG has routed to it. */
  EXTI_LINE13,                    /**< EXTI line 13 — pin 13 of whichever port SYSCFG has routed to it. */
  EXTI_LINE14,                    /**< EXTI line 14 — pin 14 of whichever port SYSCFG has routed to it. */
  EXTI_LINE15                     /**< EXTI line 15 — pin 15 of whichever port SYSCFG has routed to it. */
} EXTI_Line_t;

/**
 * @brief Configuration structure for EXTI line
 * @details Holds all configuration parameters for an EXTI line
 * One EXTI line, its trigger edge, and what to run when it fires. The six
 * ultrasonic echo pins each own a line configured for **both** edges: the rising
 * edge starts the echo timing, the falling edge stops it, and the difference is
 * the distance.
 *
 * @note EXTI line *N* is shared by pin *N* of every port — PA3, PB3 and PC3 all
 *       map to EXTI3, and only one of them can own it. Which port actually drives
 *       the line is chosen in SYSCFG, not here.
 */
typedef struct
{
  EXTI_Line_t Line;              /**< Which EXTI line, 0..15. */
  EXTI_TriggerSrc_t TrigSrc;     /**< Which edge fires it: rising, falling, or both. */
  EXTI_Enable_t Enable;          /**< Whether the line is unmasked at init. */
  void (*Copy_pvCallBack)(void); /**< Called from the EXTI ISR when the line fires. Must be ISR-safe: short, non-blocking, `...FromISR` API only. */
} EXTI_LineConfig_t;

/**
 * @brief Initialize EXTI line with configuration
 * @details Configures EXTI line based on provided configuration structure
 * @param Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return ErrorState_t - Error state of the operation
 * @note Must be called after SYSCFG configuration
 * @par Example:
 * 
 * EXTI_LineConfig_t cfg = {EXTI_LINE0, EXTI_RISING_EDGE, EXTI_EN, MyCallback};
 * EXTI_vLineInit(&cfg);
 */
ErrorState_t EXTI_vLineInit(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @brief Enable EXTI interrupt
 * @details Enables the specified EXTI line interrupt
 * @param Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return ErrorState_t - Error state of the operation
 * @par Example:
 * EXTI_vEnableInterrupt(&cfg);
 */
ErrorState_t EXTI_vEnableInterrupt(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @brief Change which edge an already-configured EXTI line triggers on.
 * @param[in] Copy_pEXTI_LineConfig The line to reconfigure.
 * @param     Copy_u8Trigger        The new trigger edge.
 * @return OK if the line was reconfigured, NULL_POINTER if the pointer was NULL.
 *
 * @code
 * EXTI_vSetTrigSrc(&cfg, EXTI_FALLING_EDGE);
 * @endcode
 */
ErrorState_t EXTI_vSetTrigSrc(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig, EXTI_TriggerSrc_t Copy_u8Trigger);

/**
 * @brief Set EXTI pending flag
 * @details Sets the pending flag for the specified EXTI line
 * @param Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return ErrorState_t - Error state of the operation
 * @par Example:
 * EXTI_vSetPendFlag(&cfg);
 */
ErrorState_t EXTI_vSetPendFlag(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @brief Disable EXTI interrupt
 * @details Disables the specified EXTI line interrupt
 * @param Copy_pEXTI_LineConfig - Pointer to EXTI configuration structure
 * @return ErrorState_t - Error state of the operation
 * @par Example:
 * EXTI_vDisableInterrupt(&cfg);
 */
ErrorState_t EXTI_vDisableInterrupt(const EXTI_LineConfig_t *Copy_pEXTI_LineConfig);

/**
 * @brief Clear a line's pending flag.
 * @param Copy_u8Line The EXTI line to clear.
 *
 * @note The flag is cleared by writing a **1** to it, not a 0 — a quirk of the
 *       hardware. Every EXTI handler must do this, because a pending flag left set
 *       re-enters the ISR the instant it returns and locks the CPU in an interrupt
 *       storm.
 *
 * @code
 * EXTI_vClearPendFlag(EXTI_LINE0);
 * @endcode
 */
void EXTI_vClearPendFlag(EXTI_Line_t Copy_u8Line);

/**
 * @brief Read EXTI pending flag status
 * @details Reads the status of the pending flag for specified EXTI line
 * @param Copy_u8Line - EXTI line number
 * @return uint8_t - Status of the pending flag (0: not pending, 1: pending)
 * @par Example:
 * 
 * uint8_t status = EXTI_u8ReadPendFlag(EXTI_LINE0);
 */
uint8_t EXTI_u8ReadPendFlag(EXTI_Line_t Copy_u8Line);

#endif /* MCAL_EXTI_INTERFACE_H_ */
