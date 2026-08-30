/**
 ******************************************************************************
 * @file    STM32F446xx.h
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   STM32F446RE register map: peripheral base addresses, register
 *          layout structs, and the typed pointers the MCAL drivers write to.
 * @ingroup lib
 *
 * This is the hand-written replacement for ST's CMSIS device header — the
 * project deliberately uses no vendor HAL/LL code, so this file is the single
 * point where a C symbol is bound to a physical peripheral address.
 *
 * Each peripheral is described in two steps:
 * 1. a `<PERIPH>_BASEADDR` constant, taken from the memory map in RM0390;
 * 2. a `<PERIPH>_RegDef_t` struct whose members are laid out in register
 *    order, so member offsets match hardware offsets exactly.
 *
 * The `M<PERIPH>` macros then cast the base address to that struct, which is
 * how every driver reaches the hardware, e.g. `MGPIOA->ODR |= (1 << 5);`.
 *
 * @warning Member order and the `RESERVED` padding arrays are load-bearing:
 *          they are what makes each member land on its real hardware offset.
 *          Never reorder a member or drop a padding field.
 * @note    Register bit *positions* do not live here — each driver keeps its
 *          own bit definitions in its `<NAME>_private.h`.
 ******************************************************************************
 */

#ifndef STM32F446xx_H
#define STM32F446xx_H

#include <stdint.h>

/**
 * @addtogroup lib
 * @{
 */

/**
 * @name Memory base addresses
 * @{
 */
#define FLASH_BASEADDR 0x08000000UL /**< Start of on-chip Flash (512 KB), where the vector table and code live. */
#define SRAM_BASEADDR  0x20000000UL /**< Start of SRAM1 (128 KB): the stack, the heap and all FreeRTOS objects. */
#define ROM_BASEADDR   0x1FFF0000UL /**< Start of system memory, the factory ROM bootloader. */
/** @} */

/**
 * @name Cortex-M4 core peripheral base addresses
 * @{
 */
#define NVIC_BASEADDR   0XE000E100UL /**< Nested vectored interrupt controller. */
#define SCB_BASEADDR    0XE000ED00UL /**< System control block. */
#define SYSTIC_BASEADDR 0XE000E010UL /**< SysTick 24-bit system timer. */
/** @} */

/**
 * @name AHB1 peripheral base addresses
 * @{
 */
#define GPIOA_BASEADDR 0X40020000UL /**< GPIO port A. */
#define GPIOB_BASEADDR 0X40020400UL /**< GPIO port B. */
#define GPIOC_BASEADDR 0X40020800UL /**< GPIO port C. */
#define GPIOD_BASEADDR 0X40020C00UL /**< GPIO port D. */
#define GPIOE_BASEADDR 0X40021000UL /**< GPIO port E. */
#define GPIOF_BASEADDR 0X40021400UL /**< GPIO port F. */
#define GPIOG_BASEADDR 0X40021800UL /**< GPIO port G. */
#define GPIOH_BASEADDR 0X40021C00UL /**< GPIO port H. */

#define RCC_BASEADDR 0x40023800UL /**< Reset and clock control. */

#define DMA1_BASEADDR 0X40026000UL /**< DMA controller 1 (declared for completeness; the firmware does not use DMA). */
#define DMA2_BASEADDR 0X40026400UL /**< DMA controller 2 (declared for completeness; the firmware does not use DMA). */
/** @} */

/**
 * @name APB1 peripheral base addresses
 * @{
 */
#define USART2_BASEADDR 0x40004400UL /**< USART2 — the Raspberry Pi telemetry link. */
#define USART3_BASEADDR 0x40004800UL /**< USART3. */
#define USART4_BASEADDR 0x40004C00UL /**< UART4. */
#define USART5_BASEADDR 0x40005000UL /**< UART5. */
#define SPI2_BASEADDR   0X40003800UL /**< SPI2. */
#define SPI3_BASEADDR   0X40003C00UL /**< SPI3. */
/** @} */

/**
 * @name APB2 peripheral base addresses
 * @{
 */
