/**
 * @file    TIM_program.c
 * @author  Abdallah Saleh
 * @brief   Timer (TIM) Program File
 * @details This file contains the implementation of the Timer driver functions.
 */

#include "../Inc/Drivers/LIB/STM32F446xx.h"
#include "../Inc/Drivers/LIB/STD_MACROS.h"
#include "../Inc/Drivers/LIB/ErrTypes.h"

#include "../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_private.h"

/**************************************         Private Variables
 * ******************************************/

static TIM_TypeDef *TIM_Instances[TIM_TIMER_COUNT] = {TIM2, TIM3, TIM4, TIM5, TIM1, TIM8, TIM6, TIM7};
static void (*TIM_Callbacks[TIM_TIMER_COUNT])(void) = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

/* Define Default Clock if not defined */
#ifndef TIM_CLOCK_FREQ
#define TIM_CLOCK_FREQ 16000000UL
#endif

/**************************************         Function Definitions
 * ******************************************/

/**
 * @fn      TIM_vInit
 * @brief   Initialize the Timer with the specified configuration.
 */
ErrorState_t TIM_vInit(const TIM_Config_t *pxConfig)
{
    ErrorState_t Local_ErrorState = OK;

    if (pxConfig == NULL)
    {
        Local_ErrorState = NULL_POINTER;
    }
    else if (pxConfig->Timer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        TIM_TypeDef *TIMx = TIM_Instances[pxConfig->Timer];

        /* Disable Timer during configuration */
        CLR_BIT(TIMx->CR1, TIM_CR1_CEN);

        /* Set Prescaler */
        TIMx->PSC = pxConfig->Prescaler;

        /* Set Auto-Reload Value */
        TIMx->ARR = pxConfig->AutoReloadValue;

        /* Set Counter Mode */
        if (pxConfig->Mode == TIM_COUNTERMODE_UP)
        {
            CLR_BIT(TIMx->CR1, TIM_CR1_DIR);
        }
        else
        {
            SET_BIT(TIMx->CR1, TIM_CR1_DIR);
        }

        /* Generate Update Event to load PSC and ARR immediately */
        SET_BIT(TIMx->EGR, TIM_EGR_UG);
        /* Clear Update Interrupt Flag to avoid immediate interrupt */
        CLR_BIT(TIMx->SR, TIM_SR_UIF);
    }

    return Local_ErrorState;
}

/**
 * @fn      TIM_vStart
 * @brief   Start the Timer counter.
 */
ErrorState_t TIM_vStart(TIM_Num_t Copy_eTimer)
{
    ErrorState_t Local_ErrorState = OK;

    if (Copy_eTimer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        SET_BIT(TIM_Instances[Copy_eTimer]->CR1, TIM_CR1_CEN);
    }

    return Local_ErrorState;
}

/**
 * @fn      TIM_vStop
 * @brief   Stop the Timer counter.
 */
ErrorState_t TIM_vStop(TIM_Num_t Copy_eTimer)
{
    ErrorState_t Local_ErrorState = OK;

    if (Copy_eTimer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        CLR_BIT(TIM_Instances[Copy_eTimer]->CR1, TIM_CR1_CEN);
    }

    return Local_ErrorState;
}

/**
 * @fn      TIM_vPWM_Init
 * @brief   Initialize a PWM channel.
 */
