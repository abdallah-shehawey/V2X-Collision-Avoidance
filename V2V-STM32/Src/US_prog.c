/**
 ******************************************************************************
 * @file    US_prog.c
 * @author  Abdallah Abdelmoemen Shehawey
 * @brief   Implementation of the US driver — the HC-SR04 ultrasonic rangefinders.
 * @ingroup hal_us
 ******************************************************************************
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

/**
 * @brief Maps a @ref TIM_Num_t onto its register block.
 * @note Order must match @ref TIM_Num_t — the enum indexes straight into it.
 */
static TIM_TypeDef *US_TIM_Array[TIM_TIMER_COUNT] = { TIM2, TIM3, TIM4, TIM5, TIM1, TIM8, TIM6, TIM7 };

/* ===================== Interrupt-driven measurement state ===================== */
/**
 * @brief Where a single echo measurement has got to.
 *
 * An HC-SR04 reports distance as the *width* of its echo pulse, so timing it takes
 * two interrupts: one on the rising edge to start the clock, one on the falling
 * edge to stop it. This enum is what the ISR uses to know which of the two it is
 * looking at.
 */
typedef enum
{
  US_PHASE_RISING = 0, /**< Waiting for the rising edge — the echo has not started yet. */
  US_PHASE_FALLING,    /**< Rising edge seen and timestamped; waiting for the falling edge. */
  US_PHASE_DONE        /**< Both edges seen; the distance is ready. */
} US_Phase_t;

/**
 * @brief The one measurement currently in flight.
 *
 * There is deliberately only one. The six sensors are read strictly one after
 * another — partly to avoid acoustic cross-talk, where one sensor hears another's
 * ping and reports a phantom object, and partly because it makes a single shared
 * context and one binary semaphore sufficient. No per-sensor state, and no race.
 *
 * Every field is touched by both the reading task and the capture ISR, hence
 * `volatile`.
 */
static volatile struct
{
  TIM_Num_t  timer;   /**< Timer doing the capture for the active sensor. */
  uint8_t    channel; /**< Its capture/compare channel, 0..3. */
  uint32_t   maxval;  /**< The timer's wrap value (0xFFFF or 0xFFFFFFFF), needed to unwrap a counter that rolled over mid-echo. */
  uint32_t   t1;      /**< Counter value captured on the rising edge. */
  US_Phase_t phase;   /**< Which edge we are waiting for. */
  uint8_t    valid;   /**< 1 if a real echo was timed; 0 if it was out of range or garbage. */
  uint16_t   dist_cm; /**< The result [cm]; meaningful only once `phase == US_PHASE_DONE` and `valid` is 1. */
} US_Active;

/**
 * @brief Given by the capture ISR when a measurement completes.
 *
 * This is what lets the reading task **sleep** through the echo's flight time
 * instead of busy-waiting on a flag. An echo from a distant object can take
 * tens of milliseconds; spending that spinning would starve every lower-priority
 * task in the system.
 */
static SemaphoreHandle_t US_xDoneSem = NULL;

/* Private Function Prototypes */
/**
 * @brief Fire the trigger pulse that starts one sensor's measurement.
 *
 * Holds TRIG low for @ref US_TRIG_SETTLE_US to guarantee a clean edge, then high
 * for @ref US_TRIG_PULSE_US — the minimum the HC-SR04 datasheet demands.
 *
 * @param[in] pxSensor The sensor to trigger.
 */
static void    US_vSendTrigger(const US_Config_t *pxSensor);
static void    US_CC_Handler(TIM_Num_t Copy_eTimer, uint8_t Copy_u8Channel, uint32_t Copy_u32Capture);
static uint8_t US_u8NvicIrqForTimer(TIM_Num_t Copy_eTimer);

/**************************************         Private Functions
 * ******************************************/

