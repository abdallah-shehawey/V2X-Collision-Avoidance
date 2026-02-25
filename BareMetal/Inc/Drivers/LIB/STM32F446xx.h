/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<    STM32F446xx.h     >>>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Abdelmoemen Shehawey                   **
 **                  Layer  : LIB                                             **
 **                  CPU    : Cortex-M4                                       **
 **                  MCU    : NUCLEO-F446RE                                   **
 **                  SWC    : STM32F446xx                                     **
 **                                                                           **
 **===========================================================================**
 */

#ifndef STM32F446xx_H
#define STM32F446xx_H

#include <stdint.h>

/**************************************         Various Memories Base Adresses
 * ******************************************/
#define FLASH_BASEADDR 0x08000000UL
#define SRAM_BASEADDR 0x20000000UL
#define ROM_BASEADDR 0x1FFF0000UL

/**************************************         NVIC Base Adresses
 * ******************************************/
#define NVIC_BASEADDR 0XE000E100UL

/**************************************         SCB Base Adresses
 * ******************************************/
#define SCB_BASEADDR 0XE000ED00UL

/**************************************         AHB1 Peripheral Base Adresses
 * ******************************************/
#define GPIOA_BASEADDR 0X40020000UL
#define GPIOB_BASEADDR 0X40020400UL
#define GPIOC_BASEADDR 0X40020800UL
#define GPIOD_BASEADDR 0X40020C00UL
#define GPIOE_BASEADDR 0X40021000UL
#define GPIOF_BASEADDR 0X40021400UL
#define GPIOG_BASEADDR 0X40021800UL
#define GPIOH_BASEADDR 0X40021C00UL

#define RCC_BASEADDR 0x40023800UL


/*Internal DMA Base Adresses */
#define DMA1_BASEADDR 0X40026000UL
#define DMA2_BASEADDR 0X40026400UL

/**************************************         AHB2 Peripheral Base Adresses
 * ******************************************/
/**************************************         AHB3 Peripheral Base Adresses
 * ******************************************/
/**************************************         APB1 Peripheral Base Adresses
 * ******************************************/

#define USART2_BASEADDR 0x40004400UL
#define USART3_BASEADDR 0x40004800UL
#define USART4_BASEADDR 0x40004C00UL
#define USART5_BASEADDR 0x40005000UL
#define SPI2_BASEADDR 0X40003800UL
#define SPI3_BASEADDR 0X40003C00UL

/**************************************         APB2 Peripheral Base Adresses
 * ******************************************/

#define SYSCFG_BASEADDR 0X40013800UL
#define EXTI_BASEADDR 0X40013C00UL
#define USART1_BASEADDR 0X40011000UL
#define USART6_BASEADDR 0X40011400UL
#define SPI1_BASEADDR 0X40013000UL
#define SPI4_BASEADDR 0X40013400UL

/**************************************         APB3 Peripheral Base Adresses
 * ******************************************/


 /************************************************** PPB Peripheral Base Addresses***********************/

#define SYSTIC_BASEADDR 0XE000E010UL

 
/**************************************         SYSTIC Peripheral Definitions
 * *********************************************/

typedef struct {
  volatile uint32_t CTRL; // Systic control & Status registers
  volatile uint32_t LOAD; // Systic 
  volatile uint32_t VAL;
  volatile uint32_t CALIB;
} SYSTIC_RegDef_t;

#define MSYSTIC ((SYSTIC_RegDef_t *)SYSTIC_BASEADDR)

/**************************************       GPIO Register Definition Structure  ******************************************/
typedef struct {
  volatile uint32_t MODER;   /* GPIO PORT mode register              */
  volatile uint32_t OTYPER;  /* GPIO PORT output type register       */
  volatile uint32_t OSPEEDR; /* GPIO PORT output speed register      */
  volatile uint32_t PUPDR;   /* GPIO PORT pull-up/pull-down register */
  volatile uint32_t IDR;     /* GPIO PORT input data register        */
  volatile uint32_t ODR;     /* GPIO PORT output data register       */
  volatile uint32_t BSRR;    /* GPIO PORT bit set/reset register     */
  volatile uint32_t LCKR;    /* GPIO PORT configuration lock register*/
  volatile uint32_t AFR[2];  /* GPIO alternate function low register */
} GPIO_REGDEF_t;

