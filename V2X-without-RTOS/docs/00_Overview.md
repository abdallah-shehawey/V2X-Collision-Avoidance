# V2X Collision Avoidance System — Overview

> **Project:** A vehicle collision-avoidance system using V2X (Vehicle-to-Everything) technology.
> **MCU:** STM32F446RE (Cortex-M4)
> **Bare-Metal / No RTOS** — everything runs in a single super-loop inside `main()`.

---

## 1. What is V2X?

V2X stands for **Vehicle-to-Everything** — the car talks to everything around it:

- **V2V** (Vehicle-to-Vehicle): car talks to car → **this is what this project implements**.
- V2I (Vehicle-to-Infrastructure): car talks to traffic lights and roadside units.
- V2P (Vehicle-to-Pedestrian): car talks to pedestrians' phones.

The core idea: **every car periodically broadcasts a message** containing its data (speed, heading, and its warning flags). Every nearby car receives that message and decides: *Am I in danger? Should I warn my driver?*

This is called **Cooperative Awareness** — I don't just see with my own sensors, I also "hear" from other cars I might not be able to see.

---

## 2. The Five Safety Applications

The project implements **5 V2V safety applications**, each handling a different hazard scenario:

| # | Acronym | Full name | Scenario it solves |
|---|---------|-----------|--------------------|
| 1 | **FCW**  | Forward Collision Warning | Warns of a frontal crash (car ahead braking / object ahead) |
| 2 | **EEBL** | Electronic Emergency Brake Lights | When *I* brake hard, warn the car behind me |
| 3 | **BSW**  | Blind Spot Warning | Warns of a car in the blind spot (the angle the mirror misses) |
| 4 | **DNPW** | Do Not Pass Warning | "Do not overtake" — oncoming car while you consider passing |
| 5 | **IMA**  | Intersection Movement Assist | Helps at intersections — who goes first so nobody collides |

> **FCW and DNPW share one module** (`FCW_DNPW`) because they are decided from the
> same signals; the others have their own module. See the index below.

---

## 3. Overall Architecture

The system is built in **layers**, the standard professional embedded structure:

```text
┌─────────────────────────────────────────────────────────┐
│                      APP Layer                          │
│   main.c → SafetyEngine → (FCW_DNPW, EEBL, BSW, IMA)    │
├─────────────────────────────────────────────────────────┤
│                  Communication Layer                    │
│              DSRC (send & receive layer)                │
├─────────────────────────────────────────────────────────┤
│                   MCAL (Drivers)                        │
│   USART · GPIO · RCC · SysTick · NVIC                   │
├─────────────────────────────────────────────────────────┤
│                     Hardware                            │
│              STM32F446RE (Cortex-M4)                    │
└─────────────────────────────────────────────────────────┘
```

Each safety module follows the same fixed file pattern:

| File | Purpose |
|------|---------|
| `Xxx_interface.h` | The public API (functions called from outside the module) |
| `Xxx_config.h`    | Tunable numbers and settings (thresholds, enables) |
| `Xxx_private.h`   | Internal `static` functions belonging to the module |
| `Xxx_program.c`   | The actual implementation (the logic) |

> **Why this split?** If you want to change a setting (e.g. a safe distance) you only touch `config.h` without touching the logic. If you want to understand the API you only read `interface.h`.

---

## 4. The Heart of the System: the Single-Pass Pattern

This is the most important concept in the whole project. Instead of each module
iterating the neighbor table on its own — i.e. one loop each = many loops — we do
**a single pass**.

`SafetyEngine_voidUpdate()` does the following:

```text
1) BeginCycle      → each module resets its per-cycle state
2) ProcessNeighbor → iterate ONCE over the neighbors; each neighbor is dispatched
                     only to the modules its direction can affect
```

The results are then read on demand through each module's getter (there is no
EndCycle). The neighbor's direction (`DetectDirection`) is computed **once** per
neighbor and shared by all modules.

> Full details in [`07_SafetyEngine.md`](07_SafetyEngine.md).

---

## 5. The Super-Loop (the `main` without an RTOS)

