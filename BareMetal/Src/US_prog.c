/**
 **===========================================================================**
 **<<<<<<<<<<<<<<<<<<<<<<<<<<      US_prog.c        >>>>>>>>>>>>>>>>>>>>>>>>>>**
 **                                                                           **
 **                  Author : Abdallah Saleh                                  **
 **                  Layer  : HAL                                             **
 **                  SWC    : US (Ultrasonic Distance Sensor - ICU Interrupt) **
 **                                                                           **
 **  Interrupt-driven HC-SR04 driver:                                         **
 **    - Trigger is fired, then the calling task SLEEPS on a semaphore        **
 **      (CPU free) until the Input-Capture ISR delivers both echo edges.     **
 **    - Sequential by design (one active measurement) → no acoustic          **
 **      cross-talk and a trivially-safe single shared state + semaphore.     **
 **                                                                           **
 **  RTOS dependency: this driver uses FreeRTOS (binary semaphore + FromISR). **
 **===========================================================================**
 */

#include <stdint.h>
#include "../Inc/Drivers/LIB/STM32F446xx.h"
#include "../Inc/Drivers/LIB/STD_MACROS.h"
#include "../Inc/Drivers/LIB/ErrTypes.h"
#include "../Inc/Drivers/MCAL/GPIO/GPIO_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_interface.h"
#include "../Inc/Drivers/MCAL/TIM/TIM_private.h"
#include "../Inc/Drivers/MCAL/NVIC/NVIC_interface.h"
#include "../Inc/Drivers/HAL/US/US_interface.h"
#include "../Inc/Drivers/HAL/US/US_config.h"
#include "../Inc/Drivers/HAL/US/US_private.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

static TIM_TypeDef *US_TIM_Array[TIM_TIMER_COUNT] = {TIM2, TIM3, TIM4, TIM5, TIM1, TIM8, TIM6, TIM7};

/* ===================== Interrupt-driven measurement state ===================== */
typedef enum { US_PHASE_RISING = 0, US_PHASE_FALLING, US_PHASE_DONE } US_Phase_t;

/* Exactly ONE measurement is active at a time (sequential reads), so a single
 * shared context + one binary semaphore is race-free. All fields are touched by
 * both the reading task and the ISR → volatile. */
static volatile struct {
    TIM_Num_t  timer;
    uint8_t    channel;     /* 0..3 */
    uint32_t   maxval;      /* timer wrap value (0xFFFF or 0xFFFFFFFF) */
    uint32_t   t1;          /* rising-edge timestamp */
    US_Phase_t phase;
    uint16_t   dist_cm;     /* result, valid when phase == US_PHASE_DONE */
} US_Active;

static SemaphoreHandle_t US_xDoneSem = NULL;  /* given by ISR on completion */

/* Private Function Prototypes */
static void US_vSendTrigger(const US_Config_t *pxSensor);
static void US_CC_Handler(TIM_Num_t Copy_eTimer, uint8_t Copy_u8Channel, uint32_t Copy_u32Capture);
static uint8_t US_u8NvicIrqForTimer(TIM_Num_t Copy_eTimer);

/**************************************         Private Functions
 * ******************************************/

static void US_vSendTrigger(const US_Config_t *pxSensor)
{
    GPIO_enumWritePinVal(pxSensor->TrigPort, pxSensor->TrigPin, GPIO_PIN_LOW);
    TIM_vDelayUs(TIM_TIMER6, 40);

    GPIO_enumWritePinVal(pxSensor->TrigPort, pxSensor->TrigPin, GPIO_PIN_HIGH);
    TIM_vDelayUs(TIM_TIMER6, 20);

    GPIO_enumWritePinVal(pxSensor->TrigPort, pxSensor->TrigPin, GPIO_PIN_LOW);
}

/* Map a US timer to its NVIC IRQ number (US uses general-purpose TIM2..TIM5). */
static uint8_t US_u8NvicIrqForTimer(TIM_Num_t Copy_eTimer)
{
    switch (Copy_eTimer)
    {
        case TIM_TIMER2: return NVIC_TIM2;
        case TIM_TIMER3: return NVIC_TIM3;
        case TIM_TIMER4: return NVIC_TIM4;
        case TIM_TIMER5: return NVIC_TIM5;
        default:         return 0xFFu;   /* unsupported for IC interrupt */
    }
}

