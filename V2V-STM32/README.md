# 🔧 BareMetal Layer – STM32F446RE

## 🚗 V2V Safety-Critical Core

This directory contains the **real-time safety-critical implementation** of the V2X Collision Avoidance System.

It runs on **STM32F446RE** and is responsible for executing all V2V collision avoidance algorithms with deterministic timing using **FreeRTOS**.

---

## 🎯 Purpose of the BareMetal Layer

The BareMetal layer handles:

- Real-time sensor acquisition

- Collision detection algorithms

- ADAS decision logic

- Alert generation (LED, Buzzer, LCD)

- V2V message transmission

- Hardware-level driver control

This layer is designed to be:

- Deterministic

- Modular

- Scalable

- Safety-focused

---

## 🧠 Architecture Overview

The firmware follows a layered embedded architecture:

```bash
Application Layer (V2V Subsystems)
        ↓
HAL (Peripheral Drivers)
        ↓
MCAL (Register-Level Drivers)
        ↓
Hardware (STM32F446RE)
```

---

## 📁 Directory Structure

```bash
BareMetal/
├── Inc/
│   ├── Application/        # V2V Subsystem Headers
│   │   ├── BSW/
│   │   ├── DNPW/
│   │   ├── EEBL/
│   │   ├── FCW/
│   │   ├── IMA/
│   │   └── SDW/
│   │
│   ├── Drivers/
│   │   ├── MCAL/           # RCC, GPIO, NVIC, SCB, SYSCFG, SYSTICK
│   │   ├── HAL/            # SPI, USART, TIM, EXTI
│   │   └── LIB/            # Common macros & utilities
│   │
│   └── System/
│       └── System.h        # System-wide configuration
│
├── Src/
│   ├── main.c
│   ├── System.c
│   ├── RCC_program.c
│   ├── GPIO_prog.c
│   ├── NVIC_program.c
│   ├── SCB_program.c
│   ├── SYSCFG_program.c
│   ├── SYSTIC_program.c
│   ├── TIM_program.c
│   ├── USART_program.c
│   ├── SPI_program.c
│   ├── EXTI_program.c
│   ├── MPU9250_program.c
│   ├── US_prog.c
│   ├── BSW_program.c
│   ├── DNPW_program.c
│   ├── EEBL_program.c
│   ├── FCW_program.c
│   ├── IMA_program.c
│   ├── SDW_program.c
│   ├── syscalls.c
│   └── sysmem.c
│
├── Startup/
│   └── startup_stm32f446retx.s
│
├── STM32F446RETX_FLASH.ld
├── STM32F446RETX_RAM.ld
│
├── ThirdParty/
│   ├── License/
│   └── Source/             # FreeRTOS Kernel
│       ├── tasks.c
│       ├── queue.c
│       ├── timers.c
│       ├── event_groups.c
│       ├── stream_buffer.c
│       ├── croutine.c
│       ├── list.c
│       ├── FreeRTOSConfig.h
│       └── portable/
│
└── Debug/                  # Auto-generated build files
```

---

## 🧵 Real-Time Operating System (FreeRTOS)

FreeRTOS is integrated as third-party middleware.

### Responsibilities

- Priority-based task scheduling

- Deterministic execution

- Inter-task communication (Queues, Event Groups)

- Software timers

- Safe separation of sensing & decision tasks

### Example Task Distribution

|Task|Purpose|
|---|---|
|Sensor_Task|Reads IMU & Ultrasonic data|
|V2V_Task|Executes collision algorithms|
|Communication_Task|Sends/Receives V2V messages|
|Display_Task|Updates LCD & LEDs|
|Logger_Task|UART debugging|

---

## 🚘 Implemented V2V Subsystems

### 🔹 EEBL – Electronic Emergency Brake Light

Detects sudden deceleration and broadcasts emergency warnings.

### 🔹 FCW – Forward Collision Warning

Calculates relative distance & speed to prevent rear-end collisions.

### 🔹 SDW – Safe Distance Warning

Maintains safe buffer distance around the vehicle.

### 🔹 DNPW – Do Not Pass Warning

Evaluates overtaking safety using opposite lane data.

### 🔹 IMA – Intersection Movement Assist

Analyzes crossing trajectories to avoid intersection crashes.

### 🔹 BSW – Blind Spot Warning

Monitors lateral zones during lane changes.

---

## ⚙️ Hardware Dependencies

- STM32F446RE

- MPU9250 IMU

- Ultrasonic Sensors

- USART Communication Module

- SPI Interface Devices

- LEDs / Buzzer / LCD

---

## 🔐 Safety & Determinism

- Interrupt-driven sensing

- RTOS preemption for critical tasks

- Minimal blocking code

- Latency target < 100 ms for collision events

---

## 🏁 Summary

The BareMetal layer represents the **core safety engine** of the V2X project.

It is optimized for:

- Real-time responsiveness

- Hardware-level control

- Modular safety subsystems

- Expandability for future V2X features

---