Since there is no RTOS, everything runs in one infinite loop:

```c
while (1)
{
  DSRC_Update();              // 1. process messages that arrived
  SafetyEngine_voidUpdate();  // 2. run the modules over the neighbors

  // 3. read local results for this car's alerts (LED/buzzer)
  uint8_t fcw_front  = FCW_GetFrontFlag();
  uint8_t blind_spot = BSW_u8GetBlindSpot();
  // ... EEBL, head-on, DNPW, IMA ...

  // 4. broadcast my cooperative flags
  Neighbor self = simulate_self(VEHICLE_ID);
  self.fcw_headon_flag = FCW_GetHeadonFlag();
  self.bsw_flag        = BSW_u8GetFlag();
  self.ima_flag        = IMA_u8GetFlag();
  DSRC_SendNeighbor(&self);

  SYSTIC_vDelayMs(500);       // 5. wait half a second and repeat
}
```

Reception happens in the background via the **USART interrupt** (not in the loop), and the loop just processes whatever accumulated.

---

## 6. The Flags & Cooperation Concept (Cooperative Flags)

Each car broadcasts the flags other cars need to cooperate:

- `fcw_headon_flag` — do I have a head-on candidate (a car ahead while meeting an oncoming car)?
- `bsw_flag`        — do I see a car on my front-left/right?
- `ima_flag`        — am I at risk at an intersection?

The other car uses these flags to **confirm** its decision (Cooperative Confirmation).
Example: in FCW, I don't confirm a head-on warning from an oncoming car unless **I have
a head-on candidate** AND **it reports one too** → both agree → the warning is real, not
a false alarm. If only I see one, it's an overtaking situation → DNPW instead.

---

## 7. The Shared Risk Model (Distance-Based Risk Model)

All modules (except IMA) use the same risk-assessment formula, in `SafetyEngine_AssessDistanceRisk()`:

```text
safe_distance = host_speed (m/s) × cm-per-(m/s)   (with a minimum floor)
if actual_distance >= safe_distance        → SAFE
if between critical and safe_distance       → WARNING
if actual_distance < critical_distance      → CRITICAL
```

The idea: **the faster you go, the larger the safety gap you need.** This is a simple, effective alternative to complex TTC (Time-To-Collision) computations.

> **Note:** the project is calibrated for a prototype car with a top speed of ~5 m/s. All numbers in the `config.h` files can be re-tuned on the real car.

---

## 8. Documentation Index

| File | Content |
|------|---------|
| [`00_Overview.md`](00_Overview.md) | **(you are here)** Overview and architecture |
| [`01_FCW_DNPW.md`](01_FCW_DNPW.md) | Forward Collision + Do-Not-Pass Warning, in detail |
| [`02_EEBL.md`](02_EEBL.md) | Electronic Emergency Brake Lights, in detail |
| [`03_BSW.md`](03_BSW.md) | Blind Spot Warning, in detail |
| [`05_IMA.md`](05_IMA.md) | Intersection Movement Assist, in detail |
| [`06_DSRC.md`](06_DSRC.md) | The DSRC communication layer (send/receive/parser) |
| [`07_SafetyEngine.md`](07_SafetyEngine.md) | The central engine and the Single-Pass pattern |

---

## 9. Notes on the Current Code (if you want to extend it)

- **`Host_Speed` / `Host_Heading` / `US_Distances` are currently fixed at zero:** in `SafetyEngine_program.c` they are defined as zero and nothing updates them from real sensors yet (the MPU and ultrasonic drivers are not wired in). This means in the current state the modules stay in SAFE, because speed is zero and distances are zero. This is expected at the prototype stage — once you connect the sensors, those values will move.
- **`simulate_self()` in `main.c`** generates fake speed/heading data for testing (simulation without real sensors).
- **`DSRC_RemoveStale()` is never called** in the current loop — so stale neighbors are not removed from the table. If you enable it, you need `last_update` to be updated with real time.

> These are not necessarily bugs — they are open points for the sensor-integration stage. They are listed so you keep them in mind as you continue.