#define SYSCFG_BASEADDR 0X40013800UL /**< System configuration controller (EXTI port routing). */
#define EXTI_BASEADDR   0X40013C00UL /**< External interrupt/event controller. */
#define USART1_BASEADDR 0X40011000UL /**< USART1 — the ESP32 V2V radio link. */
#define USART6_BASEADDR 0X40011400UL /**< USART6. */
#define SPI1_BASEADDR   0X40013000UL /**< SPI1 — the MPU9250 IMU link. */
#define SPI4_BASEADDR   0X40013400UL /**< SPI4. */
/** @} */

/**
 * @name Timer peripheral base addresses
 * @{
 */
#define TIM1_BASE  (0x40010000UL) /**< TIM1: advanced-control timer. */
#define TIM2_BASE  (0x40000000UL) /**< TIM2: 32-bit general-purpose timer. */
#define TIM3_BASE  (0x40000400UL) /**< TIM3: general-purpose timer. */
#define TIM4_BASE  (0x40000800UL) /**< TIM4: general-purpose timer. */
#define TIM5_BASE  (0x40000C00UL) /**< TIM5: 32-bit general-purpose timer. */
#define TIM6_BASE  (0x40001000UL) /**< TIM6: basic timer. */
#define TIM7_BASE  (0x40001400UL) /**< TIM7: basic timer. */
#define TIM8_BASE  (0x40010400UL) /**< TIM8: advanced-control timer. */
#define TIM9_BASE  (0x40014000UL) /**< TIM9: general-purpose timer. */
#define TIM10_BASE (0x40014400UL) /**< TIM10: general-purpose timer. */
#define TIM11_BASE (0x40014800UL) /**< TIM11: general-purpose timer. */
#define TIM12_BASE (0x40001800UL) /**< TIM12: general-purpose timer. */
#define TIM13_BASE (0x40001C00UL) /**< TIM13: general-purpose timer. */
#define TIM14_BASE (0x40002000UL) /**< TIM14: general-purpose timer. */
/** @} */

/*============================================================================*/
/*                            REGISTER LAYOUTS                                */
/*============================================================================*/

/**
 * @brief SysTick register layout (Cortex-M4 core timer).
 *
 * Used by the MCAL SYSTICK driver for the microsecond/millisecond delays that
 * the ultrasonic trigger pulses depend on.
 */
typedef struct
{
  volatile uint32_t CTRL;  /**< Control and status: enable, tick interrupt, clock source, COUNTFLAG. */
  volatile uint32_t LOAD;  /**< Reload value loaded into VAL each time the counter reaches zero. */
  volatile uint32_t VAL;   /**< Current counter value; writing any value clears it and COUNTFLAG. */
  volatile uint32_t CALIB; /**< Calibration value (read-only, factory programmed). */
} SYSTIC_RegDef_t;

/** @brief Typed pointer to the SysTick peripheral. */
#define MSYSTIC ((SYSTIC_RegDef_t *)SYSTIC_BASEADDR)

/**
 * @brief GPIO port register layout (identical for every port A..H).
 */
typedef struct
{
  volatile uint32_t MODER;   /**< Mode: 2 bits per pin — input / output / alternate function / analog. */
  volatile uint32_t OTYPER;  /**< Output type: 1 bit per pin — push-pull or open-drain. */
  volatile uint32_t OSPEEDR; /**< Output speed: 2 bits per pin — low / medium / fast / high. */
  volatile uint32_t PUPDR;   /**< Pull-up/pull-down: 2 bits per pin — none / pull-up / pull-down. */
  volatile uint32_t IDR;     /**< Input data (read-only): the level currently on each pin. */
  volatile uint32_t ODR;     /**< Output data: the level driven onto each output pin. */
  volatile uint32_t BSRR;    /**< Bit set/reset: atomic set (low half) and reset (high half) of ODR bits. */
  volatile uint32_t LCKR;    /**< Configuration lock: freezes a pin's config until the next reset. */
  volatile uint32_t AFR[2];  /**< Alternate function: `AFR[0]` covers pins 0..7, `AFR[1]` covers pins 8..15, 4 bits per pin. */
} GPIO_REGDEF_t;

/**
 * @name GPIO port pointers
 * @{
 */
