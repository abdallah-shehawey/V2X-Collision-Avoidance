/**
 ******************************************************************************
 * @file    IWDG_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the IWDG driver — the independent watchdog.
 * @ingroup mcal_iwdg
 ******************************************************************************
 */

#ifndef IWDG_INTERFACE_H_
#define IWDG_INTERFACE_H_

#include <stdint.h>

/**
 * @brief  Start the Independent Watchdog with the given timeout.
 * @param  timeout_ms  Reset timeout in milliseconds (≈ 4 .. 8000 ms).
 *
 * @note   - Clocked by the LSI (~32 kHz, imprecise → keep a healthy margin).
 *         - Once started the IWDG CANNOT be stopped; it MUST be refreshed via
 *           IWDG_voidRefresh() more often than timeout_ms or the MCU resets.
 *         - The IWDG is frozen while the core is halted by the debugger
 *           (so breakpoints don't trigger spurious resets).
 */
void IWDG_voidInit(uint16_t timeout_ms);

/**
 * @brief  Reload the watchdog counter ("kick the dog"). Call periodically.
 */
void IWDG_voidRefresh(void);

#endif /* IWDG_INTERFACE_H_ */
