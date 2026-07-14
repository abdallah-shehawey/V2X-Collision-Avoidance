/**
 ******************************************************************************
 * @file    GPIO_interface.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Public API of the GPIO driver: pin/port configuration and I/O.
 * @ingroup mcal_gpio
 *
 * @details
 * The driver works at three granularities, all backed by the same registers:
 *
 * - **single pin** — @ref GPIO_enumPinInit, @ref GPIO_enumWritePinVal,
 *   @ref GPIO_enumReadPinVal, @ref GPIO_enumTogPinVal. This is what the LED,
 *   buzzer and ultrasonic drivers use.
 * - **a slice of a port** — a nibble (4 pins), a byte (8 pins), a half-port or
 *   a whole port, configured in one call so a parallel bus does not need
 *   sixteen separate init calls.
 * - **raw port access** — @ref GPIO_enumReadPortVal and friends.
 *
 * Every entry point validates its arguments and reports the outcome through
 * @ref ErrorState_t; none of them can fault on a bad port or pin index.
 *
 * @note The port's clock must already be enabled in RCC (`RCC_enumAHB1EnableCLK`)
 *       before any function here has an effect: a GPIO port with its clock gated
 *       off ignores writes and reads back as zero.
 ******************************************************************************
 */

#ifndef GPIO_INTERFACE_H_
#define GPIO_INTERFACE_H_

#include "../../LIB/ErrTypes.h"
#include <stdint.h>

/**
 * @addtogroup mcal_gpio
 * @{
 */

/*============================================================================*/
/*                                   TYPES                                    */
/*============================================================================*/

/** @brief GPIO port selector. The value is the index into the driver's port table. */
typedef enum {
  GPIO_PORTA = 0, /**< Port A. */
  GPIO_PORTB,     /**< Port B. */
  GPIO_PORTC,     /**< Port C. */
  GPIO_PORTD,     /**< Port D. */
  GPIO_PORTE,     /**< Port E. */
  GPIO_PORTF,     /**< Port F. */
  GPIO_PORTG,     /**< Port G. */
  GPIO_PORTH      /**< Port H. */
} GPIO_Port_t;

/** @brief Pin selector within a port. The value is the bit position in the port registers. */
typedef enum {
  GPIO_PIN0 = 0, /**< Pin 0. */
  GPIO_PIN1,     /**< Pin 1. */
  GPIO_PIN2,     /**< Pin 2. */
  GPIO_PIN3,     /**< Pin 3. */
  GPIO_PIN4,     /**< Pin 4. */
  GPIO_PIN5,     /**< Pin 5. */
  GPIO_PIN6,     /**< Pin 6. */
  GPIO_PIN7,     /**< Pin 7. */
  GPIO_PIN8,     /**< Pin 8. */
  GPIO_PIN9,     /**< Pin 9. */
  GPIO_PIN10,    /**< Pin 10. */
  GPIO_PIN11,    /**< Pin 11. */
  GPIO_PIN12,    /**< Pin 12. */
  GPIO_PIN13,    /**< Pin 13. */
  GPIO_PIN14,    /**< Pin 14. */
  GPIO_PIN15     /**< Pin 15. */
} GPIO_Pin_t;

/** @brief Pin mode — the 2-bit field written into `MODER`. */
typedef enum {
  GPIO_INPUT = 0, /**< Digital input; the pin is read through `IDR`. */
  GPIO_OUTPUT,    /**< Digital output; the pin is driven from `ODR`/`BSRR`. */
  GPIO_ALTFN,     /**< Alternate function; a peripheral (USART, SPI, TIM) owns the pin. */
  GPIO_ANALOG     /**< Analog; the digital input buffer is disconnected. */
} GPIO_Mode_t;

