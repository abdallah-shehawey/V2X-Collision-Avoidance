/**
 * @file    TIM_interface.h
 * @author  Abdallah Saleh
 * @brief   Timer (TIM) Interface Header File
 * @details This file contains the function prototypes and data types for the Timer driver.
 */

#ifndef TIM_INTERFACE_H_
#define TIM_INTERFACE_H_

#include "../../../LIB/STD_MACROS.h"
#include "../../../LIB/ErrTypes.h"
#include "../../../LIB/STM32F446xx.h"

/**************************************         Data Types
 * ******************************************/

/**
 * @enum TIM_Num_t
 * @brief Enumeration for Timer Instances
 */
typedef enum
{
    TIM_TIMER2 = 0,
    TIM_TIMER3,
    TIM_TIMER4,
    TIM_TIMER5,
    TIM_TIMER1, /* Advanced-Control Timer — Full PWM/IC support. IRQ: TIM1_UP_TIM10 */
    TIM_TIMER8, /* Advanced-Control Timer — Full PWM/IC support. IRQ: TIM8_UP_TIM13 */
    TIM_TIMER6, /* Basic Timer — Delay/Interrupt ONLY. No PWM/IC support */
    TIM_TIMER7, /* Basic Timer — Delay/Interrupt ONLY. No PWM/IC support */
    TIM_TIMER_COUNT /* Number of supported timers */
} TIM_Num_t;

/**
 * @enum TIM_CounterMode_t
 * @brief Enumeration for Timer Counter Modes
 */
typedef enum
{
    TIM_COUNTERMODE_UP = 0,
    TIM_COUNTERMODE_DOWN
} TIM_CounterMode_t;

/**
 * @enum TIM_Channel_t
 * @brief Enumeration for Timer Channels
 */
typedef enum
{
    TIM_CHANNEL1 = 0,
    TIM_CHANNEL2,
    TIM_CHANNEL3,
    TIM_CHANNEL4
} TIM_Channel_t;

/**
 * @enum TIM_PWMMode_t
 * @brief Enumeration for PWM Modes
 */
typedef enum
{
    TIM_PWM_MODE1 = 6, /* Output active as long as CNT < CCRx */
    TIM_PWM_MODE2 = 7  /* Output active as long as CNT > CCRx */
} TIM_PWMMode_t;

/**
 * @enum TIM_Polarity_t
 * @brief Enumeration for Output Polarity
 */
typedef enum
{
    TIM_POLARITY_HIGH = 0,
    TIM_POLARITY_LOW
} TIM_Polarity_t;

/**
 * @struct TIM_Config_t
 * @brief  Configuration structure for Timer Initialization
 */
typedef struct
{
    TIM_Num_t Timer;            /* Timer Instance */
    uint16_t Prescaler;         /* Clock Prescaler (0-65535) */
    uint32_t AutoReloadValue;   /* Auto-Reload Value (Period) */
    TIM_CounterMode_t Mode;     /* Counter Mode (Up/Down) */
} TIM_Config_t;

/**
 * @struct TIM_PWMConfig_t
 * @brief  Configuration structure for PWM Initialization
 */
typedef struct
{
    TIM_Num_t Timer;            /* Timer Instance */
    TIM_Channel_t Channel;      /* PWM Channel */
    TIM_PWMMode_t Mode;         /* PWM Mode */
    uint32_t DutyCycle;         /* Initial Duty Cycle (Compare Value) */
    uint32_t Period;            /* PWM Period (Auto-Reload Value) */
    uint16_t Prescaler;         /* Clock Prescaler */
    TIM_Polarity_t Polarity;    /* Output Polarity */
} TIM_PWMConfig_t;

/**
 * @enum TIM_IC_Selection_t
 * @brief Enumeration for Input Capture Selection
 */
typedef enum
{
    TIM_IC_SELECTION_DIRECT_TI = 1,   /* CCx channel is configured as input, ICx is mapped on TIx */
    TIM_IC_SELECTION_INDIRECT_TI = 2, /* CCx channel is configured as input, ICx is mapped on TIy */
    TIM_IC_SELECTION_TRC = 3          /* CCx channel is configured as input, ICx is mapped on TRC */
} TIM_IC_Selection_t;

/**
 * @enum TIM_IC_Prescaler_t
 * @brief Enumeration for Input Capture Prescaler
 */