#define MGPIOA ((GPIO_REGDEF_t *)GPIOA_BASEADDR) /**< Typed pointer to GPIO port A. */
#define MGPIOB ((GPIO_REGDEF_t *)GPIOB_BASEADDR) /**< Typed pointer to GPIO port B. */
#define MGPIOC ((GPIO_REGDEF_t *)GPIOC_BASEADDR) /**< Typed pointer to GPIO port C. */
#define MGPIOD ((GPIO_REGDEF_t *)GPIOD_BASEADDR) /**< Typed pointer to GPIO port D. */
#define MGPIOE ((GPIO_REGDEF_t *)GPIOE_BASEADDR) /**< Typed pointer to GPIO port E. */
#define MGPIOF ((GPIO_REGDEF_t *)GPIOF_BASEADDR) /**< Typed pointer to GPIO port F. */
#define MGPIOG ((GPIO_REGDEF_t *)GPIOG_BASEADDR) /**< Typed pointer to GPIO port G. */
#define MGPIOH ((GPIO_REGDEF_t *)GPIOH_BASEADDR) /**< Typed pointer to GPIO port H. */
/** @} */

/**
 * @brief Register layout of a single DMA stream.
 * @note  Declared for completeness — the firmware drives every peripheral by
 *        interrupt, not by DMA, so no driver uses this.
 */
typedef struct
{
  uint32_t CR;   /**< Stream configuration: channel, direction, priority, enable. */
  uint32_t NDTR; /**< Number of data items still to transfer. */
  uint32_t PAR;  /**< Peripheral address the stream reads from or writes to. */
  uint32_t M0AR; /**< Memory 0 address (the buffer used in single-buffer mode). */
  uint32_t M1AR; /**< Memory 1 address (the second buffer in double-buffer mode). */
  uint32_t FCR;  /**< FIFO control: threshold and FIFO status. */
} DMA_STREAM_REGDEF_t;

/**
 * @brief DMA controller register layout: the shared registers plus 8 streams.
 * @note  Declared for completeness — see @ref DMA_STREAM_REGDEF_t.
 */
typedef struct
{
  uint32_t            LISR;      /**< Interrupt status for streams 0..3. */
  uint32_t            HISR;      /**< Interrupt status for streams 4..7. */
  uint32_t            LIFCR;     /**< Interrupt flag clear for streams 0..3. */
  uint32_t            HIFCR;     /**< Interrupt flag clear for streams 4..7. */
  DMA_STREAM_REGDEF_t Stream[8]; /**< The 8 per-stream register blocks. */
} DMA_REGDEF_t;

#define MDMA1 ((DMA_REGDEF_t *)DMA1_BASEADDR) /**< Typed pointer to DMA controller 1. */
#define MDMA2 ((DMA_REGDEF_t *)DMA2_BASEADDR) /**< Typed pointer to DMA controller 2. */

/**
 * @brief RCC (reset and clock control) register layout.
 *
 * This is the peripheral the MCAL RCC driver uses to bring the clock tree up to
 * 180 MHz and to gate the clock of every other peripheral on. A peripheral whose
 * clock is not enabled here reads back as all zeroes and silently ignores writes,
 * which is the single most common bring-up bug in a bare-metal driver.
 */