/** @brief Output driver type — the 1-bit field written into `OTYPER`. */
typedef enum {
  GPIO_PUSH_PULL = 0, /**< Push-pull: the pin actively drives both high and low. */
  GPIO_OPEN_DRAIN     /**< Open-drain: the pin only pulls low, and floats otherwise. */
} GPIO_OutputType_t;

/** @brief Which half of a byte a nibble operation applies to. */
typedef enum {
  GPIO_LOW_NIBBLE, /**< Pins 0..3. */
  GPIO_HIGH_NIBBLE /**< Pins 4..7. */
} GPIO_NibbleType_t;

/**
 * @brief Output slew rate — the 2-bit field written into `OSPEEDR`.
 * @note  A faster edge means more EMI. Use the slowest speed the signal allows;
 *        the LEDs and the buzzer are perfectly happy at @ref GPIO_LOW_SPEED.
 */
typedef enum {
  GPIO_LOW_SPEED = 0,  /**< Low speed. */
  GPIO_MEDIUM_SPEED,   /**< Medium speed. */
  GPIO_HIGH_SPEED,     /**< High speed. */
  GPIO_VERY_HIGH_SPEED /**< Very high speed. */
} GPIO_OutputSpeed_t;

/** @brief Internal pull resistor — the 2-bit field written into `PUPDR`. */
typedef enum {
  GPIO_NO_PULL = 0, /**< Floating: no internal pull resistor. */
  GPIO_PULL_UP,     /**< Internal pull-up to VDD. */
  GPIO_PULL_DOWN    /**< Internal pull-down to VSS. */
} GPIO_PullUpDown_t;

/**
 * @brief Alternate-function number — the 4-bit field written into `AFR[]`.
 * @note  Which AF number maps to which peripheral is fixed by the datasheet's
 *        alternate-function table, not by this driver. On this board the ones
 *        that matter are AF5 (SPI1, the IMU) and AF7 (USART1/USART2).
 */
typedef enum {
  GPIO_AF0 = 0, /**< AF0  — system (MCO, SWD, JTAG). */
  GPIO_AF1,     /**< AF1  — TIM1/TIM2. */
  GPIO_AF2,     /**< AF2  — TIM3/TIM4/TIM5. */
  GPIO_AF3,     /**< AF3  — TIM8..TIM11. */
  GPIO_AF4,     /**< AF4  — I2C1..I2C3. */
  GPIO_AF5,     /**< AF5  — SPI1/SPI2/SPI4 (the MPU9250 link). */
  GPIO_AF6,     /**< AF6  — SPI3. */
  GPIO_AF7,     /**< AF7  — USART1/USART2/USART3 (the ESP32 and Raspberry Pi links). */
  GPIO_AF8,     /**< AF8  — UART4..UART6. */
  GPIO_AF9,     /**< AF9  — CAN1/CAN2, TIM12..TIM14. */
  GPIO_AF10,    /**< AF10 — USB OTG. */
  GPIO_AF11,    /**< AF11 — reserved on this part. */
  GPIO_AF12,    /**< AF12 — SDIO, FMC. */
  GPIO_AF13,    /**< AF13 — DCMI. */
  GPIO_AF14,    /**< AF14 — reserved on this part. */
  GPIO_AF15     /**< AF15 — EVENTOUT. */
} GPIO_AlternateFunction_t;

/** @brief Logic level of a pin. */
typedef enum {
  GPIO_PIN_LOW = 0, /**< Logic 0 / 0 V. */
  GPIO_PIN_HIGH     /**< Logic 1 / VDD. */
} GPIO_PinValue_t;

/**
 * @brief Full configuration of one pin, as passed to @ref GPIO_enumPinInit.
 *
 * `Otype` and `Speed` only matter when `Mode` is @ref GPIO_OUTPUT or
 * @ref GPIO_ALTFN; `AlternateFunction` only matters when `Mode` is
 * @ref GPIO_ALTFN. The unused members are still written, harmlessly.
 */