typedef enum
{
    TIM_IC_PSC_DIV1 = 0, /* Capture performed each time an edge is detected on the capture input */
    TIM_IC_PSC_DIV2 = 1, /* Capture performed every 2 events */
    TIM_IC_PSC_DIV4 = 2, /* Capture performed every 4 events */
    TIM_IC_PSC_DIV8 = 3  /* Capture performed every 8 events */
} TIM_IC_Prescaler_t;

/**
 * @struct TIM_ICConfig_t
 * @brief  Configuration structure for Input Capture Initialization
 */
typedef struct
{
    TIM_Num_t Timer;               /* Timer Instance */
    TIM_Channel_t Channel;         /* Input Capture Channel */
    TIM_IC_Selection_t Selection;  /* Input Capture Selection */
    TIM_IC_Prescaler_t Prescaler;  /* Input Capture Prescaler */
    TIM_Polarity_t Polarity;       /* Input Capture Polarity (Edge) */
    uint16_t Filter;               /* Input Capture Filter (0x0 to 0xF) */
} TIM_ICConfig_t;

/**************************************         Function Prototypes
 * ******************************************/

/**
 * @fn      TIM_vInit
 * @brief   Initialize the Timer with the specified configuration.
 * @param   pxConfig: Pointer to the configuration structure.
 * @return  ErrorState_t: OK if successful, NOK if invalid parameters.
 * @example
 * TIM_Config_t myTimer = {TIM_TIMER2, 16000-1, 1000-1, TIM_COUNTERMODE_UP};
 * TIM_vInit(&myTimer); // 16MHz clock -> 1KHz tick, 1s period
 */
ErrorState_t TIM_vInit(const TIM_Config_t *pxConfig);

/**
 * @fn      TIM_vStart
 * @brief   Start the Timer counter.
 * @param   Copy_eTimer: Timer Instance.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vStart(TIM_TIMER2);
 */
ErrorState_t TIM_vStart(TIM_Num_t Copy_eTimer);

/**
 * @fn      TIM_vStop
 * @brief   Stop the Timer counter.
 * @param   Copy_eTimer: Timer Instance.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vStop(TIM_TIMER2);
 */
ErrorState_t TIM_vStop(TIM_Num_t Copy_eTimer);

/**
 * @fn      TIM_vPWM_Init
 * @brief   Initialize a PWM channel.
 * @param   pxPWMConfig: Pointer to the PWM configuration structure.
 * @return  ErrorState_t: OK if successful.
 * @example
 * TIM_PWMConfig_t myPWM = {TIM_TIMER3, TIM_CHANNEL1, TIM_PWM_MODE1, 500, 1000, 16-1, TIM_POLARITY_HIGH};
 * TIM_vPWM_Init(&myPWM);
 */
 * TIM_vPWM_Init(&myPWM);
 */
ErrorState_t TIM_vPWM_Init(const TIM_PWMConfig_t *pxPWMConfig);

/**
 * @fn      TIM_vIC_Init
 * @brief   Initialize an Input Capture channel.
 * @param   pxICConfig: Pointer to the Input Capture configuration structure.
 * @return  ErrorState_t: OK if successful.
 * @example
 * TIM_ICConfig_t myIC = {TIM_TIMER2, TIM_CHANNEL1, TIM_IC_SELECTION_DIRECT_TI, TIM_IC_PSC_DIV1, TIM_POLARITY_HIGH, 0};
 * TIM_vIC_Init(&myIC);
 */
ErrorState_t TIM_vIC_Init(const TIM_ICConfig_t *pxICConfig);

/**
 * @fn      TIM_u32GetCaptureValue
 * @brief   Get the captured value from the CCR register.
 * @param   Copy_eTimer: Timer Instance.
 * @param   Copy_eChannel: Timer Channel.
 * @param   pu32Value: Pointer to store the captured value.
 * @return  ErrorState_t: OK if successful.
 * @example
 * uint32_t val;
 * TIM_u32GetCaptureValue(TIM_TIMER2, TIM_CHANNEL1, &val);
 */
ErrorState_t TIM_u32GetCaptureValue(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, uint32_t *pu32Value);