/**************************************       DMA Regster Definitions Structure  ******************************************/
typedef struct {
  uint32_t CR;   // Stream x configuration register (DMA_SxCR)
  uint32_t NDTR; // Stream x number of data register (DMA_SxNDTR)
  uint32_t PAR;  // Stream x peripheral address register (DMA_SxPAR)
  uint32_t M0AR; // Stream x memory 0 address register (DMA_SxM0AR)
  uint32_t M1AR; // Stream x memory 1 address register (DMA_SxM1AR)
  uint32_t FCR;  // Stream x FIFO control register (DMA_SxFCR)
} DMA_STREAM_REGDEF_t;

typedef struct {
  uint32_t LISR;  // Low interrupt status register (DMA_LISR)
  uint32_t HISR;  // High interrupt status register (DMA_HISR)
  uint32_t LIFCR; // Low interrupt flag clear register (DMA_LIFCR)
  uint32_t HIFCR; // High interrupt flag clear register (DMA_HIFCR)
  DMA_STREAM_REGDEF_t Stream[8];
} DMA_REGDEF_t;

#define MDMA1 ((DMA_REGDEF_t *)DMA1_BASEADDR)
#define MDMA2 ((DMA_REGDEF_t *)DMA2_BASEADDR)

/**************************************       RCC Register Definitions Structure* ******************************************/
typedef struct {
  volatile uint32_t CR;      /* RCC clock control register      */
  volatile uint32_t PLLCFGR; /* RCC PLL configuration register (RCC_PLLCFGR) */
  volatile uint32_t CFGR; /* RCC clock configuration register (RCC_CFGR)    */
  volatile uint32_t CIR;  /* RCC clock interrupt register (RCC_CIR)     */
  volatile uint32_t
      AHP1RSTR; /* RCC AHB1 peripheral reset register (RCC_AHB1RSTR) */
  volatile uint32_t
      AHP2RSTR; /* RCC AHB2 peripheral reset register (RCC_AHB2RSTR) */
  volatile uint32_t
      AHP3RSTR; /* RCC AHB3 peripheral reset register (RCC_AHB3RSTR) */
  volatile uint32_t RESERVED1[1]; /* RESERVED */
  volatile uint32_t
      APB1RSTR; /* RCC APB1 peripheral reset register (RCC_APB1RSTR) */
  volatile uint32_t
      APB2RSTR; /* RCC APB2 peripheral reset register (RCC_APB2RSTR) */
  volatile uint32_t RESERVED2[2]; /* RESERVED */
  volatile uint32_t
      AHP1ENR; /* RCC AHB1 peripheral clock enable register (RCC_AHB1ENR) */
  volatile uint32_t
      AHP2ENR; /* RCC AHB2 peripheral clock enable register (RCC_AHB2ENR) */
  volatile uint32_t
      AHP3ENR; /* RCC AHB3 peripheral clock enable register (RCC_AHB3ENR) */
  volatile uint32_t RESERVED3[1]; /* RESERVED */
  volatile uint32_t
      APB1ENR; /* RCC APB1 peripheral clock enable register (RCC_APB1ENR) */
  volatile uint32_t
      APB2ENR; /* RCC APB2 peripheral clock enable register (RCC_APB2ENR) */
  volatile uint32_t RESERVED4[2]; /* RESERVED */
  volatile uint32_t AHB1LPENR; /* RCC AHB1 peripheral clock enable in low power
                                  mode register(RCC_AHB1LPENR) */
  volatile uint32_t AHP2LPENR; /* RCC AHB2 peripheral clock enable in low power
                                  mode register(RCC_AHB2LPENR) */
  volatile uint32_t AHP3LPENR; /* RCC AHB3 peripheral clock enable in low power
                                  mode register(RCC_AHB3LPENR) */
  volatile uint32_t RESERVED5[1]; /* RESERVED */
  volatile uint32_t APB1LPENR; /* RCC APB1 peripheral clock enable in low power
                                  mode register(RCC_APB1LPENR) */
  volatile uint32_t APB2LPENR; /* RCC APB2 peripheral clock enabled in low power
                                  mode register(RCC_APB2LPENR)*/
  volatile uint32_t RESERVED6[2]; /* RESERVED */
  volatile uint32_t BDCR; /* RCC Backup domain control register (RCC_BDCR) */
  volatile uint32_t CSR;  /* RCC clock control & status register (RCC_CSR)  */
  volatile uint32_t RESERVED7[2]; /* RESERVED */
  volatile uint32_t
      SSCGR; /* RCC spread spectrum clock generation register (RCC_SSCGR) */
  volatile uint32_t
      PLLI2SCFGR; /* RCC PLLI2S configuration register (RCC_PLLI2SCFGR) */
  volatile uint32_t
      PLLSAICFGR; /* RCC PLL configuration register (RCC_PLLSAICFGR) */
  volatile uint32_t
      DCKCFGR; /* RCC dedicated clock configuration register (RCC_DCKCFGR) */
  volatile uint32_t CKGATENR; /* RCC clocks gated enable register (CKGATENR) */
  volatile uint32_t
      DCKCFGR2; /* RCC dedicated clocks configuration register 2 (DCKCFGR2) */
} RCC_RegDef_t;

