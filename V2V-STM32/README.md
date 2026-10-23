# 🔧 V2V-STM32 — Real-Time Safety Core

## 🚗 V2V Safety-Critical Core

This directory contains the **real-time safety-critical implementation** of the V2X Collision Avoidance System.

It runs on **STM32F446RE** and is responsible for executing all V2V collision avoidance algorithms with deterministic timing using **FreeRTOS**, talking to the ESP32 (V2V radio) over UART1 and to the Raspberry Pi (telemetry + remote control) over UART2.

---

## 🎯 Purpose of this Layer

This layer handles:

- Real-time sensor acquisition (6× ultrasonic + MPU9250 IMU)
- Cooperative collision detection (FCW, DNPW, EEBL, BSW, IMA) over DSRC
- Alert generation (LEDs, buzzer)
- V2V message broadcast/receive via the ESP32 bridge (ESP-NOW)
- Telemetry streaming to the Raspberry Pi
- Hardware-level driver control (all MCAL/HAL written from scratch — no HAL library)
- Independent hardware watchdog (IWDG) with per-task liveness checks

This layer is designed to be:

- Deterministic
- Modular
- Safety-focused
- Self-recovering (a hung task triggers a hardware reset instead of hanging forever)

---

## 🧠 Architecture Overview

The firmware follows a layered embedded architecture:

```text
Application Layer (SafetyEngine + FCW/DNPW, EEBL, BSW, IMA + DSRC)
        ↓
HAL (LED, Buzzer, Ultrasonic, MPU9250)
        ↓
MCAL (RCC, GPIO, NVIC, SCB, SYSCFG, SYSTICK, EXTI, SPI, TIM, USART, IWDG)
        ↓
Hardware (STM32F446RE)
```

### FreeRTOS task pipeline

`main.c` creates six tasks; the ADAS decision and V2X TX/RX run at the
highest priority, sensing at medium, and actuation/telemetry/monitoring below
that. Every monitored task bumps its own heartbeat slot each loop; the
watchdog task only kicks the hardware IWDG when **all** slots are still
advancing — a stalled task (hard fault, infinite loop, ISR storm) stops the
kicks and the MCU hardware-resets.

| Task | Priority | Period | Role |
| --- | --- | --- | --- |
| `SafetyEngine_Task` | 4 (highest) | 50 ms | Single-pass ADAS brain: locks both mutexes, runs `SafetyEngine_voidUpdate()` over the whole DSRC neighbor table, aggregates every module's result into the `G_u16SystemFlags` status word (2 bits/module) |
| `ESP_Comm_Task` | 4 | event-driven RX + 100 ms TX | Drains UART1 bytes into the DSRC parser, maintains the neighbor table (flush + purge stale), and broadcasts this vehicle's `Neighbor` frame every 100 ms |
| `Sensors_Task` | 3 | adaptive (~15–150 ms) | Reads all 6 ultrasonics (interrupt-driven, sleeps during echo flight) + the MPU9250 (speed/heading/pitch/roll/position), publishes into `G_stHostVehicleState` |
| `Feedback_Task` | 2 | 25 ms | The "muscle": renders `G_u16SystemFlags` onto LEDs + buzzer. Makes no decisions of its own — front LEDs on FCW-critical, rear LEDs on EEBL-critical, interior LED + buzzer on any alert |
| `RPi_Comm_Task` | 1 | 100 ms | Streams a `\n`-terminated ASCII CSV telemetry line over UART2 to the Raspberry Pi (speed, heading, pitch/roll, 6× ultrasonic, ADAS flags, BSW per-side severity) |
| `Watchdog_Task` | 1 (lowest) | 300 ms check | Liveness monitor; kicks the IWDG (2 s hardware timeout) only if every task's heartbeat advanced since the last check |

Lock order is always **NeighborTable → Data** and is never taken nested from
more than one place, so no deadlock is possible between tasks.

---

## 📁 Directory Structure

```text
V2V-STM32/
├── Inc/
│   ├── Application/
│   │   ├── SafetyEngine/   # Shared risk model, direction detection, module orchestration
│   │   ├── DSRC/           # Neighbor table + wire protocol (shared with esp32/)
│   │   ├── FCW_DNPW/       # Forward Collision Warning + Do-Not-Pass Warning (one module)
│   │   ├── EEBL/           # Electronic Emergency Brake Light
│   │   ├── BSW/            # Blind Spot Warning
│   │   └── IMA/            # Intersection Movement Assist
│   │
│   ├── Drivers/
│   │   ├── MCAL/           # RCC, GPIO, NVIC, SCB, SYSCFG, SYSTICK, EXTI, SPI, TIM, USART, IWDG
│   │   ├── HAL/             # LED, BUZZ, US (ultrasonic), MPU9250
│   │   └── LIB/              # ErrTypes, STD_MACROS, register maps
│   │
│   └── System/
│       └── System.h        # System-wide configuration
│
├── Src/
│   ├── main.c                  # FreeRTOS tasks, ISR callbacks, watchdog heartbeats
│   ├── System.c
│   ├── SafetyEngine_program.c  # Direction detection + risk model + per-cycle orchestration
│   ├── DSRC.c                  # Neighbor table, UART framing, stale purge
│   ├── FCW_DNPW_program.c
│   ├── EEBL_program.c
│   ├── BSW_program.c
│   ├── IMA_program.c
│   ├── RCC_program.c / GPIO_prog.c / NVIC_program.c / SCB_program.c
│   ├── SYSCFG_program.c / SYSTIC_program.c / TIM_program.c
│   ├── EXTI_program.c / SPI_program.c / USART_program.c / IWDG_program.c
│   ├── MPU9250_program.c / US_prog.c / LED_prog.c / BUZ_program.c
│   └── syscalls.c
│
├── Startup/
│   └── startup_stm32f446retx.s
│
├── STM32F446RETX_FLASH.ld
├── STM32F446RETX_RAM.ld
│
├── docs/
│   └── PCB_BUILD_STAGES.md   # Altium carrier-board build guide, stage by stage
│
├── ThirdParty/
│   ├── License/
│   └── Source/              # FreeRTOS kernel (tasks, queue, timers, event_groups, ...)
│
└── Debug/                   # Auto-generated build output (STM32CubeIDE / make)
```

