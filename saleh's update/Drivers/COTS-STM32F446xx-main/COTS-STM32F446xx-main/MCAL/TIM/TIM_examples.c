/**
 * @file    TIM_examples.c
 * @author  Abdallah Saleh
 * @brief   Examples for using the Timer (TIM) Driver
 * @details This file contains organized function examples demonstrating how to use
 *          the various features of the Timer driver, including Time Base generation,
 *          PWM output, Interrupts, and Delays.
 */

#include "TIM_interface.h"
#include "../../../MCAL/RCC/Inc/RCC_interface.h"
#include "../../../MCAL/GPIO/Inc/GPIO_interface.h"
#include "../../../MCAL/NVIC/Inc/NVIC_interface.h"

/* Helper function to initialize system clock (Assuming 16MHz HSI) */
void SystemClock_Config(void)
{
    /* Initialize RCC to use HSI */
    RCC_enumSetClkSts(RCC_HSI_CLK, RCC_CLK_ON);
    RCC_enumSetSysClk(RCC_HSI_CLK);
    
    /* Enable TIM2, TIM3, TIM4 Clocks */
    RCC_enumAPB1Config(RCC_APB1_TIM2, RCC_PER_ON);
    RCC_enumAPB1Config(RCC_APB1_TIM3, RCC_PER_ON);
    RCC_enumAPB1Config(RCC_APB1_TIM4, RCC_PER_ON);

    /* Enable GPIOA Clock for PWM Pin (PA6 -> TIM3_CH1) */
    RCC_enumAHB1Config(RCC_AHB1_GPIOA, RCC_PER_ON);
}

/* ========================================================================= */
/*                          1. Time Base Example                             */
/* ========================================================================= */
/**
 * @brief  Configures TIM2 to count up every 1ms with a period of 1 second.
 * @note   System Clock = 16MHz
 *         Prescaler = 16000 - 1  -> Timer Clock = 1KHz (1ms tick)
 *         ARR = 1000 - 1         -> Period = 1000 ticks = 1 second
 */
void Example_TimeBase_Init(void)
{
    TIM_Config_t TimeBaseConfig;
    
    TimeBaseConfig.Timer = TIM_TIMER2;
    TimeBaseConfig.Prescaler = 16000 - 1;       /* 16MHz / 16000 = 1KHz */
    TimeBaseConfig.AutoReloadValue = 1000 - 1;  /* 1000 ticks = 1 sec */
    TimeBaseConfig.Mode = TIM_COUNTERMODE_UP;
    
    TIM_vInit(&TimeBaseConfig);
    
    /* Start the timer */
    TIM_vStart(TIM_TIMER2);
}

/* ========================================================================= */
/*                          2. PWM Output Example                            */
/* ========================================================================= */
/**
 * @brief  Configures TIM3 Channel 1 (PA6) as PWM Output.
 * @note   PA6 must be configured as Alternate Function (AF2 for TIM3).
 *         PWM Frequency = 1KHz, Duty Cycle = 50%
 */
void Example_PWM_Usage(void)
{
    TIM_PWMConfig_t PWMConfig;
    GPIO_PinConfig_t PinConfig;

    /* 1. Configure GPIO Pin PA6 as AF2 (TIM3_CH1) */
    PinConfig.Port = GPIO_PORTA;
    PinConfig.PinNum = GPIO_PIN6;
    PinConfig.Mode = GPIO_ALTFN;
    PinConfig.AlternateFunction = GPIO_AF2;
    PinConfig.Otype = GPIO_PUSH_PULL;
    PinConfig.Speed = GPIO_LOW_SPEED;
    GPIO_enumPinInit(&PinConfig);

    /* 2. Configure PWM */
    PWMConfig.Timer = TIM_TIMER3;
    PWMConfig.Channel = TIM_CHANNEL1;
    PWMConfig.Mode = TIM_PWM_MODE1;
    PWMConfig.Prescaler = 16 - 1;          /* 16MHz / 16 = 1MHz Timer Clock */
    PWMConfig.Period = 1000 - 1;           /* 1MHz / 1000 = 1KHz PWM Freq */
    PWMConfig.DutyCycle = 500;             /* 500/1000 = 50% Duty Cycle */
    PWMConfig.Polarity = TIM_POLARITY_HIGH;

    TIM_vPWM_Init(&PWMConfig);
    
    /* Start the timer to output PWM */
    TIM_vStart(TIM_TIMER3);
    
    /* Change Duty Cycle dynamically */
    TIM_vSetCompareValue(TIM_TIMER3, TIM_CHANNEL1, 750); /* Change to 75% */
}

/* ========================================================================= */
/*                          3. Interrupt Example                             */
/* ========================================================================= */
/**
 * @brief  Configures TIM4 to generate an interrupt every 500ms.
 */
void TIM4_Callback_Function(void)
{
    /* Toggle LED or perform action */
    // GPIO_enumTogPinVal(GPIO_PORTA, GPIO_PIN5);
}

void Example_Interrupt_Usage(void)
{
    TIM_Config_t TimerConfig;
    
    /* Configure Timer: 2KHz clock, 1000 count -> 0.5s period */
    TimerConfig.Timer = TIM_TIMER4;
    TimerConfig.Prescaler = 8000 - 1;       /* 16MHz / 8000 = 2KHz */
    TimerConfig.AutoReloadValue = 1000 - 1; /* 1000 ticks / 2KHz = 0.5s */
    TimerConfig.Mode = TIM_COUNTERMODE_UP;
    
    TIM_vInit(&TimerConfig);
    
    /* Set Callback */
    TIM_vSetCallback(TIM_TIMER4, TIM4_Callback_Function);
    
    /* Enable Timer Interrupt inside Driver */
    TIM_vEnableInterrupt(TIM_TIMER4);
    
    /* Enable NVIC Interrupt for TIM4 */
    NVIC_vEnableIRQ(NVIC_TIM4); // Assuming NVIC driver has this enum
    
    /* Start Timer */
    TIM_vStart(TIM_TIMER4);
}

/* ========================================================================= */
/*                          4. Blocking Delay Example                        */
/* ========================================================================= */
/**
 * @brief  Uses TIM5 to verify blocking delays.
 */
void Example_Delay_Usage(void)
{
    /* Initialize Timer Clock (Required before using delay) */
    RCC_enumAPB1Config(RCC_APB1_TIM5, RCC_PER_ON);

    while (1)
    {
        /* Toggle Pin */
        // GPIO_enumTogPinVal(GPIO_PORTA, GPIO_PIN5);
        
        /* Wait 1 second */
        TIM_vDelayMs(TIM_TIMER5, 1000);
        
        /* Toggle Pin */
        // GPIO_enumTogPinVal(GPIO_PORTA, GPIO_PIN5);

        /* Wait 500 microseconds */
        TIM_vDelayUs(TIM_TIMER5, 500);
    }
}

/* ========================================================================= */
/*                               Main Function                               */
/* ========================================================================= */
int main(void)
{
    /* Initialize System Clock */
    SystemClock_Config();

    /* Uncomment the example you want to run */
    
    // Example_TimeBase_Init();
    // Example_PWM_Usage();
    // Example_Interrupt_Usage();
    Example_Delay_Usage();

    while (1)
    {
    }
}