typedef struct
{
  volatile uint32_t CR;           /**< Clock control: HSI/HSE/PLL enable and their "ready" flags. */
  volatile uint32_t PLLCFGR;      /**< PLL configuration: the M, N, P, Q dividers and the PLL source. */
  volatile uint32_t CFGR;         /**< Clock configuration: system clock switch, AHB/APB1/APB2 prescalers. */
  volatile uint32_t CIR;          /**< Clock interrupt: ready-interrupt enables and flags. */
  volatile uint32_t AHP1RSTR;     /**< AHB1 peripheral reset. */
  volatile uint32_t AHP2RSTR;     /**< AHB2 peripheral reset. */
  volatile uint32_t AHP3RSTR;     /**< AHB3 peripheral reset. */
  volatile uint32_t RESERVED1[1]; /**< Padding — keeps APB1RSTR at its hardware offset. */
  volatile uint32_t APB1RSTR;     /**< APB1 peripheral reset. */
  volatile uint32_t APB2RSTR;     /**< APB2 peripheral reset. */
  volatile uint32_t RESERVED2[2]; /**< Padding — keeps AHP1ENR at its hardware offset. */
  volatile uint32_t AHP1ENR;      /**< AHB1 peripheral clock enable (the GPIO ports live here). */
  volatile uint32_t AHP2ENR;      /**< AHB2 peripheral clock enable. */
  volatile uint32_t AHP3ENR;      /**< AHB3 peripheral clock enable. */
  volatile uint32_t RESERVED3[1]; /**< Padding — keeps APB1ENR at its hardware offset. */
  volatile uint32_t APB1ENR;      /**< APB1 peripheral clock enable (USART2, SPI2/3, TIM2..7). */
  volatile uint32_t APB2ENR;      /**< APB2 peripheral clock enable (USART1/6, SPI1/4, SYSCFG, TIM1/8..11). */
  volatile uint32_t RESERVED4[2]; /**< Padding — keeps AHB1LPENR at its hardware offset. */
  volatile uint32_t AHB1LPENR;    /**< AHB1 clock enable while in low-power (sleep) mode. */
  volatile uint32_t AHP2LPENR;    /**< AHB2 clock enable while in low-power mode. */
  volatile uint32_t AHP3LPENR;    /**< AHB3 clock enable while in low-power mode. */
  volatile uint32_t RESERVED5[1]; /**< Padding — keeps APB1LPENR at its hardware offset. */
  volatile uint32_t APB1LPENR;    /**< APB1 clock enable while in low-power mode. */
  volatile uint32_t APB2LPENR;    /**< APB2 clock enable while in low-power mode. */
  volatile uint32_t RESERVED6[2]; /**< Padding — keeps BDCR at its hardware offset. */
  volatile uint32_t BDCR;         /**< Backup domain control: LSE oscillator and RTC clock source. */
  volatile uint32_t CSR;          /**< Clock control and status: LSI, and the reset-cause flags (including the IWDG reset flag). */
  volatile uint32_t RESERVED7[2]; /**< Padding — keeps SSCGR at its hardware offset. */
  volatile uint32_t SSCGR;        /**< Spread-spectrum clock generation (EMI reduction). */
  volatile uint32_t PLLI2SCFGR;   /**< PLLI2S configuration. */
  volatile uint32_t PLLSAICFGR;   /**< PLLSAI configuration. */
  volatile uint32_t DCKCFGR;      /**< Dedicated clock configuration. */
  volatile uint32_t CKGATENR;     /**< Clock gating enable. */
  volatile uint32_t DCKCFGR2;     /**< Dedicated clock configuration 2. */
} RCC_RegDef_t;

/** @brief Typed pointer to the RCC peripheral. */
#define MRCC ((RCC_RegDef_t *)RCC_BASEADDR)

/**
 * @brief SPI register layout (identical for SPI1..SPI4).
 *
 * Only SPI1 is used, as the master link to the MPU9250 IMU.
 */
typedef struct
{
  volatile uint32_t CR1;     /**< Control 1: master/slave, clock polarity and phase, baud prescaler, SPI enable. */
  volatile uint32_t CR2;     /**< Control 2: interrupt enables, DMA enables, slave-select output. */
  volatile uint32_t SR;      /**< Status: TXE, RXNE, BSY and the error flags. */
  volatile uint32_t DR;      /**< Data: writing transmits a frame, reading takes the received one. */
  volatile uint32_t CRCPR;   /**< CRC polynomial. */
  volatile uint32_t RXCRCR;  /**< CRC computed over the received frames. */
  volatile uint32_t TXCRCR;  /**< CRC computed over the transmitted frames. */
  volatile uint32_t I2SCFGR; /**< I2S mode configuration (unused — this SPI stays in SPI mode). */
  volatile uint32_t I2SPR;   /**< I2S prescaler (unused). */
} SPI_RegDef_t;

/**
 * @name SPI peripheral pointers
 * @{
 */