static void US_vSendTrigger(const US_Config_t *pxSensor)
{
  /* Settle LOW for a clean edge, then a 10us HIGH pulse (HC-SR04 datasheet). */
  GPIO_enumWritePinVal(pxSensor->TrigPort, pxSensor->TrigPin, GPIO_PIN_LOW);
  TIM_vDelayUs(TIM_TIMER6, US_TRIG_SETTLE_US);

  GPIO_enumWritePinVal(pxSensor->TrigPort, pxSensor->TrigPin, GPIO_PIN_HIGH);
  TIM_vDelayUs(TIM_TIMER6, US_TRIG_PULSE_US);

  GPIO_enumWritePinVal(pxSensor->TrigPort, pxSensor->TrigPin, GPIO_PIN_LOW);
}

/**
 * @brief Map one of the ultrasonic timers to its NVIC IRQ number.
 * @param Copy_eTimer The timer (the ultrasonics only use TIM2..TIM5).
 * @return The matching @ref NVIC_IRQNumber_t value.
 */
static uint8_t US_u8NvicIrqForTimer(TIM_Num_t Copy_eTimer)
{
  switch (Copy_eTimer)
  {
  case TIM_TIMER2:
    return NVIC_TIM2;
  case TIM_TIMER3:
    return NVIC_TIM3;
  case TIM_TIMER4:
    return NVIC_TIM4;
  case TIM_TIMER5:
    return NVIC_TIM5;
  default:
    return 0xFFu; /* unsupported for IC interrupt */
  }
}

/**
 * @brief Input-capture callback — runs the two-edge state machine for the active echo.
 *
 * Called from timer IRQ context on each captured edge. On the rising edge it
 * timestamps the start; on the falling edge it works out the pulse width, converts
 * it to a distance via @ref US_SOUND_SPEED_FACTOR, clamps it against
 * @ref US_MAX_RANGE_CM, and gives @ref US_xDoneSem to wake the reading task.
 *
 * @param Copy_eTimer    Which timer fired.
 * @param Copy_u8Channel Which of its channels captured, 0..3.
 * @param Copy_u32Capture The captured counter value.
 *
 * @note Captures that do not belong to the active measurement are ignored — a
 *       stray edge on an idle sensor must not be mistaken for our echo.
 */
static void US_CC_Handler(TIM_Num_t Copy_eTimer, uint8_t Copy_u8Channel, uint32_t Copy_u32Capture)
{
  /* Ignore captures that don't belong to the active measurement */
  if (Copy_eTimer != US_Active.timer || Copy_u8Channel != US_Active.channel)
  {
    return;
  }

  if (US_Active.phase == US_PHASE_RISING)
  {
    US_Active.t1 = Copy_u32Capture;
    US_Active.phase = US_PHASE_FALLING;
    /* Now look for the falling edge on the same channel */
    TIM_vSetICPolarity(Copy_eTimer, (TIM_Channel_t)Copy_u8Channel, TIM_POLARITY_LOW);
  }
  else if (US_Active.phase == US_PHASE_FALLING)
  {
    uint32_t high = (Copy_u32Capture >= US_Active.t1)
                      ? (Copy_u32Capture - US_Active.t1)
                      : ((US_Active.maxval - US_Active.t1) + Copy_u32Capture + 1u);

    /* Decode µs → cm. An echo longer than the sensor's useful range means
         * "no object within range": report it as OUT-OF-RANGE (valid = 0) instead
         * of silently clamping a bogus long/wrapped pulse to a fake 400cm reading
         * — clamping is what made a lost echo masquerade as a solid 400cm fix and
         * made the reading flap between a real distance and 400. */
    uint32_t dist = high / US_SOUND_SPEED_FACTOR;
    if (dist == 0u || dist > US_MAX_RANGE_CM)
    {
      US_Active.valid = 0u; /* out of range / spurious */
      US_Active.dist_cm = US_MAX_RANGE_CM;
    }
    else
    {
      US_Active.valid = 1u; /* a real, in-range echo */
      US_Active.dist_cm = (uint16_t)dist;
    }
    US_Active.phase = US_PHASE_DONE;

    /* Done — silence this channel and wake the task */
    TIM_vDisableCCInterrupt(Copy_eTimer, (TIM_Channel_t)Copy_u8Channel);

    BaseType_t xHPW = pdFALSE;
    xSemaphoreGiveFromISR(US_xDoneSem, &xHPW);
    portYIELD_FROM_ISR(xHPW);
  }
}