ErrorState_t TIM_vPWM_Init(const TIM_PWMConfig_t *pxPWMConfig)
{
    ErrorState_t Local_ErrorState = OK;

    if (pxPWMConfig == NULL)
    {
        Local_ErrorState = NULL_POINTER;
    }
    else if (pxPWMConfig->Timer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        TIM_TypeDef *TIMx = TIM_Instances[pxPWMConfig->Timer];

        /* Disable Timer */
        CLR_BIT(TIMx->CR1, TIM_CR1_CEN);

        /* Set Prescaler and Period */
        TIMx->PSC = pxPWMConfig->Prescaler;
        TIMx->ARR = pxPWMConfig->Period;

        /* Configure Channel */
        switch (pxPWMConfig->Channel)
        {
        case TIM_CHANNEL1:
            /* CC1S = 00 (Output) */
            TIMx->CCMR1 &= ~(0x3 << TIM_CCMR1_CC1S);
            /* OC1M = Mode (PWM1 or PWM2) */
            TIMx->CCMR1 &= ~(0x7 << TIM_CCMR1_OC1M);
            TIMx->CCMR1 |= (pxPWMConfig->Mode << TIM_CCMR1_OC1M);
            /* Enable Preload */
            SET_BIT(TIMx->CCMR1, TIM_CCMR1_OC1PE);
            /* Set Compare Value */
            TIMx->CCR1 = pxPWMConfig->DutyCycle;
            /* Polarity */
            if (pxPWMConfig->Polarity == TIM_POLARITY_LOW)
                SET_BIT(TIMx->CCER, TIM_CCER_CC1P);
            else
                CLR_BIT(TIMx->CCER, TIM_CCER_CC1P);
            /* Enable Output */
            SET_BIT(TIMx->CCER, TIM_CCER_CC1E);
            break;

        case TIM_CHANNEL2:
            TIMx->CCMR1 &= ~(0x3 << TIM_CCMR1_CC2S);
            TIMx->CCMR1 &= ~(0x7 << TIM_CCMR1_OC2M);
            TIMx->CCMR1 |= (pxPWMConfig->Mode << TIM_CCMR1_OC2M);
            SET_BIT(TIMx->CCMR1, TIM_CCMR1_OC2PE);
            TIMx->CCR2 = pxPWMConfig->DutyCycle;
            if (pxPWMConfig->Polarity == TIM_POLARITY_LOW)
                SET_BIT(TIMx->CCER, TIM_CCER_CC2P);
            else
                CLR_BIT(TIMx->CCER, TIM_CCER_CC2P);
            SET_BIT(TIMx->CCER, TIM_CCER_CC2E);
            break;

        case TIM_CHANNEL3:
            TIMx->CCMR2 &= ~(0x3 << TIM_CCMR2_CC3S);
            TIMx->CCMR2 &= ~(0x7 << TIM_CCMR2_OC3M);
            TIMx->CCMR2 |= (pxPWMConfig->Mode << TIM_CCMR2_OC3M);
            SET_BIT(TIMx->CCMR2, TIM_CCMR2_OC3PE);
            TIMx->CCR3 = pxPWMConfig->DutyCycle;
            if (pxPWMConfig->Polarity == TIM_POLARITY_LOW)
                SET_BIT(TIMx->CCER, TIM_CCER_CC3P);
            else
                CLR_BIT(TIMx->CCER, TIM_CCER_CC3P);
            SET_BIT(TIMx->CCER, TIM_CCER_CC3E);
            break;

        case TIM_CHANNEL4:
            TIMx->CCMR2 &= ~(0x3 << TIM_CCMR2_CC4S);
            TIMx->CCMR2 &= ~(0x7 << TIM_CCMR2_OC4M);
            TIMx->CCMR2 |= (pxPWMConfig->Mode << TIM_CCMR2_OC4M);
            SET_BIT(TIMx->CCMR2, TIM_CCMR2_OC4PE);
            TIMx->CCR4 = pxPWMConfig->DutyCycle;
            if (pxPWMConfig->Polarity == TIM_POLARITY_LOW)
                SET_BIT(TIMx->CCER, TIM_CCER_CC4P);
            else
                CLR_BIT(TIMx->CCER, TIM_CCER_CC4P);
            SET_BIT(TIMx->CCER, TIM_CCER_CC4E);
            break;
        }

        /* Auto-reload Preload Enable */
        SET_BIT(TIMx->CR1, TIM_CR1_ARPE);

        /* Generate Update Event */
        SET_BIT(TIMx->EGR, TIM_EGR_UG);
    }

    return Local_ErrorState;
}

/**
 * @fn      TIM_vIC_Init
 * @brief   Initialize an Input Capture channel.
 */