typedef struct {
  GPIO_Port_t Port;                           /**< Which port the pin belongs to. */
  GPIO_Pin_t PinNum;                          /**< Which pin within that port. */
  GPIO_Mode_t Mode;                           /**< Input, output, alternate function or analog. */
  GPIO_OutputType_t Otype;                    /**< Push-pull or open-drain (output modes only). */
  GPIO_OutputSpeed_t Speed;                   /**< Slew rate (output modes only). */
  GPIO_PullUpDown_t PullType;                 /**< Internal pull resistor. */
  GPIO_AlternateFunction_t AlternateFunction; /**< AF number (alternate-function mode only). */
} GPIO_PinConfig_t;

/** @brief Which half of a port a half-port operation applies to. */
typedef enum {
  PORT_FIRST_HALF = 0, /**< Pins 0..7. */
  PORT_SECOND_HALF     /**< Pins 8..15. */
} GPIO_PortHalf_t;

/**
 * @brief Configuration of 8 consecutive pins, as passed to @ref GPIO_enumPort8PinsInit.
 *
 * All eight pins get the same mode, output type, speed and pull.
 */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< First of the 8 pins; must be @ref GPIO_PIN8 or below so the run fits in the port. */
  GPIO_Mode_t Mode;           /**< Mode applied to all 8 pins. */
  GPIO_OutputType_t Otype;    /**< Output type applied to all 8 pins. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to all 8 pins. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to all 8 pins. */
} GPIO_8PinsConfig_t;

/**
 * @brief Configuration of 4 consecutive pins, as passed to @ref GPIO_enumPort4PinsInit.
 *
 * All four pins get the same mode, output type, speed and pull.
 */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< First of the 4 pins; must be @ref GPIO_PIN12 or below so the run fits in the port. */
  GPIO_Mode_t Mode;           /**< Mode applied to all 4 pins. */
  GPIO_OutputType_t Otype;    /**< Output type applied to all 4 pins. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to all 4 pins. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to all 4 pins. */
} GPIO_4PinsConfig_t;

/** @brief Configuration of pins 0..3 of a port, as passed to @ref GPIO_enumLowNibbleInit. */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< Must be @ref GPIO_PIN0 — the low nibble starts at pin 0 by definition. */
  GPIO_Mode_t Mode;           /**< Mode applied to pins 0..3. */
  GPIO_OutputType_t Otype;    /**< Output type applied to pins 0..3. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to pins 0..3. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to pins 0..3. */
} GPIO_LowNibbleConfig_t;

/** @brief Configuration of pins 4..7 of a port, as passed to @ref GPIO_enumHighNibbleInit. */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< Must be @ref GPIO_PIN4 — the high nibble starts at pin 4 by definition. */
  GPIO_Mode_t Mode;           /**< Mode applied to pins 4..7. */
  GPIO_OutputType_t Otype;    /**< Output type applied to pins 4..7. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to pins 4..7. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to pins 4..7. */
} GPIO_HighNibbleConfig_t;

/** @brief Configuration of pins 0..7 of a port, as passed to @ref GPIO_enumByteInit. */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< Must be @ref GPIO_PIN0 — the low byte starts at pin 0 by definition. */
  GPIO_Mode_t Mode;           /**< Mode applied to pins 0..7. */
  GPIO_OutputType_t Otype;    /**< Output type applied to pins 0..7. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to pins 0..7. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to pins 0..7. */
} GPIO_ByteConfig_t;

/** @brief Configuration of half a port, as passed to @ref GPIO_enumHalfPortInit. */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< Must be @ref GPIO_PIN0. */
  GPIO_Mode_t Mode;           /**< Mode applied to the half port. */
  GPIO_OutputType_t Otype;    /**< Output type applied to the half port. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to the half port. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to the half port. */
} GPIO_HalfPortConfig_t;