/*************************************         Public Functions
 * ******************************************/

ErrorState_t US_vInit(const US_Config_t *pxSensor)
{
  if (pxSensor == NULL)
    return NULL_POINTER;
  if (pxSensor->Timer >= TIM_TIMER6)
    return NOK; /* basic timers have no IC */

  /* 1. Initialize GPIO Pins */
  GPIO_PinConfig_t TrigCfg = {
    .Port = pxSensor->TrigPort, .PinNum = pxSensor->TrigPin, .Mode = GPIO_OUTPUT, .Otype = GPIO_PUSH_PULL, .Speed = GPIO_MEDIUM_SPEED, .PullType = GPIO_NO_PULL
  };
  GPIO_enumPinInit(&TrigCfg);

  GPIO_PinConfig_t EchoCfg = {
    .Port = pxSensor->EchoPort, .PinNum = pxSensor->EchoPin, .Mode = GPIO_ALTFN, .Otype = GPIO_PUSH_PULL, .Speed = GPIO_VERY_HIGH_SPEED, .PullType = GPIO_NO_PULL
  };

  if (pxSensor->Timer == TIM_TIMER1 || pxSensor->Timer == TIM_TIMER2)
    EchoCfg.AlternateFunction = GPIO_AF1;
  else if (pxSensor->Timer >= TIM_TIMER3 && pxSensor->Timer <= TIM_TIMER5)
    EchoCfg.AlternateFunction = GPIO_AF2;
  else if (pxSensor->Timer == TIM_TIMER8)
    EchoCfg.AlternateFunction = GPIO_AF3;
  else
    return NOK;
  GPIO_enumPinInit(&EchoCfg);

  /* 2. Configure Timer Prescaler for 1us resolution (only if not already running) */
  TIM_TypeDef *TIMx = US_TIM_Array[pxSensor->Timer];

  if (!(TIMx->CR1 & TIM_CR1_CEN))
  {
    uint32_t SystemBusClock = US_SYS_CLK_HZ;
    uint16_t Local_u16PSC = (uint16_t)((SystemBusClock / 1000000U) - 1U);

    TIMx->CR1 = 0;
    TIMx->PSC = Local_u16PSC;
    TIMx->ARR = (pxSensor->Timer == TIM_TIMER2 || pxSensor->Timer == TIM_TIMER5) ? 0xFFFFFFFF : 0xFFFF;

    if (pxSensor->Timer == TIM_TIMER1 || pxSensor->Timer == TIM_TIMER8)
    {
      SET_BIT(TIMx->BDTR, 15); /* MOE: Main Output Enable */
    }

    SET_BIT(TIMx->EGR, 0); /* Force update */
    TIMx->SR = 0;
  }

  /* 3. Configure ICU Channel (rising edge, capture enabled; CC interrupt stays OFF) */
  TIM_ICConfig_t IC_Cfg = {
    .Timer = pxSensor->Timer, .Channel = pxSensor->Channel, .Selection = TIM_IC_SELECTION_DIRECT_TI, .Prescaler = TIM_IC_PSC_DIV1, .Polarity = TIM_POLARITY_HIGH, .Filter = 0xF /* max digital filter: reject glitches that fake an early rising edge → bogus 400cm */
  };
  TIM_vIC_Init(&IC_Cfg);

  /* 4. Start Timer only if not running */
  if (!(TIMx->CR1 & TIM_CR1_CEN))
  {
    TIM_vStart(pxSensor->Timer);
  }

  /* 5. Interrupt infrastructure (idempotent across sensors) */
  if (US_xDoneSem == NULL)
  {
    US_xDoneSem = xSemaphoreCreateBinary(); /* safe to create before scheduler */
  }
  TIM_vSetCCCallback(pxSensor->Timer, US_CC_Handler);

  uint8_t Local_u8Irq = US_u8NvicIrqForTimer(pxSensor->Timer);
  if (Local_u8Irq != 0xFFu)
  {
    NVIC_vSetPriority(Local_u8Irq, 6); /* FreeRTOS-safe (>= configMAX_SYSCALL_INTERRUPT_PRIORITY) */
    NVIC_vEnableIRQ(Local_u8Irq);
  }

  return OK;
}