ErrorState_t TIM_vIC_Init(const TIM_ICConfig_t *pxICConfig)
{
    ErrorState_t Local_ErrorState = OK;

    if (pxICConfig == NULL)
    {
        Local_ErrorState = NULL_POINTER;
    }
    else if (pxICConfig->Timer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        TIM_TypeDef *TIMx = TIM_Instances[pxICConfig->Timer];

        /* Disable Timer */
        CLR_BIT(TIMx->CR1, TIM_CR1_CEN);

        switch (pxICConfig->Channel)
        {
        case TIM_CHANNEL1:
            /* CC1S = Selection (Direct/Indirect/TRC) */
            TIMx->CCMR1 &= ~(0x3 << TIM_CCMR1_CC1S);
            TIMx->CCMR1 |= (pxICConfig->Selection << TIM_CCMR1_CC1S);
            /* IC1F = Filter */
            TIMx->CCMR1 &= ~(0xF << 4); /* Filter bits at offset 4 */
            TIMx->CCMR1 |= (pxICConfig->Filter << 4);
            /* IC1PSC = Prescaler */
            TIMx->CCMR1 &= ~(0x3 << 2); /* Prescaler bits at offset 2 */
            TIMx->CCMR1 |= (pxICConfig->Prescaler << 2);
            /* CC1P/CC1NP = Polarity */
            CLR_BIT(TIMx->CCER, TIM_CCER_CC1P);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC1NP);
            if (pxICConfig->Polarity == TIM_POLARITY_LOW)
            {
                SET_BIT(TIMx->CCER, TIM_CCER_CC1P);
            }
            /* Enable Capture */
            SET_BIT(TIMx->CCER, TIM_CCER_CC1E);
            break;

        case TIM_CHANNEL2:
            TIMx->CCMR1 &= ~(0x3 << TIM_CCMR1_CC2S);
            TIMx->CCMR1 |= (pxICConfig->Selection << TIM_CCMR1_CC2S);
            TIMx->CCMR1 &= ~(0xF << 12);
            TIMx->CCMR1 |= (pxICConfig->Filter << 12);
            TIMx->CCMR1 &= ~(0x3 << 10);
            TIMx->CCMR1 |= (pxICConfig->Prescaler << 10);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC2P);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC2NP);
            if (pxICConfig->Polarity == TIM_POLARITY_LOW)
            {
                SET_BIT(TIMx->CCER, TIM_CCER_CC2P);
            }
            SET_BIT(TIMx->CCER, TIM_CCER_CC2E);
            break;

        case TIM_CHANNEL3:
            TIMx->CCMR2 &= ~(0x3 << TIM_CCMR2_CC3S);
            TIMx->CCMR2 |= (pxICConfig->Selection << TIM_CCMR2_CC3S);
            TIMx->CCMR2 &= ~(0xF << 4);
            TIMx->CCMR2 |= (pxICConfig->Filter << 4);
            TIMx->CCMR2 &= ~(0x3 << 2);
            TIMx->CCMR2 |= (pxICConfig->Prescaler << 2);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC3P);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC3NP);
            if (pxICConfig->Polarity == TIM_POLARITY_LOW)
            {
                SET_BIT(TIMx->CCER, TIM_CCER_CC3P);
            }
            SET_BIT(TIMx->CCER, TIM_CCER_CC3E);
            break;

        case TIM_CHANNEL4:
            TIMx->CCMR2 &= ~(0x3 << TIM_CCMR2_CC4S);
            TIMx->CCMR2 |= (pxICConfig->Selection << TIM_CCMR2_CC4S);
            TIMx->CCMR2 &= ~(0xF << 12);
            TIMx->CCMR2 |= (pxICConfig->Filter << 12);
            TIMx->CCMR2 &= ~(0x3 << 10);
            TIMx->CCMR2 |= (pxICConfig->Prescaler << 10);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC4P);
            CLR_BIT(TIMx->CCER, TIM_CCER_CC4NP);
            if (pxICConfig->Polarity == TIM_POLARITY_LOW)
            {
                SET_BIT(TIMx->CCER, TIM_CCER_CC4P);
            }
            SET_BIT(TIMx->CCER, TIM_CCER_CC4E);
            break;
        }
    }
    return Local_ErrorState;
}

ErrorState_t TIM_u32GetCaptureValue(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, uint32_t *pu32Value)
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT || pu32Value == NULL) return NOK;

    TIM_TypeDef *TIMx = TIM_Instances[Copy_eTimer];
    switch (Copy_eChannel)
    {
    case TIM_CHANNEL1: *pu32Value = TIMx->CCR1; break;
    case TIM_CHANNEL2: *pu32Value = TIMx->CCR2; break;
    case TIM_CHANNEL3: *pu32Value = TIMx->CCR3; break;
    case TIM_CHANNEL4: *pu32Value = TIMx->CCR4; break;
    default: Local_ErrorState = NOK; break;
    }
    return Local_ErrorState;
}

ErrorState_t TIM_vSetICPolarity(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, TIM_Polarity_t Copy_ePolarity)
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT) return NOK;

    TIM_TypeDef *TIMx = TIM_Instances[Copy_eTimer];
    switch (Copy_eChannel)
    {
    case TIM_CHANNEL1:
        if (Copy_ePolarity == TIM_POLARITY_LOW) SET_BIT(TIMx->CCER, TIM_CCER_CC1P);
        else CLR_BIT(TIMx->CCER, TIM_CCER_CC1P);
        break;
    case TIM_CHANNEL2:
        if (Copy_ePolarity == TIM_POLARITY_LOW) SET_BIT(TIMx->CCER, TIM_CCER_CC2P);
        else CLR_BIT(TIMx->CCER, TIM_CCER_CC2P);
        break;
    case TIM_CHANNEL3:
        if (Copy_ePolarity == TIM_POLARITY_LOW) SET_BIT(TIMx->CCER, TIM_CCER_CC3P);
        else CLR_BIT(TIMx->CCER, TIM_CCER_CC3P);
        break;
    case TIM_CHANNEL4:
        if (Copy_ePolarity == TIM_POLARITY_LOW) SET_BIT(TIMx->CCER, TIM_CCER_CC4P);
        else CLR_BIT(TIMx->CCER, TIM_CCER_CC4P);
        break;
    default: Local_ErrorState = NOK; break;
    }
    return Local_ErrorState;
}