/** @brief Configuration of all 16 pins of a port, as passed to @ref GPIO_enumPortInit. */
typedef struct {
  GPIO_Port_t Port;           /**< Which port. */
  GPIO_Pin_t StartPin;        /**< Must be @ref GPIO_PIN0 — a whole-port init always starts at pin 0. */
  GPIO_Mode_t Mode;           /**< Mode applied to all 16 pins. */
  GPIO_OutputType_t Otype;    /**< Output type applied to all 16 pins. */
  GPIO_OutputSpeed_t Speed;   /**< Output speed applied to all 16 pins. */
  GPIO_PullUpDown_t PullType; /**< Pull configuration applied to all 16 pins. */
} GPIO_PortConfig_t;

/*============================================================================*/
/*                             INITIALISATION                                 */
/*============================================================================*/

/**
 * @brief Configure a single pin: mode, output type, speed, pull and AF.
 * @param[in] PinConfig Fully populated pin configuration.
 * @retval OK           The pin was configured.
 * @retval NULL_POINTER @p PinConfig was NULL.
 * @retval NOK          A field of @p PinConfig was out of range.
 *
 * @code
 * GPIO_PinConfig_t led = {GPIO_PORTA, GPIO_PIN5, GPIO_OUTPUT,
 *                         GPIO_PUSH_PULL, GPIO_MEDIUM_SPEED,
 *                         GPIO_NO_PULL, GPIO_AF0};
 * GPIO_enumPinInit(&led);
 * @endcode
 */
ErrorState_t GPIO_enumPinInit(const GPIO_PinConfig_t *PinConfig);

/**
 * @brief Configure pins 0..3 of a port identically, in one call.
 * @param[in] LowNibbleConfig Configuration to apply to the low nibble.
 * @retval OK           The four pins were configured.
 * @retval NULL_POINTER @p LowNibbleConfig was NULL.
 * @retval NOK          A field of @p LowNibbleConfig was out of range.
 */
ErrorState_t GPIO_enumLowNibbleInit(GPIO_LowNibbleConfig_t *LowNibbleConfig);

/**
 * @brief Configure pins 4..7 of a port identically, in one call.
 * @param[in] HighNibbleConfig Configuration to apply to the high nibble.
 * @retval OK           The four pins were configured.
 * @retval NULL_POINTER @p HighNibbleConfig was NULL.
 * @retval NOK          A field of @p HighNibbleConfig was out of range.
 */
ErrorState_t GPIO_enumHighNibbleInit(GPIO_HighNibbleConfig_t *HighNibbleConfig);

/**
 * @brief Configure pins 0..7 of a port identically, in one call.
 * @param[in] ByteConfig Configuration to apply to the low byte.
 * @retval OK           The eight pins were configured.
 * @retval NULL_POINTER @p ByteConfig was NULL.
 * @retval NOK          A field of @p ByteConfig was out of range.
 */
ErrorState_t GPIO_enumByteInit(GPIO_ByteConfig_t *ByteConfig);

/**
 * @brief Configure half a port identically, in one call.
 * @param[in] HalfPortConfig Configuration to apply to the half port.
 * @retval OK           The pins were configured.
 * @retval NULL_POINTER @p HalfPortConfig was NULL.
 * @retval NOK          A field of @p HalfPortConfig was out of range.
 */
ErrorState_t GPIO_enumHalfPortInit(GPIO_HalfPortConfig_t *HalfPortConfig);

/**
 * @brief Configure all 16 pins of a port identically, in one call.
 * @param[in] PortConfig Configuration to apply to the whole port.
 * @retval OK           The sixteen pins were configured.
 * @retval NULL_POINTER @p PortConfig was NULL.
 * @retval NOK          A field of @p PortConfig was out of range.
 */
ErrorState_t GPIO_enumPortInit(GPIO_PortConfig_t *PortConfig);

/**
 * @brief Configure 8 consecutive pins starting at an arbitrary pin.
 * @param[in] GPIO_8PinsConfig Configuration, including the first pin of the run.
 * @retval OK           The eight pins were configured.
 * @retval NULL_POINTER @p GPIO_8PinsConfig was NULL.
 * @retval NOK          A field was out of range, or the run would not fit in the port.
 */