/**
 * @fn     TIM_vSetICPolarity
 * @brief  Set/Update Input Capture Polarity (Edge).
 * @param  Copy_eTimer: Timer Instance.
 * @param  Copy_eChannel: Timer Channel.
 * @param  Copy_ePolarity: New Polarity (Rising/Falling/Both).
 * @return ErrorState_t: OK if successful.
 * @example TIM_vSetICPolarity(TIM_TIMER2, TIM_CHANNEL1, TIM_POLARITY_LOW);
 */
ErrorState_t TIM_vSetICPolarity(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, TIM_Polarity_t Copy_ePolarity);


/**
 * @fn      TIM_vSetCompareValue
 * @brief   Set the Compare Value (Duty Cycle) for a specific channel.
 * @param   Copy_eTimer: Timer Instance.
 * @param   Copy_eChannel: Timer Channel.
 * @param   Copy_u32Value: New Compare Value.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vSetCompareValue(TIM_TIMER3, TIM_CHANNEL1, 750); // 75% duty cycle if defined period is 1000
 */
ErrorState_t TIM_vSetCompareValue(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, uint32_t Copy_u32Value);

/**
 * @fn      TIM_vSetPrescaler
 * @brief   Set the Prescaler value dynamically.
 * @param   Copy_eTimer: Timer Instance.
 * @param   Copy_u16Prescaler: New Prescaler Value.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vSetPrescaler(TIM_TIMER2, 7999);
 */
ErrorState_t TIM_vSetPrescaler(TIM_Num_t Copy_eTimer, uint16_t Copy_u16Prescaler);

/**
 * @fn      TIM_vSetAutoReloadValue
 * @brief   Set the Auto-Reload Value (Period) dynamically.
 * @param   Copy_eTimer: Timer Instance.
 * @param   Copy_u32AutoReload: New Auto-Reload Value.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vSetAutoReloadValue(TIM_TIMER2, 4999);
 */
ErrorState_t TIM_vSetAutoReloadValue(TIM_Num_t Copy_eTimer, uint32_t Copy_u32AutoReload);

/**
 * @fn      TIM_vDelayMs
 * @brief   Blocking Delay in Milliseconds using a specific Timer.
 * @param   Copy_eTimer: Timer Instance to use for delay.
 * @param   Copy_u32Ms: Delay in milliseconds.
 * @return  ErrorState_t: OK if successful.
 * @note    This function reconfigures the timer. Do not use if Timer is being used for other purposes.
 * @example TIM_vDelayMs(TIM_TIMER2, 1000);
 */
ErrorState_t TIM_vDelayMs(TIM_Num_t Copy_eTimer, uint32_t Copy_u32Ms);

/**
 * @fn      TIM_vDelayUs
 * @brief   Blocking Delay in Microseconds using a specific Timer.
 * @param   Copy_eTimer: Timer Instance to use for delay.
 * @param   Copy_u32Us: Delay in microseconds.
 * @return  ErrorState_t: OK if successful.
 * @note    This function reconfigures the timer. Do not use if Timer is being used for other purposes.
 * @example TIM_vDelayUs(TIM_TIMER2, 500);
 */
ErrorState_t TIM_vDelayUs(TIM_Num_t Copy_eTimer, uint32_t Copy_u32Us);

/**
 * @fn      TIM_vEnableInterrupt
 * @brief   Enable Timer Interrupt.
 * @param   Copy_eTimer: Timer Instance.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vEnableInterrupt(TIM_TIMER2);
 */
ErrorState_t TIM_vEnableInterrupt(TIM_Num_t Copy_eTimer);

/**
 * @fn      TIM_vDisableInterrupt
 * @brief   Disable Timer Interrupt.
 * @param   Copy_eTimer: Timer Instance.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vDisableInterrupt(TIM_TIMER2);
 */
ErrorState_t TIM_vDisableInterrupt(TIM_Num_t Copy_eTimer);

/**
 * @fn      TIM_vSetCallback
 * @brief   Set the callback function for Timer Interrupt.
 * @param   Copy_eTimer: Timer Instance.
 * @param   pvCallback: Pointer to the callback function.
 * @return  ErrorState_t: OK if successful.
 * @example TIM_vSetCallback(TIM_TIMER2, MyISR);
 */
ErrorState_t TIM_vSetCallback(TIM_Num_t Copy_eTimer, void (*pvCallback)(void));

#endif /* TIM_INTERFACE_H_ */