/**
 * @fn      TIM_vSetCompareValue
 * @brief   Set the Compare Value.
 */
ErrorState_t TIM_vSetCompareValue(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, uint32_t Copy_u32Value)
{
    ErrorState_t Local_ErrorState = OK;

    if (Copy_eTimer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        TIM_TypeDef *TIMx = TIM_Instances[Copy_eTimer];
        switch (Copy_eChannel)
        {
        case TIM_CHANNEL1: TIMx->CCR1 = Copy_u32Value; break;
        case TIM_CHANNEL2: TIMx->CCR2 = Copy_u32Value; break;
        case TIM_CHANNEL3: TIMx->CCR3 = Copy_u32Value; break;
        case TIM_CHANNEL4: TIMx->CCR4 = Copy_u32Value; break;
        default: Local_ErrorState = NOK; break;
        }
    }
    return Local_ErrorState;
}

/**
 * @fn      TIM_vSetPrescaler
 * @brief   Set/Update Prescaler.
 */
ErrorState_t TIM_vSetPrescaler(TIM_Num_t Copy_eTimer, uint16_t Copy_u16Prescaler)
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT) return NOK;
    TIM_Instances[Copy_eTimer]->PSC = Copy_u16Prescaler;
    return Local_ErrorState;
}

/**
 * @fn      TIM_vSetAutoReloadValue
 * @brief   Set/Update ARR.
 */
ErrorState_t TIM_vSetAutoReloadValue(TIM_Num_t Copy_eTimer, uint32_t Copy_u32AutoReload)
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT) return NOK;
    TIM_Instances[Copy_eTimer]->ARR = Copy_u32AutoReload;
    return Local_ErrorState;
}

/**
 * @fn      TIM_vDelayMs
 * @brief   Blocking Delay in ms.
 * @note    Assumes 16MHz clock if not configured.
 */
ErrorState_t TIM_vDelayMs(TIM_Num_t Copy_eTimer, uint32_t Copy_u32Ms)
{
    ErrorState_t Local_ErrorState = OK;

    if (Copy_eTimer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        TIM_TypeDef *TIMx = TIM_Instances[Copy_eTimer];
        uint32_t Local_u32Prescaler = (TIM_CLOCK_FREQ / 1000) - 1; /* 1KHz Timer Frequency */
        
        /* Configure Timer for 1ms tick */
        TIMx->PSC = Local_u32Prescaler;
        TIMx->ARR = Copy_u32Ms; /* Set Auto-Reload to Delay Ms */
        TIMx->CNT = 0;
        
        /* Clear Update Flag and Disable Interrupts for this timer during blocking delay */
        CLR_BIT(TIMx->SR, TIM_SR_UIF);
        CLR_BIT(TIMx->DIER, TIM_DIER_UIE);
        
        /* Enable Timer */
        SET_BIT(TIMx->CR1, TIM_CR1_CEN);
        
        /* Wait for Update Flag */
        while (READ_BIT(TIMx->SR, TIM_SR_UIF) == 0);
        
        /* Disable Timer */
        CLR_BIT(TIMx->CR1, TIM_CR1_CEN);
        CLR_BIT(TIMx->SR, TIM_SR_UIF);
    }
    return Local_ErrorState;
}

/**
 * @fn      TIM_vDelayUs
 * @brief   Blocking Delay in us.
 */
