/**
 * @file    TIM_documentation.md
 * @author  Abdallah Saleh
 * @brief   Documentation for the Timer (TIM) Driver
 * @details This file provides an in-depth explanation of the Timer driver architecture, API usage, and examples.
 */

# Timer Driver Documentation for STM32F446xx

## 1. Overview
The Timer (TIM) Driver provides a hardware abstraction layer for configuring and controlling the General Purpose Timers (TIM2-TIM5) on the STM32F446xx microcontroller. It supports various modes of operation including:
- **Time Base Generation**: Generating precise periodic events.
- **Output Compare / PWM**: Generating Pulse Width Modulated signals.
- **Input Capture**: Measuring input signal frequency and duty cycle (Future Implementation).
- **Interrupts**: Handling timer overflow and update events.

## 2. Architecture
The driver is structured into the following files:

- **`TIM_interface.h`**: The public API header file. Contains function prototypes, enumerations, and structures that the application layer uses.
- **`TIM_config.h`**: Configuration file for compile-time settings (currently minimal).
- **`TIM_private.h`**: Private header file containing register definitions and bit macros. Not accessible by the application layer.
- **`TIM_program.c`**: The source file containing the implementation of the driver functions.

## 3. Supported Timers
The driver currently supports the following general-purpose timers:
- **TIM2** (32-bit counter)
- **TIM3** (16-bit counter)
- **TIM4** (16-bit counter)
- **TIM5** (32-bit counter)

## 4. API Reference

### 4.1 Initialization Functions
- `TIM_vInit(const TIM_Config_t *pxConfig)`: Initializes the timer with the specified prescaler, auto-reload value, and counter mode.
- `TIM_vPWM_Init(const TIM_PWMConfig_t *pxPWMConfig)`: Configures a timer channel for PWM output.

### 4.2 Control Functions
- `TIM_vStart(TIM_Num_t Copy_eTimer)`: Starts the timer counter.
- `TIM_vStop(TIM_Num_t Copy_eTimer)`: Stops the timer counter.
- `TIM_vSetCompareValue(TIM_Num_t Copy_eTimer, TIM_Channel_t Copy_eChannel, uint32_t Copy_u32Value)`: Updates the compare value (Duty Cycle) dynamically.
- `TIM_vSetPrescaler(TIM_Num_t Copy_eTimer, uint16_t Copy_u16Prescaler)`: Updates the prescaler dynamically.
- `TIM_vSetAutoReloadValue(TIM_Num_t Copy_eTimer, uint32_t Copy_u32AutoReload)`: Updates the auto-reload value (Period) dynamically.

### 4.3 Interrupt Handling
- `TIM_vEnableInterrupt(TIM_Num_t Copy_eTimer)`: Enables the timer update interrupt.
- `TIM_vDisableInterrupt(TIM_Num_t Copy_eTimer)`: Disables the timer update interrupt.
- `TIM_vSetCallback(TIM_Num_t Copy_eTimer, void (*pvCallback)(void))`: Registers a user-defined function to be called when the interrupt occurs.

### 4.4 Utility Functions
- `TIM_vDelayMs(TIM_Num_t Copy_eTimer, uint32_t Copy_u32Ms)`: Blocking delay function in milliseconds.
- `TIM_vDelayUs(TIM_Num_t Copy_eTimer, uint32_t Copy_u32Us)`: Blocking delay function in microseconds.

## 5. Usage Guide

### 5.1 Time Base Configuration
To configure a timer for a specific frequency:
1. Define a `TIM_Config_t` structure.
2. Select the timer instance (`TIM_TIMERx`).
3. Calculate Prescaler and AutoReloadValue based on the system clock.
   - `Timer_Clock = System_Clock / (Prescaler + 1)`
   - `Update_Event_Period = (AutoReloadValue + 1) / Timer_Clock`
4. call `TIM_vInit()`.
5. Call `TIM_vStart()`.

**Example:**
```c
TIM_Config_t myTimer;
/* System Clock = 16MHz */
myTimer.Timer = TIM_TIMER2;
myTimer.Prescaler = 16000 - 1;       /* Timer Clock = 1KHz (1ms) */
myTimer.AutoReloadValue = 1000 - 1;  /* Period = 1000ms = 1s */
myTimer.Mode = TIM_COUNTERMODE_UP;
TIM_vInit(&myTimer);
TIM_vStart(TIM_TIMER2);
```

### 5.2 PWM Configuration
To generate a PWM signal:
1. Configure the GPIO pin as Alternate Function (AF).
2. Define a `TIM_PWMConfig_t` structure.
3. Set Channel, Mode (`TIM_PWM_MODE1` or `TIM_PWM_MODE2`), Prescaler, Period, and Initial Duty Cycle.
4. Call `TIM_vPWM_Init()`.
5. Call `TIM_vStart()`.

**Example:**
```c
TIM_PWMConfig_t myPWM;
myPWM.Timer = TIM_TIMER3;
myPWM.Channel = TIM_CHANNEL1; /* PA6 */
myPWM.Mode = TIM_PWM_MODE1;
myPWM.Prescaler = 16 - 1;          /* 1MHz Clock */
myPWM.Period = 1000 - 1;           /* 1KHz PWM Frequency */
myPWM.DutyCycle = 500;             /* 50% Duty Cycle */
myPWM.Polarity = TIM_POLARITY_HIGH;
TIM_vPWM_Init(&myPWM);
TIM_vStart(TIM_TIMER3);
```

### 5.3 Interrupts
To use interrupts:
1. Configure and start the timer as usual.
2. Create a void function (e.g., `MyISR`).
3. Call `TIM_vSetCallback(TIM_TIMERx, MyISR)`.
4. Call `TIM_vEnableInterrupt(TIM_TIMERx)`.
5. Enable the corresponding IRQ in the NVIC (e.g., `NVIC_TIM3_IRQn`).

## 6. Important Notes
- Ensure the APB1/APB2 peripheral clock is enabled via RCC **before** initializing the timer.
- `TIM_vDelayMs` and `TIM_vDelayUs` assume a system clock of 16MHz if `TIM_CLOCK_FREQ` is not defined. You can define this macro in `TIM_program.c` or globally.
- PWM pins must be configured as **Alternate Function Output Push-Pull** in the GPIO driver. Refer to the datasheet for the correct AF mapping (usually AF1 or AF2 for Timers).