/*
 * @brief Trigger sensor and measure distance (cm). The calling task SLEEPS on a
 *        semaphore while the echo is in flight (CPU free), then the IC ISR wakes
 *        it with the result. Returns TIMEOUT_STATE if no echo within the window.
 * @note  MUST be called from a task context (uses a FreeRTOS semaphore).
 *        Not reentrant: one measurement at a time (sequential by design).
 */
ErrorState_t US_u16ReadDistance_cm(const US_Config_t *pxSensor, uint16_t *pu16Dist_cm)
{
  if (pxSensor == NULL || pu16Dist_cm == NULL)
    return NULL_POINTER;
  if (US_xDoneSem == NULL)
    return NOK; /* US_vInit not done */

  TIM_TypeDef *TIMx = US_TIM_Array[pxSensor->Timer];
  uint8_t      ch = (uint8_t)pxSensor->Channel;

  /* Drain any stale completion signal left by a previous (late) measurement */
  (void)xSemaphoreTake(US_xDoneSem, 0);

  /* Prepare the active-measurement context */
  US_Active.timer = pxSensor->Timer;
  US_Active.channel = ch;
  US_Active.maxval = (pxSensor->Timer == TIM_TIMER2 || pxSensor->Timer == TIM_TIMER5) ? 0xFFFFFFFFu : 0xFFFFu;
  US_Active.phase = US_PHASE_RISING;
  US_Active.valid = 0u;

  /* Arm rising edge → clear ONLY this channel's stale capture flag → enable CC IRQ.
     * TIM SR flags are "rc_w0": writing 0 clears, writing 1 leaves untouched. So to
     * clear just CCxIF we write all-ones EXCEPT that bit (~mask). The old code wrote
     * ~(1<<(ch+1)) which is the same intent, but built the bit from a raw shift; use
     * the named CCxIF position and a clear comment so it can't be misread as "write
     * the whole register". */
  TIM_vSetICPolarity(pxSensor->Timer, pxSensor->Channel, TIM_POLARITY_HIGH);
  TIMx->SR = ~(1UL << (TIM_SR_CC1IF + ch)); /* clear this channel's CCxIF only */
  TIM_vEnableCCInterrupt(pxSensor->Timer, pxSensor->Channel);

  /* Fire the trigger pulse */
  US_vSendTrigger(pxSensor);

  /* Sleep until the ISR delivers both edges, or until the echo window expires */
  if (xSemaphoreTake(US_xDoneSem, pdMS_TO_TICKS(US_TASK_TIMEOUT_MS)) == pdTRUE && US_Active.phase == US_PHASE_DONE)
  {
    /* An out-of-range / spurious echo (valid==0) is reported like "no object"
         * so the caller's default (400 / clear) kicks in, instead of returning a
         * fake solid 400cm reading that flaps against the real distance. */
    if (!US_Active.valid)
      return TIMEOUT_STATE;

    *pu16Dist_cm = US_Active.dist_cm;
    return OK;
  }

  /* Timeout: no echo (out of range / no object) — disarm and report */
  TIM_vDisableCCInterrupt(pxSensor->Timer, pxSensor->Channel);
  return TIMEOUT_STATE;
}