ErrorState_t GPIO_enumPort8PinsInit(GPIO_8PinsConfig_t *GPIO_8PinsConfig);

/**
 * @brief Configure 4 consecutive pins starting at an arbitrary pin.
 * @param[in] GPIO_4PinsConfig Configuration, including the first pin of the run.
 * @retval OK           The four pins were configured.
 * @retval NULL_POINTER @p GPIO_4PinsConfig was NULL.
 * @retval NOK          A field was out of range, or the run would not fit in the port.
 */
ErrorState_t GPIO_enumPort4PinsInit(GPIO_4PinsConfig_t *GPIO_4PinsConfig);

/*============================================================================*/
/*                              SINGLE-PIN I/O                                */
/*============================================================================*/

/**
 * @brief Drive one output pin high or low.
 * @param[in] Port   Which port.
 * @param[in] PinNum Which pin.
 * @param[in] PinVal @ref GPIO_PIN_HIGH or @ref GPIO_PIN_LOW.
 * @retval OK  The pin was driven.
 * @retval NOK @p Port, @p PinNum or @p PinVal was out of range.
 *
 * @code
 * GPIO_enumWritePinVal(GPIO_PORTA, GPIO_PIN5, GPIO_PIN_HIGH);
 * @endcode
 */
ErrorState_t GPIO_enumWritePinVal(GPIO_Port_t Port, GPIO_Pin_t PinNum,
                                  GPIO_PinValue_t PinVal);

/**
 * @brief Read the level currently present on one pin.
 * @param[in]  Port   Which port.
 * @param[in]  PinNum Which pin.
 * @param[out] PinVal Receives @ref GPIO_PIN_HIGH or @ref GPIO_PIN_LOW.
 * @retval OK           The level was read into @p PinVal.
 * @retval NULL_POINTER @p PinVal was NULL.
 * @retval NOK          @p Port or @p PinNum was out of range.
 *
 * @note Reads `IDR`, so this reports the level *on the pin* — for an output pin
 *       that is being loaded down, that may differ from what was written.
 */
ErrorState_t GPIO_enumReadPinVal(GPIO_Port_t Port, GPIO_Pin_t PinNum,
                                 GPIO_PinValue_t *PinVal);

/**
 * @brief Invert the current output level of one pin.
 * @param[in] Port   Which port.
 * @param[in] PinNum Which pin.
 * @retval OK  The pin was toggled.
 * @retval NOK @p Port or @p PinNum was out of range.
 */
ErrorState_t GPIO_enumTogPinVal(GPIO_Port_t Port, GPIO_Pin_t PinNum);

/*============================================================================*/
/*                             MULTI-PIN WRITES                               */
/*============================================================================*/

/**
 * @brief Write an 8-bit value onto 8 consecutive output pins.
 * @param[in] Port     Which port.
 * @param[in] StartPin First pin of the run; must be @ref GPIO_PIN8 or below.
 * @param[in] Value    The value to place on the pins, 0x00..0xFF.
 * @retval OK  The value was written.
 * @retval NOK @p Port or @p StartPin was out of range.
 *
 * @code
 * GPIO_enumWrite8PinsVal(GPIO_PORTA, GPIO_PIN0, 0xFF);
 * @endcode
 */
ErrorState_t GPIO_enumWrite8PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                    uint8_t Value);

/**
 * @brief Write a 4-bit value onto 4 consecutive output pins.
 * @param[in] Port     Which port.
 * @param[in] StartPin First pin of the run; must be @ref GPIO_PIN12 or below.
 * @param[in] Value    The value to place on the pins, 0x0..0xF (upper nibble ignored).
 * @retval OK  The value was written.
 * @retval NOK @p Port or @p StartPin was out of range.
 */