/* Input-Capture ISR callback (timer IRQ context). Runs the 2-edge state machine
 * for the single active measurement, then wakes the reading task. */
static void US_CC_Handler(TIM_Num_t Copy_eTimer, uint8_t Copy_u8Channel, uint32_t Copy_u32Capture)
{
    /* Ignore captures that don't belong to the active measurement */
    if (Copy_eTimer != US_Active.timer || Copy_u8Channel != US_Active.channel)
    {
        return;
    }

    if (US_Active.phase == US_PHASE_RISING)
    {
        US_Active.t1    = Copy_u32Capture;
        US_Active.phase = US_PHASE_FALLING;
        /* Now look for the falling edge on the same channel */
        TIM_vSetICPolarity(Copy_eTimer, (TIM_Channel_t)Copy_u8Channel, TIM_POLARITY_LOW);
    }
    else if (US_Active.phase == US_PHASE_FALLING)
    {
        uint32_t high = (Copy_u32Capture >= US_Active.t1)
                          ? (Copy_u32Capture - US_Active.t1)
                          : ((US_Active.maxval - US_Active.t1) + Copy_u32Capture + 1u);

        US_Active.dist_cm = (uint16_t)(high / US_SOUND_SPEED_FACTOR);
        US_Active.phase   = US_PHASE_DONE;

        /* Done — silence this channel and wake the task */
        TIM_vDisableCCInterrupt(Copy_eTimer, (TIM_Channel_t)Copy_u8Channel);

        BaseType_t xHPW = pdFALSE;
        xSemaphoreGiveFromISR(US_xDoneSem, &xHPW);
        portYIELD_FROM_ISR(xHPW);
    }
}

/**************************************         Public Functions
 * ******************************************/

ErrorState_t US_vInit(const US_Config_t *pxSensor)
{
    if (pxSensor == NULL) return NULL_POINTER;
    if (pxSensor->Timer >= TIM_TIMER6) return NOK;   /* basic timers have no IC */

    /* 1. Initialize GPIO Pins */
    GPIO_PinConfig_t TrigCfg = {
        .Port = pxSensor->TrigPort, .PinNum = pxSensor->TrigPin,
        .Mode = GPIO_OUTPUT, .Otype = GPIO_PUSH_PULL,
        .Speed = GPIO_MEDIUM_SPEED, .PullType = GPIO_NO_PULL
    };
    GPIO_enumPinInit(&TrigCfg);

    GPIO_PinConfig_t EchoCfg = {
        .Port = pxSensor->EchoPort, .PinNum = pxSensor->EchoPin,
        .Mode = GPIO_ALTFN, .Otype = GPIO_PUSH_PULL,
        .Speed = GPIO_VERY_HIGH_SPEED, .PullType = GPIO_NO_PULL
    };

    if (pxSensor->Timer == TIM_TIMER1 || pxSensor->Timer == TIM_TIMER2) EchoCfg.AlternateFunction = GPIO_AF1;
    else if (pxSensor->Timer >= TIM_TIMER3 && pxSensor->Timer <= TIM_TIMER5) EchoCfg.AlternateFunction = GPIO_AF2;
    else if (pxSensor->Timer == TIM_TIMER8) EchoCfg.AlternateFunction = GPIO_AF3;
    else return NOK;
    GPIO_enumPinInit(&EchoCfg);

    /* 2. Configure Timer Prescaler for 1us resolution (only if not already running) */
    TIM_TypeDef *TIMx = US_TIM_Array[pxSensor->Timer];

    if (!(TIMx->CR1 & TIM_CR1_CEN)) {
        uint32_t SystemBusClock = US_SYS_CLK_HZ;
        uint16_t Local_u16PSC = (uint16_t)((SystemBusClock / 1000000U) - 1U);

        TIMx->CR1 = 0;
        TIMx->PSC = Local_u16PSC;
        TIMx->ARR = (pxSensor->Timer == TIM_TIMER2 || pxSensor->Timer == TIM_TIMER5) ? 0xFFFFFFFF : 0xFFFF;

        if (pxSensor->Timer == TIM_TIMER1 || pxSensor->Timer == TIM_TIMER8) {
            SET_BIT(TIMx->BDTR, 15); /* MOE: Main Output Enable */
        }

        SET_BIT(TIMx->EGR, 0); /* Force update */
        TIMx->SR = 0;
    }

    /* 3. Configure ICU Channel (rising edge, capture enabled; CC interrupt stays OFF) */
    TIM_ICConfig_t IC_Cfg = {
        .Timer = pxSensor->Timer, .Channel = pxSensor->Channel,
        .Selection = TIM_IC_SELECTION_DIRECT_TI, .Prescaler = TIM_IC_PSC_DIV1,
        .Polarity = TIM_POLARITY_HIGH, .Filter = 2 /* minimal filter for stability */
    };
    TIM_vIC_Init(&IC_Cfg);

    /* 4. Start Timer only if not running */
    if (!(TIMx->CR1 & TIM_CR1_CEN)) {
        TIM_vStart(pxSensor->Timer);
    }

    /* 5. Interrupt infrastructure (idempotent across sensors) */
    if (US_xDoneSem == NULL) {
        US_xDoneSem = xSemaphoreCreateBinary();   /* safe to create before scheduler */
    }
    TIM_vSetCCCallback(pxSensor->Timer, US_CC_Handler);

    uint8_t Local_u8Irq = US_u8NvicIrqForTimer(pxSensor->Timer);
    if (Local_u8Irq != 0xFFu) {
        NVIC_vSetPriority(Local_u8Irq, 6);   /* FreeRTOS-safe (>= configMAX_SYSCALL_INTERRUPT_PRIORITY) */
        NVIC_vEnableIRQ(Local_u8Irq);
    }

    return OK;
}