#define MSPI1 ((SPI_RegDef_t *)SPI1_BASEADDR) /**< Typed pointer to SPI1 — the MPU9250 link. */
#define MSPI2 ((SPI_RegDef_t *)SPI2_BASEADDR) /**< Typed pointer to SPI2. */
#define MSPI3 ((SPI_RegDef_t *)SPI3_BASEADDR) /**< Typed pointer to SPI3. */
#define MSPI4 ((SPI_RegDef_t *)SPI4_BASEADDR) /**< Typed pointer to SPI4. */
/** @} */

/**
 * @brief NVIC register layout (Cortex-M4 interrupt controller).
 *
 * Each of the array registers holds one bit per interrupt, so IRQ number `n`
 * lives in bit `n % 32` of word `n / 32` — which is exactly the arithmetic the
 * MCAL NVIC driver does.
 */
typedef struct
{
  volatile uint32_t ISER[8];        /**< Set-enable: writing a 1 enables that IRQ. */
  volatile uint32_t RESERVED1[24];  /**< Padding — keeps ICER at its hardware offset. */
  volatile uint32_t ICER[8];        /**< Clear-enable: writing a 1 disables that IRQ. */
  volatile uint32_t RESERVED2[24];  /**< Padding — keeps ISPR at its hardware offset. */
  volatile uint32_t ISPR[8];        /**< Set-pending: writing a 1 forces that IRQ pending. */
  volatile uint32_t RESERVED3[24];  /**< Padding — keeps ICPR at its hardware offset. */
  volatile uint32_t ICPR[8];        /**< Clear-pending: writing a 1 drops a pending IRQ. */
  volatile uint32_t RESERVED4[24];  /**< Padding — keeps IABR at its hardware offset. */
  volatile uint32_t IABR[8];        /**< Active-bit (read-only): 1 while that IRQ's handler is running. */
  volatile uint32_t RESERVED5[56];  /**< Padding — keeps IPR at its hardware offset. */
  volatile uint8_t  IPR[240];       /**< Priority: one byte per IRQ (only the upper 4 bits are implemented). */
  volatile uint32_t RESERVED6[580]; /**< Padding — keeps STIR at its hardware offset. */
  volatile uint32_t STIR;           /**< Software trigger: writing an IRQ number raises it from software. */
} NVIC_RegDef_t;

/** @brief Typed pointer to the NVIC. */
#define MNVIC ((NVIC_RegDef_t *)NVIC_BASEADDR)

/**
 * @brief SCB (system control block) register layout.
 *
 * The driver uses this for `SystemInit()`, for relocating the vector table via
 * VTOR, and for the fault status registers.
 */
typedef struct
{
  uint32_t CPUID; /**< CPU identification: core type and revision (read-only). */
  uint32_t ICSR;  /**< Interrupt control and state: pending PendSV/SysTick, the active vector number. */
  uint32_t VTOR;  /**< Vector table offset: where the vector table lives in memory. */
  uint32_t AIRCR; /**< Application interrupt and reset control: priority grouping and the software-reset request. */
  uint32_t SCR;   /**< System control: sleep-on-exit, deep sleep, send-on-pend. */
  uint32_t CCR;   /**< Configuration and control: unaligned-access and divide-by-zero trapping. */
  uint32_t SHPR1; /**< System handler priority: MemManage, BusFault, UsageFault. */
  uint32_t SHPR2; /**< System handler priority: SVCall. */
  uint32_t SHPR3; /**< System handler priority: PendSV and SysTick (FreeRTOS lowers both of these). */
  uint32_t SHCSR; /**< System handler control and state: enables for the configurable faults. */
  uint8_t  CFSR;  /**< Configurable fault status: the MemManage byte. */
  uint8_t  BFSR;  /**< BusFault status. */
  uint16_t UFSR;  /**< UsageFault status. */
  uint32_t HFSR;  /**< HardFault status: says whether a fault escalated into a hard fault. */
  uint32_t DFSR;  /**< Debug fault status. */
  uint32_t MMAR;  /**< MemManage fault address: the address that faulted. */
  uint32_t BFAR;  /**< BusFault address: the address that faulted. */
  uint32_t AFSR;  /**< Auxiliary fault status (implementation defined). */
} SCB_RegDef_t;

/** @brief Typed pointer to the SCB. */
#define MSCB ((SCB_RegDef_t *)SCB_BASEADDR)

