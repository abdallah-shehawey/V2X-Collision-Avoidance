/**
 ******************************************************************************
 * @file    IWDG_program.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Implementation of the IWDG driver — the independent watchdog.
 * @ingroup mcal_iwdg
 ******************************************************************************
 */

#include <stdint.h>
#include "../Inc/Drivers/MCAL/IWDG/IWDG_interface.h"

/**
 * @brief IWDG register block — APB1 @ 0x40003000, clocked by the LSI (~32 kHz).
 *
 * The IWDG is deliberately hard to disarm: its registers are write-protected, and
 * a magic key must be written to @ref IWDG_RegDef_t::KR to unlock them. Once the
 * watchdog has been started it cannot be stopped at all — only reset before it
 * expires. That is the whole point: a runaway program must not be able to switch
 * off its own safety net.
 *
 * @note Its clock is the LSI, an RC oscillator accurate to roughly ±50%, so the
 *       real timeout can be substantially shorter or longer than the nominal one.
 *       Timeouts here are chosen with generous margin rather than to the millisecond.
 */
typedef struct
{
  volatile uint32_t KR;   /**< Key register (0x00): the magic values that unlock the registers, reload the counter, or start the watchdog. */
  volatile uint32_t PR;   /**< Prescaler (0x04): divides the ~32 kHz LSI down to the counter's tick rate. */
  volatile uint32_t RLR;  /**< Reload value (0x08): what the down-counter restarts from on each kick. Together with PR this sets the timeout. */
  volatile uint32_t SR;   /**< Status (0x0C): "an update to PR/RLR is still in progress". Both must read back clear before a new write is accepted. */
  volatile uint32_t WINR; /**< Window (0x10): if set, kicking *too early* also resets the MCU. Unused here. */
} IWDG_RegDef_t;

/** @brief Typed pointer to the IWDG peripheral. */
#define MIWDG ((IWDG_RegDef_t *)0x40003000UL)

/**
 * @name Key-register commands
 * The magic values written to `KR`. Anything else written there is ignored — the
 * watchdog is deliberately awkward to touch by accident.
 * @{
 */
#define IWDG_KEY_ENABLE  0x5555u /**< Unlock write access to `PR` and `RLR`. */
#define IWDG_KEY_REFRESH 0xAAAAu /**< Reload the down-counter — this is the "kick". */
#define IWDG_KEY_START   0xCCCCu /**< Start the watchdog. Also switches the LSI on. Cannot be undone short of a reset. */
/** @} */

/**
 * @name Timing
 * With the prescaler at /64 the counter runs at about 32 kHz / 64 = 500 Hz, so one
 * reload count is roughly 2 ms. The reload register is 12 bits, which caps the
 * timeout at about 8.19 s.
 * @{
 */
#define IWDG_PR_DIV64 0x4u    /**< Prescaler value selecting a /64 division of the LSI. */
#define IWDG_TICK_MS  2u      /**< Milliseconds per reload count, at the /64 prescaler. */
#define IWDG_RLR_MAX  0x0FFFu /**< Largest reload value the 12-bit register holds. */
/** @} */

/**
 * @name Debug freeze
 * Without this, the watchdog keeps counting while the core is halted at a
 * breakpoint, so the first time you stop to inspect anything the MCU resets under
 * you. Setting @ref DBG_IWDG_STOP freezes the counter whenever the debugger halts
 * the core.
 * @{
 */
#define DBGMCU_APB1_FZ (*(volatile uint32_t *)0xE0042008UL) /**< DBGMCU APB1 freeze register. */
#define DBG_IWDG_STOP  (1u << 12)                           /**< Freeze the IWDG counter while the core is halted. */
/** @} */

void IWDG_voidInit(uint16_t timeout_ms)
{
  uint32_t reload = (uint32_t)timeout_ms / IWDG_TICK_MS;
  if (reload == 0u)
    reload = 1u;
  if (reload > IWDG_RLR_MAX)
    reload = IWDG_RLR_MAX;

  /* Keep the dog asleep while a debugger has the core halted. */
  DBGMCU_APB1_FZ |= DBG_IWDG_STOP;

  MIWDG->KR = IWDG_KEY_ENABLE;  /* unlock PR/RLR                 */
  MIWDG->PR = IWDG_PR_DIV64;    /* /64                          */
  MIWDG->RLR = reload;          /* timeout                      */
  MIWDG->KR = IWDG_KEY_REFRESH; /* load RLR into the counter    */
  MIWDG->KR = IWDG_KEY_START;   /* start (LSI turns on in HW)   */
}

void IWDG_voidRefresh(void)
{
  MIWDG->KR = IWDG_KEY_REFRESH;
}
