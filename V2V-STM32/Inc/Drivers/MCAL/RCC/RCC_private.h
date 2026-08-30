/**
 ******************************************************************************
 * @file    RCC_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the RCC driver — not a public header.
 * @ingroup mcal_rcc
 ******************************************************************************
 */

#ifndef MCAL_RCC_PRIVATE_H_
#define MCAL_RCC_PRIVATE_H_

#define RCC_MPLL_DIV_MASK  0X3F    /**< Mask of the PLL M (input divider) field in `PLLCFGR`. */
#define RCC_NPLL_MULT_MASK 0X1FF   /**< Mask of the PLL N (VCO multiplier) field in `PLLCFGR`. */
#define RCC_PPLL_DIV_MASK  0X3     /**< Mask of the PLL P (output divider) field in `PLLCFGR`. */
#define RCC_SYS_CLK_MASK   0X3     /**< Mask of the system-clock-switch field in `CFGR`. */
#define RCC_u32TIMEOUT     10000UL /**< Busy-wait bound, in loop iterations, for a clock's "ready" flag. Prevents an absent crystal from hanging the boot forever. */
#endif                             /* MCAL_RCC_PRIVATE_H_ */