/**
 * @brief SYSCFG (system configuration controller) register layout.
 *
 * Its only job in this firmware is EXTICR: choosing *which port's* pin N drives
 * EXTI line N. Without it, every EXTI line would default to port A, and the six
 * ultrasonic echo interrupts would never fire.
 */
typedef struct
{
  uint32_t MEMRMP;       /**< Memory remap: what is mapped at address 0x00000000. */
  uint32_t PMC;          /**< Peripheral mode configuration. */
  uint32_t EXTICR[4];    /**< EXTI port routing: 4 bits per EXTI line, 4 lines per word. */
  uint32_t Reserved1[2]; /**< Padding — keeps CMPCR at its hardware offset. */
  uint32_t CMPCR;        /**< Compensation cell control. */
  uint32_t Reserved2[2]; /**< Padding — keeps CFGR at its hardware offset. */
  uint32_t CFGR;         /**< Configuration register. */
} SYSCFG_RegDef_t;

/** @brief Typed pointer to the SYSCFG peripheral. */
#define MSYSCFG ((SYSCFG_RegDef_t *)SYSCFG_BASEADDR)

/**
 * @brief EXTI (external interrupt/event controller) register layout.
 *
 * Every register holds one bit per EXTI line (0..22). The six HC-SR04 echo pins
 * are configured here for both-edge triggering: the rising edge starts the
 * echo timing, the falling edge ends it.
 */
typedef struct
{
  uint32_t IMR;   /**< Interrupt mask: 1 = this line may raise an interrupt. */
  uint32_t EMR;   /**< Event mask: 1 = this line may raise an event (no ISR). */
  uint32_t RTSR;  /**< Rising trigger select: 1 = trigger on the rising edge. */
  uint32_t FTSR;  /**< Falling trigger select: 1 = trigger on the falling edge. */
  uint32_t SWIER; /**< Software interrupt event: writing a 1 raises the line from software. */
  uint32_t PR;    /**< Pending: reads 1 when the line fired. Cleared by writing a 1 back — not a 0. */
} EXTI_RegDef_t;

/** @brief Typed pointer to the EXTI peripheral. */
#define MEXTI ((EXTI_RegDef_t *)EXTI_BASEADDR)

/**
 * @brief USART register layout (identical for USART1..6).
 *
 * USART1 carries the DSRC frames to and from the ESP32 radio bridge; USART2
 * carries the ASCII telemetry line to the Raspberry Pi.
 */
typedef struct
{
  uint32_t SR;   /**< Status: TXE, TC, RXNE, IDLE and the error flags (parity, framing, noise, overrun). */
  uint32_t DR;   /**< Data: writing transmits a byte, reading takes the received one and clears RXNE. */
  uint32_t BRR;  /**< Baud rate: the mantissa/fraction divider applied to the peripheral clock. */
  uint32_t CR1;  /**< Control 1: USART enable, TX/RX enable, word length, parity, the RXNE/TXE interrupt enables. */
  uint32_t CR2;  /**< Control 2: stop bits, clock settings. */
  uint32_t CR3;  /**< Control 3: flow control, DMA enables, error-interrupt enable. */
  uint32_t GTPR; /**< Guard time and prescaler (smartcard/IrDA modes — unused here). */
} USART_RegDef_t;

/**
 * @name USART peripheral pointers
 * @{
 */
#define MUSART1 ((USART_RegDef_t *)USART1_BASEADDR) /**< Typed pointer to USART1 — the ESP32 V2V link. */
#define MUSART2 ((USART_RegDef_t *)USART2_BASEADDR) /**< Typed pointer to USART2 — the Raspberry Pi telemetry link. */
#define MUSART3 ((USART_RegDef_t *)USART3_BASEADDR) /**< Typed pointer to USART3. */
#define MUSART4 ((USART_RegDef_t *)USART4_BASEADDR) /**< Typed pointer to UART4. */
#define MUSART5 ((USART_RegDef_t *)USART5_BASEADDR) /**< Typed pointer to UART5. */
#define MUSART6 ((USART_RegDef_t *)USART6_BASEADDR) /**< Typed pointer to USART6. */
/** @} */