ErrorState_t GPIO_enumWrite4PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                    uint8_t Value);

/**
 * @brief Set or clear a chosen subset of pins 0..3, atomically.
 *
 * @p Copy_u8Val is a **bit mask**, not a value: bit *n* selects pin *n*. Every
 * selected pin is then driven to @p PinsVal. The write goes through `BSRR`, so
 * the untouched pins of the port are not disturbed and no read-modify-write
 * race with an ISR is possible.
 *
 * @param[in] port       Which port.
 * @param[in] Copy_u8Val Mask of the pins to act on; only bits 0..3 are honoured.
 * @param[in] PinsVal    @ref GPIO_PIN_HIGH to set the selected pins, @ref GPIO_PIN_LOW to clear them.
 * @retval OK  The selected pins were driven.
 * @retval NOK @p PinsVal was out of range.
 *
 * @code
 * // Drive pins 0 and 2 of port A high, leaving pins 1 and 3 untouched.
 * GPIO_enumWriteLowNibbleVal(GPIO_PORTA, 0x05, GPIO_PIN_HIGH);
 * @endcode
 */
ErrorState_t GPIO_enumWriteLowNibbleVal(GPIO_Port_t port, uint8_t Copy_u8Val,
                                     GPIO_PinValue_t PinsVal);

/**
 * @brief Set or clear a chosen subset of pins 4..7, atomically.
 *
 * As @ref GPIO_enumWriteLowNibbleVal, but for the high nibble: @p Copy_u8Val is
 * a mask in which bit *n* selects pin *n*, and only bits 4..7 are honoured.
 *
 * @param[in] port       Which port.
 * @param[in] Copy_u8Val Mask of the pins to act on; only bits 4..7 are honoured.
 * @param[in] PinsVal    @ref GPIO_PIN_HIGH to set the selected pins, @ref GPIO_PIN_LOW to clear them.
 * @retval OK  The selected pins were driven.
 * @retval NOK @p PinsVal was out of range.
 */
ErrorState_t GPIO_enumWriteHighNibbleVal(GPIO_Port_t port, uint8_t Copy_u8Val,
                                      GPIO_PinValue_t PinsVal);

/**
 * @brief Set or clear a chosen subset of pins 0..7, atomically.
 *
 * As @ref GPIO_enumWriteLowNibbleVal, but the mask covers the whole low byte.
 *
 * @param[in] port       Which port.
 * @param[in] Copy_u8Val Mask of the pins to act on, bits 0..7.
 * @param[in] PinsVal    @ref GPIO_PIN_HIGH to set the selected pins, @ref GPIO_PIN_LOW to clear them.
 * @retval OK  The selected pins were driven.
 * @retval NOK @p PinsVal was out of range.
 */
ErrorState_t GPIO_enumWriteByteVal(GPIO_Port_t port, uint8_t Copy_u8Val,
                                GPIO_PinValue_t PinsVal);

/**
 * @brief Set or clear a chosen subset of all 16 pins, atomically.
 *
 * As @ref GPIO_enumWriteLowNibbleVal, but the mask covers all 16 pins.
 *
 * @param[in] port        Which port.
 * @param[in] Copy_u16Val Mask of the pins to act on, bits 0..15.
 * @param[in] PinsVal     @ref GPIO_PIN_HIGH to set the selected pins, @ref GPIO_PIN_LOW to clear them.
 * @retval OK  The selected pins were driven.
 * @retval NOK @p PinsVal was out of range.
 */
ErrorState_t GPIO_enumWriteHalfWordVal(GPIO_Port_t port, uint16_t Copy_u16Val,
                                    GPIO_PinValue_t PinsVal);

