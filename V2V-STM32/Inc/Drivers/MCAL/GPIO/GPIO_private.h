/**
 ******************************************************************************
 * @file    GPIO_private.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Register bit positions and internals of the GPIO driver — not a public header.
 * @ingroup mcal_gpio
 ******************************************************************************
 */

#ifndef GPIO_PRIVATE_H_
#define GPIO_PRIVATE_H_

/**
 * @brief Silence an "unused parameter" warning without changing behaviour.
 * @param X The parameter to mark as deliberately unused.
 */
#define UNUSED(X) (void)X

/************************** GPIO PORT COUNT **************************/
#define GPIO_PORT_COUNT 8u                       /**< How many GPIO ports this MCU has (A..H). */
/************************** GPIO MODER MASK **************************/
#define MODER_MASK      0b11                     /**< Mask of one pin's 2-bit field in `MODER`. */
#define MODER_PIN_ACCESS 2u                      /**< Bits per pin in `MODER` — the shift multiplier for pin N. */
/************************** GPIO OTYPER MASK **************************/
#define OTYPER_MASK     0b1                      /**< Mask of one pin's 1-bit field in `OTYPER`. */
/************************** GPIO OSPEEDR MASK **************************/
#define OSPEEDR_MASK    0b11                   /**< Mask of one pin's 2-bit field in `OSPEEDR`. */
#define OSPEEDR_PIN_ACCESS 2u                  /**< Bits per pin in `OSPEEDR`. */
/************************** GPIO PUPDR MASK **************************/
#define PUPDR_MASK      0b11                   /**< Mask of one pin's 2-bit field in `PUPDR`. */
#define PUPDR_PIN_ACCESS   2u                  /**< Bits per pin in `PUPDR`. */
/* 4 bits for Alternate Function Configuration */
#define AFR_MASK          0xF                  /**< Mask of one pin's 4-bit field in `AFR`. */
#define AFR_PIN_ACCESS    4u                   /**< Bits per pin in `AFR`. */
#define AFR_PIN_SHIFT     8u                   /**< Pins covered by `AFR[0]`; pin 8 and above live in `AFR[1]`. */
/* Number of pins per port */
#define GPIO_PIN_COUNT 16u                     /**< Pins per GPIO port. */
#endif /* GPIO_PRIVATE_H_ */