---

## 🚘 Implemented V2V Subsystems

All five modules share the same `SafetyEngine` risk model
(`SafetyEngine_AssessDistanceRisk`, `SafetyEngine_EvaluateRisk`,
`SafetyEngine_DetectDirection`) and consume the DSRC neighbor table each
50 ms cycle. Each returns a graded **0=Safe / 1=Warning / 2=Critical** flag.

### 🔹 FCW / DNPW — Forward Collision + Do-Not-Pass Warning

One combined module. `FCW_GetFrontFlag()` covers a same-lane vehicle ahead
(local, ultrasonic-only). For an oncoming vehicle, it distinguishes a
**head-on candidate** (broadcast cooperatively as `fcw_headon_flag` so the
other car can confirm) from a **Do-Not-Pass** case (oncoming car in another
lane — graded critical when the front-right sensor also reads a car alongside
on the overtaking side).

### 🔹 EEBL — Electronic Emergency Brake Light

Detects a same-direction vehicle ahead braking hard and grades the risk from
the rear ultrasonic distance via the shared speed-dependent safe-distance
model.

### 🔹 BSW — Blind Spot Warning

Dual role per cycle: broadcasts *this* car's own front-side occupancy
(`bit0=LEFT, bit1=RIGHT`) for others to use, and separately computes whether
*this* car has a neighbor in its own rear blind spot — with per-side severity
so the Raspberry Pi can tell left from right.

### 🔹 IMA — Intersection Movement Assist

Processes crossing-direction neighbors only, comparing distance-to-intersection
and speed to flag a likely intersection collision.

### DSRC — the transport underneath all five

`DSRC.c` maintains the neighbor table (up to `MAX_NEIGHBORS`, purged after
`NEIGHBOR_TIMEOUT` of silence) and frames/parses the packed `Neighbor` struct
over UART1 (`START_BYTE … END_BYTE`, checksummed). The **same struct layout**
is mirrored byte-for-byte in the ESP32 sketches (`esp32/master`,
`esp32/sniffer`) — see [`../esp32/README.md`](../esp32/README.md). Any field
reorder on one side must be mirrored on the other.

> There is no separate "SDW" (Safe Distance Warning) module — the shared
> safe/critical distance model in `SafetyEngine` (used by FCW and EEBL) covers
> that role.

---

## 📡 Telemetry to the Raspberry Pi

`RPi_Comm_Task` streams one ASCII CSV line every 100 ms over UART2:

```text
T,speed,heading,pitch,roll,FL,FC,FR,BL,BC,BR,flags,bsw_sides\n
```

ASCII (not the binary `Neighbor` struct) was chosen because it contains no
`0x00` bytes, which was previously causing dropped runs over the UART link.
`flags` is the 16-bit ADAS status word (2 bits per module); `bsw_sides` packs
left/right blind-spot severity separately since the aggregated flag alone
loses which side triggered. This feeds `RPI/DashBoard/` — see
[`../RPI/DashBoard/README.md`](../RPI/DashBoard/README.md).

---

## ⚙️ Hardware Dependencies

- STM32F446RE (Nucleo)
- MPU9250 IMU (SPI)
- 6× HC-SR04 ultrasonic sensors (interrupt-driven, front-left/center/right + rear-left/center/right)
- ESP32-S3 on UART1 (V2V radio bridge)
- Raspberry Pi on UART2 (telemetry + control)
- LEDs (front L/R, rear L/R, interior) + buzzer

See [`docs/PCB_BUILD_STAGES.md`](docs/PCB_BUILD_STAGES.md) for the carrier-board build (power, MCU, sensors, comms blocks).

---

## 🔐 Safety & Determinism

- Interrupt-driven ultrasonic sensing (task sleeps during echo flight, no busy-wait)
- Fixed lock order (NeighborTable → Data) — deadlock-free by construction
- Independent hardware watchdog (IWDG) gated on **every** task's liveness, not just one
- All timing-critical work happens inside `SafetyEngine_Task` (priority 4, 50 ms cycle)
- Feedback task never makes decisions — it only renders what SafetyEngine already computed

---

## 🏁 Summary

This layer is the **core safety engine** of the V2X project: a from-scratch
MCAL/HAL stack, a cooperative DSRC neighbor protocol shared with the ESP32
radio bridge, five ADAS modules built on one shared risk model, and a
self-monitoring FreeRTOS pipeline that hardware-resets itself if anything
stalls.

---
