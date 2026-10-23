/**
 ******************************************************************************
 * @file    SYSCFG_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the SYSCFG driver — not a public header.
 * @ingroup mcal_syscfg
 ******************************************************************************
 */

#ifndef SYSCFG_PRIVATE_H_
#define SYSCFG_PRIVATE_H_

#define EXTI_CTRL_REG_LINEBITS 4U /**< Bits per EXTI line in `EXTICR` — so four lines fit in each of the four words. */

#define EXTI_CTRL_REG_MASK 0x0F /**< Mask of one line's 4-bit port-selection field in `EXTICR`. */

#endif /* SYSCFG_PRIVATE_H_ */