/**
 * @brief Trigger sensor and measure distance (cm). The calling task SLEEPS on a
 *        semaphore while the echo is in flight (CPU free), then the IC ISR wakes
 *        it with the result. Returns TIMEOUT_STATE if no echo within the window.
 * @note  MUST be called from a task context (uses a FreeRTOS semaphore).
 *        Not reentrant: one measurement at a time (sequential by design).
 */
ErrorState_t US_u16ReadDistance_cm(const US_Config_t *pxSensor, uint16_t *pu16Dist_cm)
{
    if (pxSensor == NULL || pu16Dist_cm == NULL) return NULL_POINTER;
    if (US_xDoneSem == NULL)                      return NOK;   /* US_vInit not done */

    TIM_TypeDef *TIMx = US_TIM_Array[pxSensor->Timer];
    uint8_t      ch   = (uint8_t)pxSensor->Channel;

    /* Drain any stale completion signal left by a previous (late) measurement */
    (void)xSemaphoreTake(US_xDoneSem, 0);

    /* Prepare the active-measurement context */
    US_Active.timer   = pxSensor->Timer;
    US_Active.channel = ch;
    US_Active.maxval  = (pxSensor->Timer == TIM_TIMER2 || pxSensor->Timer == TIM_TIMER5) ? 0xFFFFFFFFu : 0xFFFFu;
    US_Active.phase   = US_PHASE_RISING;

    /* Arm rising edge → clear any stale capture flag → enable CC interrupt */
    TIM_vSetICPolarity(pxSensor->Timer, pxSensor->Channel, TIM_POLARITY_HIGH);
    TIMx->SR = ~(1UL << (ch + 1));   /* clear this channel's CCxIF */
    TIM_vEnableCCInterrupt(pxSensor->Timer, pxSensor->Channel);

    /* Fire the trigger pulse */
    US_vSendTrigger(pxSensor);

    /* Sleep until the ISR delivers both edges, or until the echo window expires */
    if (xSemaphoreTake(US_xDoneSem, pdMS_TO_TICKS(US_TASK_TIMEOUT_MS)) == pdTRUE &&
        US_Active.phase == US_PHASE_DONE)
    {
        *pu16Dist_cm = US_Active.dist_cm;
        return OK;
    }

    /* Timeout: no echo (out of range / no object) — disarm and report */
    TIM_vDisableCCInterrupt(pxSensor->Timer, pxSensor->Channel);
    return TIMEOUT_STATE;
}