/**
 * @brief Timer register layout, in the superset form used by TIM1/TIM8.
 *
 * The general-purpose and basic timers implement only part of this layout; the
 * members they lack read as zero. The firmware uses the timers as the time base
 * for the ultrasonic echo measurement and for PWM.
 */
typedef struct
{
  volatile uint32_t CR1;   /**< Control 1: counter enable, direction, auto-reload preload. */
  volatile uint32_t CR2;   /**< Control 2: master mode selection. */
  volatile uint32_t SMCR;  /**< Slave mode control: trigger source and slave mode (reset/gated/trigger). */
  volatile uint32_t DIER;  /**< DMA/interrupt enable: update and capture/compare interrupt enables. */
  volatile uint32_t SR;    /**< Status: update and capture/compare flags. Cleared by writing a 0 to the flag. */
  volatile uint32_t EGR;   /**< Event generation: forces an update or a capture/compare event from software. */
  volatile uint32_t CCMR1; /**< Capture/compare mode for channels 1 and 2 (input capture filter, or PWM mode). */
  volatile uint32_t CCMR2; /**< Capture/compare mode for channels 3 and 4. */
  volatile uint32_t CCER;  /**< Capture/compare enable: per-channel output/capture enable and polarity. */
  volatile uint32_t CNT;   /**< Counter: the current count value. */
  volatile uint32_t PSC;   /**< Prescaler: divides the timer clock, so 1 count = (PSC+1) timer clocks. */
  volatile uint32_t ARR;   /**< Auto-reload: the value the counter wraps at; sets the PWM period. */
  volatile uint32_t RCR;   /**< Repetition counter (advanced-control timers only). */
  volatile uint32_t CCR1;  /**< Capture/compare 1: the captured count, or the PWM duty for channel 1. */
  volatile uint32_t CCR2;  /**< Capture/compare 2. */
  volatile uint32_t CCR3;  /**< Capture/compare 3. */
  volatile uint32_t CCR4;  /**< Capture/compare 4. */
  volatile uint32_t BDTR;  /**< Break and dead-time (advanced-control timers only). */
  volatile uint32_t DCR;   /**< DMA control. */
  volatile uint32_t DMAR;  /**< DMA address for full transfer. */
  volatile uint32_t OR;    /**< Option register: timer-specific input remapping. */
} TIM_TypeDef;

/**
 * @name Timer peripheral pointers
 * @{
 */
#define TIM1  ((TIM_TypeDef *)TIM1_BASE)  /**< Typed pointer to TIM1. */
#define TIM2  ((TIM_TypeDef *)TIM2_BASE)  /**< Typed pointer to TIM2. */
#define TIM3  ((TIM_TypeDef *)TIM3_BASE)  /**< Typed pointer to TIM3. */
#define TIM4  ((TIM_TypeDef *)TIM4_BASE)  /**< Typed pointer to TIM4. */
#define TIM5  ((TIM_TypeDef *)TIM5_BASE)  /**< Typed pointer to TIM5. */
#define TIM6  ((TIM_TypeDef *)TIM6_BASE)  /**< Typed pointer to TIM6. */
#define TIM7  ((TIM_TypeDef *)TIM7_BASE)  /**< Typed pointer to TIM7. */
#define TIM8  ((TIM_TypeDef *)TIM8_BASE)  /**< Typed pointer to TIM8. */
#define TIM9  ((TIM_TypeDef *)TIM9_BASE)  /**< Typed pointer to TIM9. */
#define TIM10 ((TIM_TypeDef *)TIM10_BASE) /**< Typed pointer to TIM10. */
#define TIM11 ((TIM_TypeDef *)TIM11_BASE) /**< Typed pointer to TIM11. */
#define TIM12 ((TIM_TypeDef *)TIM12_BASE) /**< Typed pointer to TIM12. */
#define TIM13 ((TIM_TypeDef *)TIM13_BASE) /**< Typed pointer to TIM13. */
#define TIM14 ((TIM_TypeDef *)TIM14_BASE) /**< Typed pointer to TIM14. */
/** @} */

/** @} */ /* end of lib */

#endif /* STM32F446xx_H */
