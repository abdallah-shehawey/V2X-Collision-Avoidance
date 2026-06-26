/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<     IWDG_program.c     >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Saleh                                  **
 **                  Layer  : MCAL                                            **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : IWDG (Independent Watchdog)                     **
 **                                                                           **
 **===========================================================================**
 */

#include <stdint.h>
#include "../Inc/Drivers/MCAL/IWDG/IWDG_interface.h"

/* IWDG register block — APB1 @ 0x40003000, clocked by the LSI (~32 kHz). */
typedef struct
{
    volatile uint32_t KR;    /* Key register      (0x00) */
    volatile uint32_t PR;    /* Prescaler register(0x04) */
    volatile uint32_t RLR;   /* Reload register   (0x08) */
    volatile uint32_t SR;    /* Status register   (0x0C) */
    volatile uint32_t WINR;  /* Window register   (0x10) */
} IWDG_RegDef_t;

#define MIWDG  ((IWDG_RegDef_t *)0x40003000UL)

/* Key-register commands */
#define IWDG_KEY_ENABLE    0x5555u   /* unlock write access to PR/RLR */
#define IWDG_KEY_REFRESH   0xAAAAu   /* reload the down-counter       */
#define IWDG_KEY_START     0xCCCCu   /* start IWDG (auto-enables LSI) */

/* Prescaler /64 → counter clock ≈ 32 kHz / 64 = 500 Hz → one RLR count ≈ 2 ms.
 * 12-bit reload (max 4095) ⇒ max timeout ≈ 8.19 s. */
#define IWDG_PR_DIV64      0x4u
#define IWDG_TICK_MS       2u
#define IWDG_RLR_MAX       0x0FFFu

/* DBGMCU APB1 freeze: stop the IWDG counter while the core is halted in debug,
 * so breakpoints / single-stepping don't fire spurious watchdog resets. */
#define DBGMCU_APB1_FZ     (*(volatile uint32_t *)0xE0042008UL)
#define DBG_IWDG_STOP      (1u << 12)

void IWDG_voidInit(uint16_t timeout_ms)
{
    uint32_t reload = (uint32_t)timeout_ms / IWDG_TICK_MS;
    if (reload == 0u)            reload = 1u;
    if (reload > IWDG_RLR_MAX)   reload = IWDG_RLR_MAX;

    /* Keep the dog asleep while a debugger has the core halted. */
    DBGMCU_APB1_FZ |= DBG_IWDG_STOP;

    MIWDG->KR  = IWDG_KEY_ENABLE;    /* unlock PR/RLR                 */
    MIWDG->PR  = IWDG_PR_DIV64;      /* /64                          */
    MIWDG->RLR = reload;             /* timeout                      */
    MIWDG->KR  = IWDG_KEY_REFRESH;   /* load RLR into the counter    */
    MIWDG->KR  = IWDG_KEY_START;     /* start (LSI turns on in HW)   */
}

void IWDG_voidRefresh(void)
{
    MIWDG->KR = IWDG_KEY_REFRESH;
}