/**
 * @brief Set or clear a chosen subset of all 16 pins, atomically.
 *
 * Identical in behaviour to @ref GPIO_enumWriteHalfWordVal — a port on this MCU
 * is 16 pins wide, so a "port" write and a "half word" write cover the same bits.
 *
 * @param[in] port        Which port.
 * @param[in] Copy_u16Val Mask of the pins to act on, bits 0..15.
 * @param[in] PinsVal     @ref GPIO_PIN_HIGH to set the selected pins, @ref GPIO_PIN_LOW to clear them.
 * @retval OK  The selected pins were driven.
 * @retval NOK @p PinsVal was out of range.
 */
ErrorState_t GPIO_enumWritePortVal(GPIO_Port_t port, uint16_t Copy_u16Val,
                                GPIO_PinValue_t PinsVal);

/*============================================================================*/
/*                              MULTI-PIN READS                               */
/*============================================================================*/

/**
 * @brief Read 4 consecutive pins as a 4-bit value.
 * @param[in]  Port      Which port.
 * @param[in]  StartPin  First pin of the run; must be @ref GPIO_PIN12 or below.
 * @param[out] PinsValue Receives the 4-bit value, right-aligned in the byte.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p PinsValue was NULL.
 * @retval NOK          @p Port or @p StartPin was out of range.
 */
ErrorState_t GPIO_enumRead4PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                   uint8_t *PinsValue);

/**
 * @brief Read 8 consecutive pins as an 8-bit value.
 * @param[in]  Port      Which port.
 * @param[in]  StartPin  First pin of the run; must be @ref GPIO_PIN8 or below.
 * @param[out] PinsValue Receives the 8-bit value, right-aligned.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p PinsValue was NULL.
 * @retval NOK          @p Port or @p StartPin was out of range.
 */
ErrorState_t GPIO_enumRead8PinsVal(GPIO_Port_t Port, GPIO_Pin_t StartPin,
                                   uint8_t *PinsValue);

/**
 * @brief Read pins 0..3 of a port as a nibble.
 * @param[in]  port         Which port.
 * @param[out] LowNibbleVal Receives the value of pins 0..3, right-aligned.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p LowNibbleVal was NULL.
 * @retval NOK          @p port was out of range.
 */
ErrorState_t GPIO_enumReadLowNibbleVal(GPIO_Port_t port, uint8_t *LowNibbleVal);

/**
 * @brief Read pins 4..7 of a port as a nibble.
 * @param[in]  port          Which port.
 * @param[out] HighNibbleVal Receives the value of pins 4..7, right-aligned.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p HighNibbleVal was NULL.
 * @retval NOK          @p port was out of range.
 */
ErrorState_t GPIO_enumReadHighNibbleVal(GPIO_Port_t port,
                                        uint8_t *HighNibbleVal);

/**
 * @brief Read pins 0..7 of a port as a byte.
 * @param[in]  port    Which port.
 * @param[out] ByteVal Receives the value of pins 0..7.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p ByteVal was NULL.
 * @retval NOK          @p port was out of range.
 */
ErrorState_t GPIO_enumReadByteVal(GPIO_Port_t port, uint8_t *ByteVal);

/**
 * @brief Read all 16 pins of a port as a half word.
 * @param[in]  port        Which port.
 * @param[out] HalfPortVal Receives the value of pins 0..15.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p HalfPortVal was NULL.
 * @retval NOK          @p port was out of range.
 */
ErrorState_t GPIO_enumReadHalfPortVal(GPIO_Port_t port, uint16_t *HalfPortVal);

/**
 * @brief Read the whole `IDR` register of a port.
 * @param[in]  port    Which port.
 * @param[out] PortVal Receives `IDR`; only bits 0..15 carry pin levels.
 * @retval OK           The value was read.
 * @retval NULL_POINTER @p PortVal was NULL.
 * @retval NOK          @p port was out of range.
 */
ErrorState_t GPIO_enumReadPortVal(GPIO_Port_t port, uint32_t *PortVal);

/** @} */ /* end of mcal_gpio */

#endif /* GPIO_INTERFACE_H_ */