/**************************************       SPI Register Definitions Structure
 * ******************************************/
typedef struct {
  volatile uint32_t CR1;     /* SPI Control Register 1 */
  volatile uint32_t CR2;     /* SPI Control Register 2 */
  volatile uint32_t SR;      /* SPI Status Register */
  volatile uint32_t DR;      /* SPI Data Register */
  volatile uint32_t CRCPR;   /* SPI CRC Polynomial Register */
  volatile uint32_t RXCRCR;  /* SPI Rx CRC Register */
  volatile uint32_t TXCRCR;  /* SPI Tx CRC Register */
  volatile uint32_t I2SCFGR; /* SPI I2S Configuration Register */
  volatile uint32_t I2SPR;   /* SPI I2S Prescaler Register */
} SPI_RegDef_t;

/**************************************         SPI Peripheral Definitions
 * *********************************************/
#define MSPI1 ((SPI_RegDef_t *)SPI1_BASEADDR)
#define MSPI2 ((SPI_RegDef_t *)SPI2_BASEADDR)
#define MSPI3 ((SPI_RegDef_t *)SPI3_BASEADDR)
#define MSPI4 ((SPI_RegDef_t *)SPI4_BASEADDR)

/**************************************         NVIC Peripheral Definitions
 * *********************************************/
typedef struct {
  volatile uint32_t ISER[8]; /* Interrupt Set Enable Register */
  volatile uint32_t RESERVED1[24];
  volatile uint32_t ICER[8]; /* Interrupt Clear Enable Register */
  volatile uint32_t RESERVED2[24];
  volatile uint32_t ISPR[8]; /* Interrupt Set Pending Register */
  volatile uint32_t RESERVED3[24];
  volatile uint32_t ICPR[8]; /* Interrupt Clear Pending Register */
  volatile uint32_t RESERVED4[24];
  volatile uint32_t IABR[8]; /* Interrupt Active Bit Register */
  volatile uint32_t RESERVED5[56];
  volatile uint8_t IPR[240]; /* Interrupt Priority Register */
  volatile uint32_t RESERVED6[580];
  volatile uint32_t STIR; /* Software Trigger Interrupt Register */
} NVIC_RegDef_t;

#define MNVIC ((NVIC_RegDef_t *)NVIC_BASEADDR)

/**************************************         GPIO Peripheral Definitions
 * ******************************************/