ErrorState_t TIM_vDelayUs(TIM_Num_t Copy_eTimer, uint32_t Copy_u32Us)
{
    ErrorState_t Local_ErrorState = OK;

    if (Copy_eTimer >= TIM_TIMER_COUNT)
    {
        Local_ErrorState = NOK;
    }
    else
    {
        TIM_TypeDef *TIMx = TIM_Instances[Copy_eTimer];
        uint32_t Local_u32Prescaler = (TIM_CLOCK_FREQ / 1000000) - 1; /* 1MHz Timer Frequency */
        
        /* Configure Timer for 1us tick */
        TIMx->PSC = Local_u32Prescaler;
        TIMx->ARR = Copy_u32Us;
        TIMx->CNT = 0;
        
        CLR_BIT(TIMx->SR, TIM_SR_UIF);
        CLR_BIT(TIMx->DIER, TIM_DIER_UIE);
        SET_BIT(TIMx->CR1, TIM_CR1_CEN);
        
        while (READ_BIT(TIMx->SR, TIM_SR_UIF) == 0);
        
        CLR_BIT(TIMx->CR1, TIM_CR1_CEN);
        CLR_BIT(TIMx->SR, TIM_SR_UIF);
    }
    return Local_ErrorState;
}

ErrorState_t TIM_vEnableInterrupt(TIM_Num_t Copy_eTimer)
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT) return NOK;
    SET_BIT(TIM_Instances[Copy_eTimer]->DIER, TIM_DIER_UIE);
    return Local_ErrorState;
}

ErrorState_t TIM_vDisableInterrupt(TIM_Num_t Copy_eTimer)
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT) return NOK;
    CLR_BIT(TIM_Instances[Copy_eTimer]->DIER, TIM_DIER_UIE);
    return Local_ErrorState;
}

ErrorState_t TIM_vSetCallback(TIM_Num_t Copy_eTimer, void (*pvCallback)(void))
{
    ErrorState_t Local_ErrorState = OK;
    if (Copy_eTimer >= TIM_TIMER_COUNT || pvCallback == NULL) return NOK;
    TIM_Callbacks[Copy_eTimer] = pvCallback;
    return Local_ErrorState;
}

void TIM2_IRQHandler(void)
{
    if (READ_BIT(TIM2->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM2->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER2] != NULL)
        {
            TIM_Callbacks[TIM_TIMER2]();
        }
    }
}

void TIM3_IRQHandler(void)
{
    if (READ_BIT(TIM3->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM3->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER3] != NULL)
        {
            TIM_Callbacks[TIM_TIMER3]();
        }
    }
}

void TIM4_IRQHandler(void)
{
    if (READ_BIT(TIM4->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM4->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER4] != NULL)
        {
            TIM_Callbacks[TIM_TIMER4]();
        }
    }
}

void TIM5_IRQHandler(void)
{
    if (READ_BIT(TIM5->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM5->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER5] != NULL)
        {
            TIM_Callbacks[TIM_TIMER5]();
        }
    }
}

/**
 * @brief TIM1 Update IRQ Handler
 * @note  On STM32F446xx, TIM1 Update shares its IRQ line with TIM10.
 *        The correct handler name is TIM1_UP_TIM10_IRQHandler.
 *        Enable with NVIC IRQ: TIM1_UP_TIM10_IRQn
 * @note  Advanced-control timer: set MOE bit (BDTR register) if using PWM output.
 */
void TIM1_UP_TIM10_IRQHandler(void)
{
    if (READ_BIT(TIM1->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM1->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER1] != NULL)
        {
            TIM_Callbacks[TIM_TIMER1]();
        }
    }
}

/**
 * @brief TIM8 Update IRQ Handler
 * @note  On STM32F446xx, TIM8 Update shares its IRQ line with TIM13.
 *        The correct handler name is TIM8_UP_TIM13_IRQHandler.
 *        Enable with NVIC IRQ: TIM8_UP_TIM13_IRQn
 * @note  Advanced-control timer: set MOE bit (BDTR register) if using PWM output.
 */
void TIM8_UP_TIM13_IRQHandler(void)
{
    if (READ_BIT(TIM8->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM8->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER8] != NULL)
        {
            TIM_Callbacks[TIM_TIMER8]();
        }
    }
}

/**
 * @brief TIM6 / DAC IRQ Handler
 * @note  TIM6 shares its IRQ line with the DAC on STM32F446xx.
 *        The IRQ name is TIM6_DAC_IRQHandler.
 */
void TIM6_DAC_IRQHandler(void)
{
    if (READ_BIT(TIM6->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM6->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER6] != NULL)
        {
            TIM_Callbacks[TIM_TIMER6]();
        }
    }
}

/**
 * @brief TIM7 IRQ Handler
 */
void TIM7_IRQHandler(void)
{
    if (READ_BIT(TIM7->SR, TIM_SR_UIF))
    {
        CLR_BIT(TIM7->SR, TIM_SR_UIF);
        if (TIM_Callbacks[TIM_TIMER7] != NULL)
        {
            TIM_Callbacks[TIM_TIMER7]();
        }
    }
}