#define MGPIOA ((GPIO_REGDEF_t *)GPIOA_BASEADDR)
#define MGPIOB ((GPIO_REGDEF_t *)GPIOB_BASEADDR)
#define MGPIOC ((GPIO_REGDEF_t *)GPIOC_BASEADDR)
#define MGPIOD ((GPIO_REGDEF_t *)GPIOD_BASEADDR)
#define MGPIOE ((GPIO_REGDEF_t *)GPIOE_BASEADDR)
#define MGPIOF ((GPIO_REGDEF_t *)GPIOF_BASEADDR)
#define MGPIOG ((GPIO_REGDEF_t *)GPIOG_BASEADDR)
#define MGPIOH ((GPIO_REGDEF_t *)GPIOH_BASEADDR)

/**************************************         RCC Peripheral Definitions
 * *********************************************/

#define MRCC ((RCC_RegDef_t *)RCC_BASEADDR)

/**************************************         SCB Peripheral Definitions
 * *********************************************/

typedef struct {
  uint32_t CPUID; // CPU Identification Register
  uint32_t ICSR;  // Interrupt Control and State Register
  uint32_t VTOR;  // Vector Table Offset Register
  uint32_t AIRCR; // Application Interrupt and Reset Control Register
  uint32_t SCR;   // System Control Register
  uint32_t CCR;   // Configuration and Control Register
  uint32_t SHPR1; // System Handler Priority Register 1 (Priority of SVCall)
  uint32_t
      SHPR2; // System Handler Priority Register 2 (Priority of Debug Monitor)
  uint32_t SHPR3; // System Handler Priority Register 3 (Priority of PendSV and
                  // SysTick)
  uint32_t SHCSR; // System Handler Control and State Register
  uint8_t CFSR;   // Configurable Fault Status Register (lower 8 bits)
  uint8_t BFSR;   // BusFault Status Register (lower 8 bits)
  uint16_t UFSR;  // UsageFault Status Register (lower 16 bits)
  uint32_t HFSR;  // HardFault Status Register
  uint32_t DFSR;  // Debug Fault Status Register
  uint32_t MMAR;  // MemManage Fault Address Register
  uint32_t BFAR;  // BusFault Address Register
  uint32_t AFSR;  // Auxiliary Fault Status Register
} SCB_RegDef_t;

#define MSCB ((SCB_RegDef_t *)SCB_BASEADDR)

/**************************************         SYSCFG Peripheral Definitions
 * *********************************************/

typedef struct {
  uint32_t MEMRMP;    // Memory Remap Register
  uint32_t PMC;       // PMC Register
  uint32_t EXTICR[4]; // External Interrupt Configuration Register
  uint32_t Reserved1[2];
  uint32_t CMPCR; // Compensation Cell Control Register
  uint32_t Reserved2[2];
  uint32_t CFGR;
} SYSCFG_RegDef_t;

#define MSYSCFG ((SYSCFG_RegDef_t *)SYSCFG_BASEADDR)

/**************************************         EXTI Peripheral Definitions
 * *********************************************/
typedef struct {
  uint32_t IMR;   // Interrupt Mask Register
  uint32_t EMR;   // Event Mask Register
  uint32_t RTSR;  // Rising Trigger Selection Register
  uint32_t FTSR;  // Falling Trigger Selection Register
  uint32_t SWIER; // Software Interrupt Event Register
  uint32_t PR;    // Pending Register
} EXTI_RegDef_t;

#define MEXTI ((EXTI_RegDef_t *)EXTI_BASEADDR)

/**************************************      USART Register Definitions
 * Structure   ******************************************/

typedef struct {
  uint32_t SR;
  uint32_t DR;
  uint32_t BRR;
  uint32_t CR1;
  uint32_t CR2;
  uint32_t CR3;
  uint32_t GTPR;
} USART_RegDef_t;

/**************************************         USART Peripheral Definitions
 * *********************************************/

#define MUSART1 ((USART_RegDef_t *)USART1_BASEADDR)
#define MUSART2 ((USART_RegDef_t *)USART2_BASEADDR)
#define MUSART3 ((USART_RegDef_t *)USART3_BASEADDR)
#define MUSART4 ((USART_RegDef_t *)USART4_BASEADDR)
#define MUSART5 ((USART_RegDef_t *)USART5_BASEADDR)
#define MUSART6 ((USART_RegDef_t *)USART6_BASEADDR)

/**************************************         TIMER Peripheral Base Adresses
 * ******************************************/
#define TIM1_BASE (0x40010000UL)
#define TIM2_BASE (0x40000000UL)
#define TIM3_BASE (0x40000400UL)
#define TIM4_BASE (0x40000800UL)
#define TIM5_BASE (0x40000C00UL)
#define TIM6_BASE (0x40001000UL)
#define TIM7_BASE (0x40001400UL)
#define TIM8_BASE (0x40010400UL)
#define TIM9_BASE (0x40014000UL)
#define TIM10_BASE (0x40014400UL)
#define TIM11_BASE (0x40014800UL)
#define TIM12_BASE (0x40001800UL)
#define TIM13_BASE (0x40001C00UL)
#define TIM14_BASE (0x40002000UL)

/**************************************      TIMER Register Definitions
 * Structure   ******************************************/
typedef struct {
  volatile uint32_t CR1;   /* TIMx control register 1              */
  volatile uint32_t CR2;   /* TIMx control register 2              */
  volatile uint32_t SMCR;  /* TIMx slave mode control register     */
  volatile uint32_t DIER;  /* TIMx DMA/Interrupt enable register   */
  volatile uint32_t SR;    /* TIMx status register                 */
  volatile uint32_t EGR;   /* TIMx event generation register       */
  volatile uint32_t CCMR1; /* TIMx capture/compare mode register 1 */
  volatile uint32_t CCMR2; /* TIMx capture/compare mode register 2 */
  volatile uint32_t CCER;  /* TIMx capture/compare enable register */
  volatile uint32_t CNT;   /* TIMx counter                         */
  volatile uint32_t PSC;   /* TIMx prescaler                       */
  volatile uint32_t ARR;   /* TIMx auto-reload register            */
  volatile uint32_t RCR;   /* TIMx repetition counter register     */
  volatile uint32_t CCR1;  /* TIMx capture/compare register 1      */
  volatile uint32_t CCR2;  /* TIMx capture/compare register 2      */
  volatile uint32_t CCR3;  /* TIMx capture/compare register 3      */
  volatile uint32_t CCR4;  /* TIMx capture/compare register 4      */
  volatile uint32_t BDTR;  /* TIMx break and dead-time register    */
  volatile uint32_t DCR;   /* TIMx DMA control register            */
  volatile uint32_t DMAR;  /* TIMx DMA address for full transfer   */
  volatile uint32_t OR;    /* TIMx option register                 */
} TIM_TypeDef;

/**************************************         TIMER Peripheral Definitions  *********************************************/
#define TIM1 ((TIM_TypeDef *)TIM1_BASE)
#define TIM2 ((TIM_TypeDef *)TIM2_BASE)
#define TIM3 ((TIM_TypeDef *)TIM3_BASE)
#define TIM4 ((TIM_TypeDef *)TIM4_BASE)
#define TIM5 ((TIM_TypeDef *)TIM5_BASE)
#define TIM6 ((TIM_TypeDef *)TIM6_BASE)
#define TIM7 ((TIM_TypeDef *)TIM7_BASE)
#define TIM8 ((TIM_TypeDef *)TIM8_BASE)
#define TIM9 ((TIM_TypeDef *)TIM9_BASE)
#define TIM10 ((TIM_TypeDef *)TIM10_BASE)
#define TIM11 ((TIM_TypeDef *)TIM11_BASE)
#define TIM12 ((TIM_TypeDef *)TIM12_BASE)
#define TIM13 ((TIM_TypeDef *)TIM13_BASE)
#define TIM14 ((TIM_TypeDef *)TIM14_BASE)

#endif /* STM32F446xx_H */
