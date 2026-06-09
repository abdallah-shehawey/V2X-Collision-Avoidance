# RTOS Explained — V2X Project (BareMetal), Line by Line

> This document is written for someone who already fully understands the system *without* an RTOS (the `V2X-without-RTOS` version that runs as a single super-loop in `main`), and wants to understand **exactly** what FreeRTOS does in the `BareMetal` version.
>
> The core idea to keep in mind from start to finish:
> In the bare-metal version you had **one loop** that read the sensors, computed, and drove the outputs one after another in sequence. Here we split that same work into **5 independent loops (Tasks)**, each running "as if" it were alone, while FreeRTOS distributes CPU time among them and decides who runs first.

---

## Table of Contents

1. [The Fundamental Shift: From Super-Loop to Tasks](#1-the-fundamental-shift-from-super-loop-to-tasks)
2. [Kernel Configuration: `FreeRTOSConfig.h`](#2-kernel-configuration-freertosconfigh)
3. [Concepts You Must Be Clear On Before the Code](#3-concepts-you-must-be-clear-on-before-the-code)
4. [`main`: From the First Line to Starting the Scheduler](#4-main-from-the-first-line-to-starting-the-scheduler)
5. [Inter-Task Communication: Queue and Mutex](#5-inter-task-communication-queue-and-mutex)
6. [Every Task, Line by Line](#6-every-task-line-by-line)
7. [The ISR: Bridging Hardware to Tasks](#7-the-isr-bridging-hardware-to-tasks)
8. [A Full Timeline Scenario](#8-a-full-timeline-scenario)
9. [Why Are the Priorities and Locks Arranged This Way?](#9-why-are-the-priorities-and-locks-arranged-this-way)

---

## 1. The Fundamental Shift: From Super-Loop to Tasks

### In the bare-metal version (the one you understand)

```c
int main(void) {
    System_setup();
    while (1) {
        read_sensors();      // everything in sequence
        run_adas_logic();    // one after another
        drive_actuators();   // nothing interrupts anything
        send_to_esp();
        send_to_rpi();
    }
}
```

The problem here: if `read_sensors()` waits 300ms for the ultrasonic echo, the CPU is **stuck waiting** and doing nothing else. And if a byte arrives from the ESP in the meantime, nobody responds to it until the loop completes a full iteration.

### In the RTOS version

We split the same work into 5 functions, each with its own `for(;;)` — i.e. each is an **independent infinite loop**:

| Task | Priority | What it does |
| --- | --- | --- |
| `vTask_SafetyEngine` | 4 (highest) | The brain: runs all ADAS modules and produces the commands |
| `vTask_ESP_Comm` | 4 (highest) | V2X communication with the ESP |
| `vTask_Sensors` | 3 | Reads 6 ultrasonics + the MPU9250 |
| `vTask_Feedback` | 2 | The muscles: drives the motors, LEDs and buzzer |
| `vTask_RPi_Comm` | 1 (lowest) | Sends telemetry to the Raspberry Pi |

The **Scheduler** of FreeRTOS decides which of them gets the CPU at any instant. The magic is that when a task needs to wait for something (like the ultrasonic returning its echo), it tells the kernel "put me to sleep" and the CPU goes off to run another task instead of standing idle.

The result: the CPU is **never stuck waiting**, and the important things (safety) automatically preempt the less important ones (telemetry).

---

## 2. Kernel Configuration: `FreeRTOSConfig.h`

This file ([FreeRTOSConfig.h](../ThirdParty/FreeRTOS/FreeRTOSConfig.h)) defines the entire behavior of the kernel. The most relevant lines for our project:

```c
#define configUSE_PREEMPTION            1
```

**Preemption is on.** This means that if a higher-priority task becomes ready while it isn't running, the kernel **immediately interrupts** the running task and hands the CPU to the higher-priority one. This is the opposite of "cooperative" scheduling, which waits for a task to yield voluntarily.

```c
#define configCPU_CLOCK_HZ              ( SystemCoreClock )   // 16 MHz (HSI)
#define configTICK_RATE_HZ             ( ( TickType_t ) 1000 )
```

**Tick = 1000 Hz → each tick = 1 millisecond.** This is the RTOS "heartbeat". A hardware timer (the SysTick) fires an interrupt every 1ms, and on each interrupt the kernel:

- Increments the time counter (`xTaskGetTickCount`).
- Checks whether any sleeping task has finished its sleep → wakes it up.
- Decides whether to perform a context switch.

That's why when you write `pdMS_TO_TICKS(50)` it converts to 50 ticks = 50ms.

```c
#define configMAX_PRIORITIES            ( 5 )
```

Priorities range from **0 to 4**. Priority 0 is reserved for the **Idle Task** (which runs when nobody else wants the CPU). That's why our lowest task (`RPi_Comm`) has priority 1, so it doesn't tie with the Idle task.

```c
#define configMINIMAL_STACK_SIZE        ( ( unsigned short ) 130 )   // in words = 130×4 = 520 bytes
#define configTOTAL_HEAP_SIZE           ( ( size_t ) ( 75 * 1024 ) ) // 75 KB
```

- Each task has its own independent **stack** (its local variables plus the place where the CPU state is saved when it gets interrupted).
- All tasks, queues and mutexes are allocated from the FreeRTOS **heap** (those 75KB). The module in use is [`heap_4.c`](../ThirdParty/FreeRTOS/portable/MemMang/heap_4.c), which can coalesce freed memory and reuse it.

```c
#define configUSE_MUTEXES               1
#define configUSE_COUNTING_SEMAPHORES   1
#define configCHECK_FOR_STACK_OVERFLOW  2
#define configUSE_MALLOC_FAILED_HOOK    1
```

- We enabled Mutexes (we'll need them to protect shared data).
- `configCHECK_FOR_STACK_OVERFLOW = 2`: the kernel monitors each task for going past its stack and calls `vApplicationStackOverflowHook` (defined in [System.c:91](../Src/System.c#L91)).
- If the heap runs out it calls `vApplicationMallocFailedHook` ([System.c:100](../Src/System.c#L100)).

```c
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY   5
```

This is an **extremely important** point, tied to the ISR. Any interrupt that wants to call a FreeRTOS function ending in `FromISR` (like our UART) **must** have a numeric priority **greater than or equal to 5** (i.e. a lower urgency). That's why in [System.c:205](../Src/System.c#L205):

```c
NVIC_vSetPriority(NVIC_USART1, 6);  // 6 ≥ 5 → safe with FreeRTOS
```

If you set it below 5 (a higher priority), the interrupt could preempt the kernel in the middle of a critical operation and corrupt everything.

```c
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler
```

These bind the FreeRTOS mechanism to 3 Cortex-M4 interrupts:

- **SysTick**: the 1ms heartbeat.
- **PendSV**: this performs the actual **context switch** (saves the old task's registers and restores the new task's).
- **SVC**: used once to start the very first task.

---

## 3. Concepts You Must Be Clear On Before the Code

### Task States

At any instant, each task is in one of 4 states:

```text
        xTaskCreate
            │
            ▼
        ┌────────┐   scheduler picked it      ┌─────────┐
        │ READY  │ ─────────────────────────► │ RUNNING │
        │        │ ◄───────────────────────── │         │
        └────────┘   preempted by a higher    └────┬────┘
            ▲         priority task                 │
            │  sleep expired /                       │ vTaskDelay /
            │  data arrived in queue /               │ xSemaphoreTake (locked) /
            │  mutex released                        │ xQueueReceive (empty)
            │                                         ▼
        ┌────────────────────────────────────────────────┐
        │                   BLOCKED                        │
        │  (asleep / waiting for something — uses no CPU)  │
        └────────────────────────────────────────────────┘
```

- **Running**: currently executing on the CPU (only one at a time on a single-core processor).
- **Ready**: ready and wanting the CPU, but a higher-priority task is holding it.
- **Blocked**: asleep, waiting for something (time, data, a mutex). **This is the key state**: a task here **consumes no CPU at all**, and the CPU goes to others. This is the secret to why an RTOS is efficient.

### The Golden Rule of the Scheduler

> **At any instant, the CPU runs the highest-priority task that is in the Ready state.**

And if more than one task shares the same priority and they are all Ready (like `SafetyEngine` and `ESP_Comm`, both priority 4), the kernel time-slices between them equally (Round-Robin) every tick.

### `vTaskDelay` vs `vTaskDelayUntil`

Both put the task to sleep, but there is an important difference:

- **`vTaskDelay(50ms)`**: sleep 50ms **from now**. If the work before it took 20ms, the full cycle becomes 20+50 = 70ms. So the period **drifts** depending on execution time.
- **`vTaskDelayUntil(&xLastWakeTime, 50ms)`**: wake me **exactly every 50ms** relative to the last time I woke, regardless of how long the work took. This gives a **fixed frequency** (periodic). We use it for tasks that must run at a regular cadence.

---

## 4. `main`: From the First Line to Starting the Scheduler

This is the main file ([main.c](../Src/main.c)). Let's walk through `main` line by line:

```c
int main(void)
{
  vInitPrioGroupValue();        // (1)
  System_setup();               // (2)
  SEGGER_setup();               // (3)

  G_xESP_RX_Queue       = xQueueCreate(256, sizeof(uint8_t));   // (4)
  G_xDataMutex          = xSemaphoreCreateMutex();              // (5)
  G_xNeighborTableMutex = xSemaphoreCreateMutex();              // (6)

  SafetyEngine_voidInit();      // (7)

  xTaskCreate(vTask_SafetyEngine, "SafetyEngine_Task", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);  // (8)
  xTaskCreate(vTask_ESP_Comm,     "ESP_Comm_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 4, NULL);  // (9)
  xTaskCreate(vTask_Sensors,      "Sensors_Task",      configMINIMAL_STACK_SIZE + 256, NULL, 3, NULL);  // (10)
  xTaskCreate(vTask_Feedback,     "Feedback_Task",     configMINIMAL_STACK_SIZE + 128, NULL, 2, NULL);  // (11)
  xTaskCreate(vTask_RPi_Comm,     "RPi_Comm_Task",     configMINIMAL_STACK_SIZE + 100, NULL, 1, NULL);  // (12)

  RTOS_setup();                 // (13)  → vTaskStartScheduler()

  for (;;);                     // (14)  we should never reach this
}
```

**(1) `vInitPrioGroupValue()`** — sets up the NVIC priority grouping. FreeRTOS on Cortex-M requires all priority bits to be "preemption priority" with no "sub-priority", so its priority system works correctly. This is called first, before anything else.

**(2) `System_setup()`** — exactly the same idea as in bare-metal: configures the clock, enables GPIO/SPI/UART/Timers, and initializes the sensors and motors. **Nothing RTOS-related here** — this is pure hardware. (Code in [System.c:111](../Src/System.c#L111)).

**(3) `SEGGER_setup()`** — starts the **SEGGER SystemView** tool ([System.c:248](../Src/System.c#L248)). This tool records everything happening in the RTOS (who ran when, who slept, the interrupts...) and shows it on your PC as a timeline. It's a debugging tool, not part of the logic itself, but very useful to see how the tasks interleave.

### Creating the communication primitives (before the scheduler!)

**(4) The Queue:**

```c
G_xESP_RX_Queue = xQueueCreate(256, sizeof(uint8_t));
```

We create a **queue** that holds 256 elements, each one byte (`uint8_t`). This is the buffer where the UART **interrupt** drops bytes arriving from the ESP, and which `vTask_ESP_Comm` pulls from. (Why a queue specifically is explained in [Section 5](#5-inter-task-communication-queue-and-mutex)).

**(5) and (6) The Mutexes:**

```c
G_xDataMutex          = xSemaphoreCreateMutex();   // protects G_stHostVehicleState
G_xNeighborTableMutex = xSemaphoreCreateMutex();   // protects the DSRC neighbor table
```

A **Mutex** (Mutual Exclusion) = a single "key". Any task wanting to touch shared data must first take the key, finish, then return it. This prevents one task writing half the data while another reads it half-and-half (a race condition). We have two:

- `G_xDataMutex`: protects `G_stHostVehicleState` (speed, heading, US distances).
- `G_xNeighborTableMutex`: protects the DSRC neighbor-vehicle table.

> **Very important:** the queue and the mutexes are created **before** `vTaskStartScheduler()`, so that when the tasks run they find them ready. If you create a task that needs a mutex which hasn't been created yet, it will find `NULL` and crash.

**(7) `SafetyEngine_voidInit()`** — initializes all the ADAS modules (FCW/EEBL/BSW/DNPW/IMA) — zeroing their counters and flags ([SafetyEngine_program.c:28](../Src/SafetyEngine_program.c#L28)). It's called before the scheduler so that when `SafetyEngine_Task` runs it finds the modules properly initialized.

### Creating the Tasks — explaining each parameter

**(8) through (12)** — each line creates a task. The function signature:

```c
xTaskCreate( TaskFunction,  "Name",  StackDepth,  pvParameters,  Priority,  TaskHandle );
```

Take the first line as an example:

```c
xTaskCreate(vTask_SafetyEngine, "SafetyEngine_Task", configMINIMAL_STACK_SIZE + 256, NULL, 4, NULL);
```

| Parameter | Value | Meaning |
| --- | --- | --- |
| `TaskFunction` | `vTask_SafetyEngine` | The function containing the `for(;;)` that will run as a task |
| `Name` | `"SafetyEngine_Task"` | A text name for debugging only (appears in SystemView). Max length 10 chars (`configMAX_TASK_NAME_LEN`) |
| `StackDepth` | `130 + 256 = 386` words (≈1.5KB) | The size of this task's private stack. The numbers differ per task based on what it does (SafetyEngine and Sensors take +256 because they use many `float` variables and computations) |
| `pvParameters` | `NULL` | For passing data to the task at creation time. We don't need it, so `NULL` |
| `Priority` | `4` | The priority (0-4). 4 = our highest |
| `TaskHandle` | `NULL` | A "handle" to control the task later (suspend/delete it). We don't need it |

**Important note:** the moment you call `xTaskCreate`, the task **does not run immediately**. It is placed in the `Ready` state and waits for the scheduler to start. The scheduler hasn't started yet until line (13).

The creation order does **not** affect the run order — priority is what governs.

**(13) `RTOS_setup()` → `vTaskStartScheduler()`** ([System.c:255](../Src/System.c#L255)):

```c
void RTOS_setup(void) {
    vTaskStartScheduler();   // from here the kernel takes control of the processor
}
```

This is the single most important line in the whole file. What happens inside it:

1. The kernel automatically creates the **Idle Task** (priority 0).
2. It configures the SysTick interrupt to fire every 1ms.
3. It scans all ready tasks, finds the highest priority = **4**. We have two at priority 4 (`SafetyEngine` and `ESP_Comm`), so it starts with one of them.
4. **This function never returns.** From this moment, the CPU is entirely under the scheduler's control, and control moves between tasks based on priority and ticks.

**(14) `for(;;);`** — a safety net. If we reach here, the scheduler failed to start (usually the heap wasn't enough to create the idle task). We stay parked here instead of letting the code run off into the weeds.

---

## 5. Inter-Task Communication: Queue and Mutex

This is the most new-to-you part of the RTOS, so let's understand it well before the tasks.

### The Queue — for the ISR to talk to the Task

The problem: the UART interrupt fires when a byte arrives from the ESP. The interrupt must finish **very fast** (you can't do parsing inside the ISR). The solution: the ISR drops the byte into a queue and leaves, and the task processes it at its own pace.

```text
   byte from the ESP
        │
        ▼
  ┌──────────────┐  xQueueSendFromISR   ┌─────────────────────┐  xQueueReceive  ┌────────────────┐
  │ UART ISR     │ ───────────────────► │ G_xESP_RX_Queue     │ ──────────────► │ vTask_ESP_Comm │
  │ (very fast)  │   (drop and leave)   │ (256-byte buffer)   │ (pull & process)│  (at its pace) │
  └──────────────┘                      └─────────────────────┘                 └────────────────┘
```

Why a queue and not a plain variable? Because:

1. **Thread-safe**: the kernel handles the locking internally, so there is no race condition between the ISR and the task.
2. **Blocking**: the task can say "put me to sleep until something arrives in the queue" — so it consumes no CPU while waiting.

#### The life of a single byte

Let's trace exactly what happens to **one byte** arriving from the ESP:

```text
1. A byte lands in the UART hardware register → the RXNE flag is set
   → the USART1 interrupt fires.
2. The ISR (vESP_UART_RX_Callback) reads the byte and does ONE xQueueSendFromISR
   → the byte is now sitting in G_xESP_RX_Queue. The ISR returns (microseconds).
3. The byte waits in the queue (capacity 256) until the task picks it up.
4. vTask_ESP_Comm, when it runs, does xQueueReceive → pulls the byte out and
   feeds it to DSRC_RxCallback (the protocol parser).
```

So: **one byte = one interrupt = one `xQueueSendFromISR`**. Bytes accumulate in the queue; the task drains them on its own schedule. The 256-byte capacity is the safety margin: even if a burst arrives while the task is busy with something else, nothing is lost as long as the task drains it before 256 pile up.

#### Does the task busy-wait in `xQueueReceive`, or does it keep working?

This is the most important point. Look at the receive call in `vTask_ESP_Comm`:

```c
if (xQueueReceive(G_xESP_RX_Queue, &byte, pdMS_TO_TICKS(10)) == pdTRUE)
```

The third argument (`10ms`) is a **block-with-timeout**. It does **not** busy-poll. The behavior:

- **If the queue has a byte** → returns immediately with `pdTRUE`, `byte` is filled. The task processes it.
- **If the queue is empty** → the task enters the **Blocked** state and **consumes zero CPU**. The scheduler runs other tasks. Then one of two things ends the wait:
  - **A byte arrives within 10ms** (the ISR does `xQueueSendFromISR`) → the kernel **wakes the task immediately** (not at the next tick — right away if the woken task is higher priority, via `portYIELD_FROM_ISR`). `xQueueReceive` returns `pdTRUE`.
  - **10ms pass with no byte** → `xQueueReceive` returns `pdFALSE`. The `if` is skipped, and the task **carries on with the rest of the loop anyway** (neighbor-table maintenance + the TX check).

That 10ms timeout is the clever part: it makes the task **both event-driven and periodic at the same time**:

- *Event-driven*: a byte wakes it instantly, so V2X reception is responsive.
- *Periodic*: even with **zero** incoming traffic, it still wakes every ~10ms to run `DSRC_RemoveStale` (purge silent vehicles) and check whether it's time to broadcast. If it blocked **forever** (`portMAX_DELAY`) instead, a silent radio would freeze all that maintenance.

The inner drain loop uses a timeout of **`0`**:

```c
while (xQueueReceive(G_xESP_RX_Queue, &byte, 0) == pdTRUE) { ... }
```

A timeout of `0` means "**never block** — give me a byte if one is there, otherwise return `pdFALSE` instantly." This is how we empty a whole burst in one go without ever sleeping between bytes.

> **In short:** the task is *asleep* (not spinning) while waiting, it wakes the instant a byte arrives, and the 10ms cap guarantees it never stays asleep so long that its periodic maintenance is starved.

### The Mutex — to protect shared data

The problem: `G_stHostVehicleState` is written by `vTask_Sensors`, and read by `vTask_SafetyEngine`, `vTask_ESP_Comm` and `vTask_RPi_Comm`. If Sensors is writing the speed (4 bytes) and gets preempted mid-write by a higher-priority task that reads the value → it reads a half-old-half-new value (garbage).

The solution: anyone wanting to touch this data takes the mutex first:

```c
xSemaphoreTake(G_xDataMutex, portMAX_DELAY);  // take the key (or sleep until it's available)
//  ... read/write the shared data (Critical Section) ...
xSemaphoreGive(G_xDataMutex);                 // return the key
```

- `portMAX_DELAY` means: "wait forever until the key becomes available" (don't give up on a timeout).
- When a task takes the key and another requests it, the second enters the `Blocked` state until the first returns it.

**Golden rule:** the part between `Take` and `Give` (the critical section) must be **as short as possible**. That's why in `vTask_Sensors` we compute everything into local variables first, then take the mutex for just a few moments to copy the results (we'll see this shortly).

### The Two Mutexes in Our Project: Why Two? What Does Each Protect?

We have **two pieces** of shared data, each written by one task and read by several. That's why we made **two separate mutexes** instead of one, so that a task wanting the first piece of data doesn't get blocked by a task wanting the second (with a single mutex, everyone would wait on each other for no reason).

They are declared in [main.c:46](../Src/main.c#L46) and created in [main.c:71](../Src/main.c#L71):

```c
SemaphoreHandle_t G_xDataMutex;           // protects our own vehicle's state
SemaphoreHandle_t G_xNeighborTableMutex;  // protects the neighbor-vehicle table
...
G_xDataMutex          = xSemaphoreCreateMutex();
G_xNeighborTableMutex = xSemaphoreCreateMutex();
```

#### ① `G_xDataMutex` — protects `G_stHostVehicleState` (our own vehicle's state)

This protects the `G_stHostVehicleState` struct (defined in [System.h:120](../Inc/System/System.h#L120)), which holds **our own vehicle's state**: the 6 ultrasonic distances + speed + heading + pitch/roll + position. Its size is 13 × `float` ≈ 52 bytes, so writing it takes several instructions → it can be preempted mid-write.

| Task | Writes or reads? | What it does |
| --- | --- | --- |
| `vTask_Sensors` | ✍️ **writes** (the sole producer) | copies the US + MPU readings into the struct — [main.c:188](../Src/main.c#L188) |
| `vTask_SafetyEngine` | 📖 reads | the brain takes speed/heading/distances for the ADAS computations — [main.c:115](../Src/main.c#L115) |
| `vTask_ESP_Comm` | 📖 reads | takes speed/heading to broadcast them over V2X — [main.c:314](../Src/main.c#L314) |
| `vTask_RPi_Comm` | 📖 reads | takes speed/heading for the telemetry — [main.c:339](../Src/main.c#L339) |

**The dangerous scenario it prevents:** `vTask_Sensors` (priority 3) is writing the speed (4 bytes) → mid-write it gets preempted by `vTask_SafetyEngine` (priority 4, higher) which reads the speed → it reads 2 old bytes + 2 new bytes = **a completely wrong number (a torn read)**. The mutex prevents this: as long as Sensors holds the key, any reader waits until the write completes.

#### ② `G_xNeighborTableMutex` — protects the DSRC neighbor table

This protects the **neighbor-vehicle table** (vehicles whose V2X messages have arrived). This table lives inside the DSRC module and you reach it via `DSRC_GetTable()` / `DSRC_GetCount()`. Why does it need protection? Because one task modifies it (adds/removes vehicles) while another iterates over it at the same time → if preempted mid-modification it could read a half-modified table or iterate over a removed element.

| Task | Writes or reads? | What it does |
| --- | --- | --- |
| `vTask_ESP_Comm` | ✍️ **modifies** (the producer) | adds new vehicles (`DSRC_Update`) and removes stale ones (`DSRC_RemoveStale`) — [main.c:299](../Src/main.c#L299) |
| `vTask_SafetyEngine` | 📖 reads | the brain iterates over every vehicle in the table to assess risk — [main.c:114](../Src/main.c#L114) |

**The dangerous scenario it prevents:** `vTask_ESP_Comm` removes a vehicle that went silent (`DSRC_RemoveStale`) → mid-operation it gets preempted by `vTask_SafetyEngine` which is iterating over the table → it iterates over an element that was **just removed / or the count changed** → undefined behavior that could crash the system. The mutex makes the modification and the read atomic with respect to each other.

#### Why not a single mutex for everything?

We could use one mutex protecting both, but then if `vTask_RPi_Comm` just wants to read our vehicle's speed, it would wait for `vTask_ESP_Comm` which holds the mutex because it's modifying the neighbor table — **even though they are two unrelated pieces of data**. Two separate mutexes reduce contention (finer-grained locking): each task only waits on the data it actually needs.

#### Relation to deadlock

`vTask_SafetyEngine` is the **only** task that needs both mutexes together (because it reads our vehicle's state + the neighbor table in the same pass). Everyone else takes only one mutex at a time. This makes deadlock prevention easy (details in [Section 9](#9-why-are-the-priorities-and-locks-arranged-this-way)).

### Priority Inheritance (a Mutex feature)

The mutex in FreeRTOS (unlike the semaphore) does something clever called **priority inheritance**: if a low-priority task is holding the key and a high-priority task is waiting for it, the kernel **temporarily raises** the low task's priority until it returns the key. This prevents a problem called "priority inversion" where a low task stalls a high one for a long time.

---

## 6. Every Task, Line by Line

### 6.1 — `vTask_SafetyEngine` (the brain — priority 4)

```c
void vTask_SafetyEngine(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();   // (1)

  for(;;)                                            // (2)
  {
    xSemaphoreTake(G_xNeighborTableMutex, portMAX_DELAY);  // (3)
    xSemaphoreTake(G_xDataMutex,          portMAX_DELAY);  // (4)

    SafetyEngine_voidUpdate();                              // (5)

    xSemaphoreGive(G_xDataMutex);                           // (6)
    xSemaphoreGive(G_xNeighborTableMutex);                  // (7)

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));     // (8)
  }
}
```

**(1)** `xTaskGetTickCount()` returns the number of ticks (milliseconds) elapsed since the scheduler started. We store it as a starting point for `vTaskDelayUntil` later. This line executes **only once** when the task first starts (not inside the loop).

**(2)** The infinite loop. Every FreeRTOS task **must** have a `for(;;)` and never exit it. If it does exit, it must call `vTaskDelete(NULL)` to delete itself, otherwise the code crashes.

**(3) and (4)** We take **both keys**: first the neighbor table, then the vehicle data. Why both? Because `SafetyEngine_voidUpdate` reads from both (it reads our vehicle's state + iterates over the neighbor table). **The acquisition order matters a lot**: NeighborTable first, Data second — we'll see why in [Section 9](#9-why-are-the-priorities-and-locks-arranged-this-way).

> If one of the two keys isn't available (another task holds it), `SafetyEngine` enters `Blocked` and the CPU goes elsewhere until the key is released.

**(5)** `SafetyEngine_voidUpdate()` ([SafetyEngine_program.c:38](../Src/SafetyEngine_program.c#L38)) — this is **the actual brain**. In a single pass:

- It takes a snapshot of our vehicle's speed, heading and distances.
- It iterates over every neighbor vehicle in the table, and each module (FCW/EEBL/BSW/DNPW/IMA) inspects it.
- Finally it aggregates the result into `G_u8SystemFlags` (a bitmap: each bit = a module raised an alert).

This is exactly the bare-metal `run_adas_logic()`, except now it is protected by the two keys and runs every 50ms regularly.

**(6) and (7)** We return the two keys in the **reverse order** of acquisition (Data first since it was acquired last, then NeighborTable). This is the correct convention in nested locking.

**(8)** `vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50))`:

- Puts the task to sleep until 50ms have passed since `xLastWakeTime` (not since now).
- Automatically updates `xLastWakeTime` for the next time.
- Result: SafetyEngine runs **exactly every 50ms** (20 times per second), regardless of how long the computation took.
- While it sleeps those 50ms, the CPU goes to the lower-priority tasks (Sensors, Feedback, RPi). This is the home of all the safety logic.

---

### 6.2 — `vTask_Sensors` (reading the sensors — priority 3)

```c
#define SENSORS_CYCLE_GAP_MS  10U

void vTask_Sensors(void *pvParameters)
{
  static float             local_speed_ms = 0.0f;     // (1)
  static MPU9250_Position_t local_pos     = {0};
  MPU9250_Data_t  mpu_data               = {0};

  const US_Config_t *sensors[6] = {                    // (2)
      &FrontUS[0], &BackUS[0],   /* Left   */
      &FrontUS[1], &BackUS[1],   /* Center */
      &FrontUS[2], &BackUS[2]    /* Right  */
  };

  TickType_t xPrevTick = xTaskGetTickCount();          // (3)

  for(;;)
  {
    TickType_t xNow = xTaskGetTickCount();             // (4)
    float dt = (float)(xNow - xPrevTick) * 0.001f;     // (5)  ms → seconds
    if (dt <= 0.0f) dt = 0.010f;                       // (6)  guard: first run
    xPrevTick = xNow;                                  // (7)

    float    us[6];                                    // (8)
    uint16_t raw;
    for (uint8_t i = 0; i < 6; i++)
    {
      us[i] = 400.0f;                                  // (9)  default = nothing nearby
      if (US_u16ReadDistance_cm(sensors[i], &raw) == OK)
        us[i] = (float)raw;
    }

    MPU9250_enumReadData(&mpu_data);                   // (10)
    float pitch = 0.0f, roll = 0.0f, heading = 0.0f;
    MPU9250_enumGetAttitude(&mpu_data, dt, &pitch, &roll);
    MPU9250_enumGetHeading(&mpu_data, &heading);
    MPU9250_enumGetSpeed(&mpu_data, dt, &local_speed_ms);
    MPU9250_enumGetPosition(&mpu_data, local_speed_ms, heading, pitch, dt, &local_pos);

    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);       // (11)
    G_stHostVehicleState.FrontLeftUS   = us[0];
    G_stHostVehicleState.BackLeftUS    = us[1];
    G_stHostVehicleState.FrontCenterUS = us[2];
    G_stHostVehicleState.BackCenterUS  = us[3];
    G_stHostVehicleState.FrontRightUS  = us[4];
    G_stHostVehicleState.BackRightUS   = us[5];
    G_stHostVehicleState.Speed   = local_speed_ms * 100.0f;  // m/s → cm/s
    G_stHostVehicleState.Heading = heading;
    G_stHostVehicleState.Pitch   = pitch;
    G_stHostVehicleState.Roll    = roll;
    G_stHostVehicleState.PosX    = local_pos.X;
    G_stHostVehicleState.PosY    = local_pos.Y;
    G_stHostVehicleState.PosZ    = local_pos.Z;
    xSemaphoreGive(G_xDataMutex);                      // (12)

    vTaskDelay(pdMS_TO_TICKS(SENSORS_CYCLE_GAP_MS));   // (13)
  }
}
```

**(1)** The `static` variables — speed and position must persist between iterations (speed is accumulated by integrating acceleration). The `static` keyword makes the variable keep its value between iterations of the `for(;;)` instead of resetting each loop.

**(2)** The sensor order is **interleaved** (front then back of the same side): the idea is that any two consecutive reads are geographically distant so there's no **acoustic cross-talk** between the ultrasonics — one sensor hearing another's echo.

**(3)** We record the start time to compute `dt` (the time delta) — important for integrating acceleration and speed.

**(4)–(7) Computing the actual `dt`:** this task does not run at a fixed frequency (the scan itself lengthens or shortens based on object distance). So instead of assuming a fixed dt, we measure it for real: `dt = now − last iteration`. This keeps the speed computations accurate no matter how the iteration period changes. Line (6) is a guard for the first iteration (no previous time yet).

**(8)–(9) Reading the 6 ultrasonics:**

- We set the default value to 400cm (= out of range, the limit is 2 meters).
- `US_u16ReadDistance_cm` is **interrupt-driven**: it sends a Trigger pulse, then the **task sleeps (Blocked)** until an interrupt tells it the echo came back. **Here is the home of the efficiency:** instead of the CPU standing still waiting for the echo (up to 12ms for a far object), the task sleeps and the CPU goes to `Feedback` or others. That's why we can read 6 sensors back-to-back without consuming CPU.

**(10)** Reading the MPU9250 (gyro/accelerometer/magnetometer) and computing pitch/roll/heading/speed/position. It all gets computed into **local variables** (`pitch`, `roll`, `heading`, `local_speed_ms`, `local_pos`).

**(11)–(12) Publishing under the key:** only now do we take `G_xDataMutex` and copy all the computed results into the shared struct. Notice: **all the computation was done outside the mutex**, and the mutex is locked for a very short part (copying numbers only). This minimizes the time any other task waits for the key. Notice also the speed conversion from m/s to cm/s so the TTC computations in the ADAS come out in seconds.

**(13)** `vTaskDelay(10ms)` — a small rest after each full scan. Its purpose:

- Give the lower-priority tasks (Feedback, RPi) a guaranteed chance at the CPU.
- Set the minimum spacing between scans.

> Why `vTaskDelay` here and not `vTaskDelayUntil` like SafetyEngine? Because the scan itself has a variable length (adaptive — it follows the echo flight time), so we don't want a fixed frequency. We just want "take a 10ms break after you finish" regardless of how long the scan was. The result: a full cycle between ~25ms (near objects) and ~82ms (everything far).

---

### 6.2.1 Deep Dive: How the Ultrasonic Read Sleeps Through the Echo (no busy-wait)

This is the line in `vTask_Sensors` that does the magic:

```c
if (US_u16ReadDistance_cm(sensors[i], &raw) == OK)
```

The whole claim of the Sensors task is "the task SLEEPS during each echo flight, so the CPU is free." This section proves exactly how, because it's the single most important efficiency trick in the project. The driver lives in [US_prog.c](../Src/US_prog.c).

#### The problem: why the naive way is terrible

An HC-SR04 works like a bat: you fire a short trigger pulse, the sensor sends an ultrasonic chirp, and it raises its ECHO pin HIGH for exactly as long as the sound takes to fly out, bounce off an object, and come back. **Distance is encoded as the WIDTH of that HIGH pulse.** Sound travels ~343 m/s, so for our 2-meter cap the round trip (4 m) takes up to **~12 ms**.

The naive bare-metal way to measure that width:

```c
while (echo_pin == LOW);        // wait for the rising edge
uint32_t t1 = timer_now();
while (echo_pin == HIGH);       // wait for the falling edge   ← BURNS THE CPU FOR UP TO 12ms
uint32_t width = timer_now() - t1;
```

Those two `while` loops are **busy-waiting**: the CPU spins doing nothing for up to 12ms *per sensor*. With 6 sensors that's up to **72ms of pure wasted CPU every scan**, during which nothing else — not the brain, not the comms — can run. That's exactly the bare-metal disease we're escaping.

#### The fix = two tricks stacked on top of each other

**Trick 1 — let the hardware do the measuring (Timer Input Capture).**
Instead of watching the ECHO pin with a `while` loop, we wire ECHO to a **timer Input-Capture (IC) channel**. The timer runs continuously at 1µs resolution ([US_prog.c:154](../Src/US_prog.c#L154) sets the prescaler for 1MHz). When configured for input capture, the **hardware automatically records the timer value the instant an edge occurs** — no CPU involvement. So the CPU never has to watch the pin; the timer latches the rising-edge timestamp and the falling-edge timestamp by itself, and fires an interrupt to report each one.

**Trick 2 — let the task sleep until the hardware is done (a binary semaphore).**
Between firing the trigger and the echo coming back, the task has nothing to do. So it **blocks on a binary semaphore** (`US_xDoneSem`). It uses zero CPU. When the falling edge finally arrives, the IC interrupt computes the distance and "gives" the semaphore, which wakes the task. The ~12ms of echo flight becomes ~12ms of **free CPU** for the other tasks.

#### A new primitive: the binary semaphore (vs mutex vs queue)

We've now met all three FreeRTOS communication primitives. Here's how the binary semaphore differs:

| Primitive | Used for | In this project |
| --- | --- | --- |
| **Queue** | passing *data* (bytes) from ISR → task | UART RX (`G_xESP_RX_Queue`) |
| **Mutex** | *protecting* shared data; taken & given by the **same** task | `G_xDataMutex`, `G_xNeighborTableMutex` |
| **Binary semaphore** | *signaling* "an event happened" from ISR → task (no data) | US echo done (`US_xDoneSem`) |

The key difference from a mutex: a binary semaphore is **given by one context (the ISR) and taken by another (the task)** — it's a one-way "ding!" notification, like a doorbell. A mutex must be given back by whoever took it. Think of the binary semaphore as a token that starts empty; the task tries to take it and blocks because it's empty; the ISR drops the token in (`give`) when the echo completes; that unblocks the task.

It's created once in [US_prog.c:183](../Src/US_prog.c#L183): `US_xDoneSem = xSemaphoreCreateBinary();`

#### The task side, line by line — `US_u16ReadDistance_cm` ([US_prog.c:203](../Src/US_prog.c#L203))

```c
(void)xSemaphoreTake(US_xDoneSem, 0);                          // (1) drain stale signal

US_Active.timer   = pxSensor->Timer;                           // (2) set up the
US_Active.channel = ch;                                        //     shared measurement
US_Active.maxval  = ... ;                                      //     context for the ISR
US_Active.phase   = US_PHASE_RISING;                           //     start at the rising edge

TIM_vSetICPolarity(... TIM_POLARITY_HIGH);                     // (3) arm: capture RISING edge
TIMx->SR = ~(1UL << (ch + 1));                                 // (4) clear stale capture flag
TIM_vEnableCCInterrupt(pxSensor->Timer, pxSensor->Channel);    // (5) enable the IC interrupt

US_vSendTrigger(pxSensor);                                     // (6) fire the trigger pulse

if (xSemaphoreTake(US_xDoneSem, pdMS_TO_TICKS(US_TASK_TIMEOUT_MS)) == pdTRUE &&  // (7) SLEEP
    US_Active.phase == US_PHASE_DONE)
{
    *pu16Dist_cm = US_Active.dist_cm;                          // (8) success → return distance
    return OK;
}

TIM_vDisableCCInterrupt(pxSensor->Timer, pxSensor->Channel);   // (9) timeout → disarm
return TIMEOUT_STATE;                                          //     (no object in range)
```

**(1)** Throws away any leftover "done" signal from a previous measurement that completed late, so we start clean.
**(2)** Fills the single shared `US_Active` struct that the ISR will read: which timer/channel we're using and that we're expecting the **rising** edge first. (One measurement at a time → one shared struct is safe; more on that below.)
**(3)** Arms the IC hardware to capture the **rising** edge first.
**(4)** Clears the channel's stale capture flag so an old edge doesn't trigger a false interrupt.
**(5)** Enables the capture-compare interrupt for this channel — now an edge will call the ISR.
**(6)** Fires the trigger pulse: 40µs low, 20µs high, low ([US_prog.c:62](../Src/US_prog.c#L62)). This is a tiny busy-delay of microseconds (negligible) using TIM6. After this, the sensor starts chirping.
**(7) THE HEADLINE LINE.** `xSemaphoreTake(US_xDoneSem, 12ms)` puts the task to **sleep (Blocked)**. It will not run again until one of:

- the ISR `gives` the semaphore (echo came back, distance ready) → returns `pdTRUE`, **or**
- 12ms pass with no echo → returns `pdFALSE` (timeout).

**During this sleep the CPU is completely free** for SafetyEngine/ESP_Comm/Feedback/RPi. This is the whole point.
**(8)** Woken with a valid result → copy the distance out and return `OK`.
**(9)** Timed out → no echo came back, meaning **no object within range** (the 2m cap). We disarm the interrupt and return `TIMEOUT_STATE`. Back in `vTask_Sensors`, the `if (... == OK)` is false, so the value stays at its default `400.0f` (= "clear / out of range").

#### The ISR side, line by line — `US_CC_Handler` ([US_prog.c:88](../Src/US_prog.c#L88))

This runs in timer-interrupt context, **twice** per measurement: once for the rising edge, once for the falling edge. It's a tiny 2-state machine:

```c
if (Copy_eTimer != US_Active.timer || Copy_u8Channel != US_Active.channel)
    return;                                                    // (a) not our measurement → ignore

if (US_Active.phase == US_PHASE_RISING)                        // (b) FIRST interrupt: rising edge
{
    US_Active.t1    = Copy_u32Capture;                         //     record start timestamp
    US_Active.phase = US_PHASE_FALLING;                        //     now expect the falling edge
    TIM_vSetICPolarity(..., TIM_POLARITY_LOW);                 //     re-arm to capture FALLING
}
else if (US_Active.phase == US_PHASE_FALLING)                  // (c) SECOND interrupt: falling edge
{
    uint32_t high = Copy_u32Capture - US_Active.t1;            //     pulse width in µs (wrap-safe)
    US_Active.dist_cm = (uint16_t)(high / US_SOUND_SPEED_FACTOR);//   µs → cm  (÷58)
    US_Active.phase   = US_PHASE_DONE;
    TIM_vDisableCCInterrupt(...);                              //     stop capturing on this channel
    BaseType_t xHPW = pdFALSE;
    xSemaphoreGiveFromISR(US_xDoneSem, &xHPW);                 // (d) WAKE the task
    portYIELD_FROM_ISR(xHPW);                                  // (e) switch to it now if it's higher
}
```

**(a)** Safety filter: ignore any capture event that isn't for the measurement we're currently doing.
**(b) First interrupt = the ECHO pin went HIGH.** The hardware already latched the exact timer value (`Copy_u32Capture`) — the ISR just stores it as `t1` (the start of the pulse), flips the state to expect the falling edge, and re-arms the IC to capture a **falling** edge next. Then it returns; the task is still asleep.
**(c) Second interrupt = the ECHO pin went LOW.** The pulse is over. The ISR computes `high = t2 - t1` (the pulse width in µs, with wrap-around handling for when the timer rolls over), and converts to centimeters by dividing by `US_SOUND_SPEED_FACTOR` (58 — the standard HC-SR04 figure: ~58µs of round-trip flight per cm). It stores the result and marks the phase `DONE`.
**(d)** `xSemaphoreGiveFromISR(US_xDoneSem, ...)` — drops the token in the semaphore, which **wakes the task** that was blocked on line (7) above. This is the exact ISR→task counterpart of the UART's `xQueueSendFromISR`.
**(e)** `portYIELD_FROM_ISR(xHPW)` — same deferred-switch idea as the UART ISR: if the woken Sensors task is higher priority than whatever was running, switch to it right after the ISR returns. (Sensors is priority 3, so it preempts Feedback/RPi/Idle immediately, but waits politely if SafetyEngine/ESP_Comm at priority 4 are running.)

> Notice the IC interrupt's NVIC priority is also set to 6 ([US_prog.c:189](../Src/US_prog.c#L189)) — same rule as the UART: it must be ≥ `configMAX_SYSCALL_INTERRUPT_PRIORITY` (5) because it calls a `...FromISR` function.

#### Why one sensor at a time (sequential)?

There is exactly **one** `US_Active` struct and **one** `US_xDoneSem` shared by all 6 sensors. That's only safe because `vTask_Sensors` reads the sensors **one after another** — it calls `US_u16ReadDistance_cm` for sensor 0, blocks until it's done, then sensor 1, and so on. Two benefits fall out of this:

1. **Race-free with trivial code:** since only one measurement is ever in flight, a single shared context and a single semaphore need no extra locking.
2. **No acoustic cross-talk:** only one sensor is chirping at any moment, so no sensor can hear another sensor's echo and report a false reading.

#### One reading's complete journey

```text
1. vTask_Sensors calls US_u16ReadDistance_cm for sensor i.
2. The function arms the IC interrupt and fires the trigger pulse (µs).
3. It calls xSemaphoreTake(US_xDoneSem, 12ms) → TASK SLEEPS. CPU is now free.
4. ── meanwhile, other tasks run for the whole echo flight (up to ~12ms) ──
5. ECHO goes HIGH  → IC interrupt → US_CC_Handler records t1, re-arms for falling. (task still asleep)
6. ECHO goes LOW   → IC interrupt → US_CC_Handler computes distance, gives the semaphore.
7. The give wakes vTask_Sensors → xSemaphoreTake returns pdTRUE.
8. The function copies US_Active.dist_cm into *raw and returns OK.
9. vTask_Sensors moves on to sensor i+1 and repeats.
   (If step 6 never happens within 12ms → timeout → TIMEOUT_STATE → treated as 400cm "clear".)
```

So a full 6-sensor scan spends almost all of its wall-clock time **asleep**, handing the CPU to the rest of the system — yet the readings are precise to 1µs because the *hardware timer*, not a software loop, did the measuring.

---

### 6.3 — `vTask_Feedback` (the muscles — priority 2)

```c
void vTask_Feedback(void *pvParameters)
{
  for(;;)
  {
    if (G_u8SystemFlags == 0)                  // (1)
    {
      LED_TurnOff(&FrontR_LED);                // (2) all safe
      LED_TurnOff(&FrontL_LED);
      LED_TurnOff(&BackR_LED);
      LED_TurnOff(&BackL_LED);
      LED_TurnOff(&Interior_LED);
      BUZ_Off(&V2X_Buzzer);
      L298N_enumCarMoveForward(&RightMotor, &LeftMotor);
      G_eMotorGlobalCommand = CMD_MOVE_FORWARD;
    }
    else                                        // (3) an alert is active
    {
      LED_TurnOn(&Interior_LED);                // (4) general driver alert
      BUZ_On(&V2X_Buzzer);

      uint8_t fcw       = (G_u8SystemFlags & SYSFLG_FCW)  ? FCW_u8GetAlertLevel()  : 0;   // (5)
      uint8_t eebl      = (G_u8SystemFlags & SYSFLG_EEBL) ? EEBL_u8GetAlertLevel() : 0;
      uint8_t bsw_left  = (G_u8SystemFlags & SYSFLG_BSW)  ? BSW_u8GetLeftFlag()    : 0;
      uint8_t bsw_right = (G_u8SystemFlags & SYSFLG_BSW)  ? BSW_u8GetRightFlag()   : 0;

      if (fcw >= 1) { LED_TurnOn(&FrontR_LED);  LED_TurnOn(&FrontL_LED);  }                // (6)
      else          { LED_TurnOff(&FrontR_LED); LED_TurnOff(&FrontL_LED); }

      if (eebl >= 1 || bsw_right) { LED_TurnOn(&BackR_LED);  }                             // (7)
      else                        { LED_TurnOff(&BackR_LED); }

      if (eebl >= 1 || bsw_left)  { LED_TurnOn(&BackL_LED);  }
      else                        { LED_TurnOff(&BackL_LED); }

      if (fcw == 2) { L298N_enumCarStop(&RightMotor, &LeftMotor);        G_eMotorGlobalCommand = CMD_STOP;         }  // (8)
      else          { L298N_enumCarMoveForward(&RightMotor, &LeftMotor); G_eMotorGlobalCommand = CMD_MOVE_FORWARD; }
    }

    vTaskDelay(pdMS_TO_TICKS(25));              // (9)
  }
}
```

**(1)** It reads `G_u8SystemFlags` — the bitmap that SafetyEngine wrote. Notice it does **not** take a mutex! Why? Because `G_u8SystemFlags` is a single byte (`volatile uint8_t`), and reading/writing a single byte on Cortex-M is an **atomic** operation (it can't be interrupted mid-way). So no mutex is needed.

**(2)** If flags == 0 then everything is safe → turn off all LEDs and the buzzer, and move forward.

**(3)–(4)** If any bit is set (any module raised an alert) → trigger the general alert: the interior driver LED + the buzzer always on.

**(5)** It reads the **severity** of each alert from each module's getter, but only if its bit is set in the bitmap (`& SYSFLG_xxx`). The `?:` says: "if the bit is set, get the severity, otherwise zero".

**(6)–(7)** External per-module indicators: FCW → front LEDs, EEBL/BSW → rear LEDs (right/left by side).

**(8)** **The only decision on the motor:** only if FCW reaches the **critical level (== 2)** do we stop the car. Otherwise we keep moving forward. Notice `vTask_Feedback` is the **only** writer of `G_eMotorGlobalCommand` and the only one that touches the motor — a clean split between the brain (decides) and the muscles (executes).

**(9)** `vTaskDelay(25ms)` — runs ~40 times per second. Fast enough for the reaction to feel immediate to the driver, slow enough that it doesn't waste CPU for nothing.

> Notice this task has priority 2 (low). That means if SafetyEngine or Sensors want the CPU, they preempt it at any moment. That's logical: it's more important to compute the decision correctly (brain) before executing it (muscles).

---

### 6.4 — `vTask_ESP_Comm` (V2X communication — priority 4)

This is the most complex task because it does two things: **receive (RX)** and **transmit (TX)** in the same loop.

```c
void vTask_ESP_Comm(void *pvParameters)
{
  TickType_t xLastTXTime = xTaskGetTickCount();          // (1)

  for(;;)
  {
    /* ── RX: event-driven processing ── */
    uint8_t byte;
    if (xQueueReceive(G_xESP_RX_Queue, &byte, pdMS_TO_TICKS(10)) == pdTRUE)   // (2)
    {
      DSRC_RxCallback(byte);                              // (3)
      while (xQueueReceive(G_xESP_RX_Queue, &byte, 0) == pdTRUE)              // (4)
      {
        DSRC_RxCallback(byte);
      }
    }

    /* ── neighbor-table maintenance — every iteration (~10ms) ── */
    xSemaphoreTake(G_xNeighborTableMutex, portMAX_DELAY);  // (5)
    DSRC_Update();                                         // (6)
    DSRC_RemoveStale((uint32_t)xTaskGetTickCount());       // (7)
    xSemaphoreGive(G_xNeighborTableMutex);                 // (8)

    /* ── TX: periodic broadcast every 100ms ── */
    if ((xTaskGetTickCount() - xLastTXTime) >= pdMS_TO_TICKS(100))   // (9)
    {
      xLastTXTime = xTaskGetTickCount();                   // (10)

      Neighbor my_data = {0};                              // (11)
      my_data.vehicle_id  = VEHICLE_ID;
      my_data.last_update = (uint32_t)xTaskGetTickCount();

      xSemaphoreTake(G_xDataMutex, portMAX_DELAY);         // (12)
      my_data.speed   = G_stHostVehicleState.Speed;
      my_data.heading = G_stHostVehicleState.Heading;
      xSemaphoreGive(G_xDataMutex);                        // (13)

      my_data.fcw_flag  = FCW_u8GetFlag();                 // (14)
      my_data.dnpw_flag = DNPW_u8GetFlag();
      my_data.ima_flag  = IMA_u8GetFlag();

      DSRC_SendNeighbor(&my_data);                         // (15)
    }
  }
}
```

**(1)** We record the last-transmit time — we'll use it to know whether 100ms have passed since the last broadcast.

**(2) Event-driven RX:**

```c
xQueueReceive(G_xESP_RX_Queue, &byte, pdMS_TO_TICKS(10))
```

It tries to pull a byte from the queue. The third parameter (`10ms`) is a **timeout**:

- If there's a byte → it returns it immediately in `byte` and returns `pdTRUE`.
- If the queue is empty → the task **sleeps (Blocked) for up to 10ms** or until a byte arrives (whichever comes first). This means the task **consumes no CPU** while waiting for data — this is the fundamental difference from the bare-metal version, which busy-polled continuously.

**(3)** The first byte arrived → pass it to `DSRC_RxCallback` (the protocol state machine that assembles bytes into a complete packet).

**(4) Drain the rest of the queue:** now we pull **every** remaining byte with a timeout of **zero** (`0`). That means "give me whatever is there and don't make me wait if it's empty". The idea: if a burst of bytes arrived together, we process them all in one iteration instead of sleeping between them.

**(5)–(8) Neighbor-table maintenance — every iteration:**

- We take `G_xNeighborTableMutex` (the neighbor-table key only).
- `DSRC_Update()` [(6)] flushes any packets that assembled fully into the table.
- `DSRC_RemoveStale()` [(7)] removes any vehicle that has been silent for a while (went out of range / powered off). **The comment in the code matters:** this part must run **every iteration even if no bytes arrived**, otherwise a vehicle that went silent would stay in the table forever.

**(9)–(10) Checking the transmit timing:** we compare the current time to the last transmit. If 100ms passed → time to broadcast. This is a different pattern from `vTaskDelayUntil`: we do **not sleep** the task here, because the task must stay awake to process RX every 10ms. So we do the timing **manually** by comparing ticks.

**(11)** We prepare a packet (`Neighbor`) with our data: the ID and the time.

**(12)–(13)** We take `G_xDataMutex` (the data key) and copy speed and heading. **Notice:** here we took `G_xDataMutex` **alone** — not together with `G_xNeighborTableMutex`. And above, we also took `G_xNeighborTableMutex` alone. **ESP_Comm never holds both keys at the same time.** This is the basis of deadlock prevention (full explanation in [Section 9](#9-why-are-the-priorities-and-locks-arranged-this-way)).

**(14)** We read the module flags (FCW/DNPW/IMA) — these are atomic bytes, so no mutex.

**(15)** `DSRC_SendNeighbor` broadcasts the packet over the UART to the ESP, which in turn broadcasts it over ESP-NOW to all the vehicles around it.

> **Why priority 4 (the highest) like SafetyEngine?** Because V2X communication is time-sensitive — if we're late processing incoming bytes, the queue (256 bytes) could overflow and we'd lose data. So we made it as high as the brain. The two share the CPU via Round-Robin since they have the same priority.

---

### 6.5 — `vTask_RPi_Comm` (telemetry to the Raspberry Pi — priority 1)

```c
void vTask_RPi_Comm(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount();    // (1)

  for (;;)
  {
    RPi_Packet_t pkt;                                // (2)

    xSemaphoreTake(G_xDataMutex, portMAX_DELAY);     // (3)
    pkt.speed   = G_stHostVehicleState.Speed;
    pkt.heading = G_stHostVehicleState.Heading;
    xSemaphoreGive(G_xDataMutex);                    // (4)

    pkt.sys_flags = G_u8SystemFlags;                 // (5)

    pkt.start = 0xAAU;                               // (6)
    pkt.end   = 0x55U;

    const uint8_t *p = (const uint8_t *)&pkt.sys_flags;   // (7) compute the checksum
    uint8_t csum = 0;
    for (uint8_t i = 0; i < (1u + sizeof(float) + sizeof(float)); i++)
      csum ^= p[i];
    pkt.checksum = csum;

    const uint8_t *raw = (const uint8_t *)&pkt;      // (8) send byte by byte
    for (uint8_t i = 0; i < sizeof(RPi_Packet_t); i++)
      USART_enumTransmit(&RPi_UART, raw[i]);

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));  // (9)
  }
}
```

**(1)** The starting point for the periodic timing.

**(2)** We prepare the packet we'll send (the struct is defined in [System.h:151](../Inc/System/System.h#L151) with `__attribute__((packed))` so there's no padding between fields).

**(3)–(4)** We take `G_xDataMutex` and copy speed and heading (same pattern: a very short critical section).

**(5)** We read `G_u8SystemFlags` — an atomic byte, no mutex.

**(6)** We set the framing bytes: start `0xAA` and end `0x55` so the Raspberry Pi can identify the start and end of each packet in the stream.

**(7)** Compute the **checksum** as an XOR over all payload bytes (the flags + 4 speed bytes + 4 heading bytes), so the other end can verify the data arrived intact.

**(8)** We send the struct byte by byte over `RPi_UART` (UART4).

**(9)** `vTaskDelayUntil(100ms)` — fixed telemetry 10 times per second.

> **Why priority 1 (the lowest)?** Because telemetry is the **least time-sensitive** thing in the system — if a packet is late or sent a little after the safety logic, there's no problem. So any other task with a priority preempts it at any moment. It runs only in the "free" time between the important things. If there's no free time at all, it runs instead of the Idle task.

---

## 7. The ISR: Bridging Hardware to Tasks

This is the part that connects the hardware interrupt to the world of tasks:

```c
void vESP_UART_RX_Callback(void)
{
    uint8_t rxData = USART_ReceiveByteDirect(USART_CHANNEL1);   // (1)

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;              // (2)
    xQueueSendFromISR(G_xESP_RX_Queue, &rxData, &xHigherPriorityTaskWoken);  // (3)
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);              // (4)
}
```

This callback is invoked from inside the USART1 interrupt when a byte arrives from the ESP.

**(1)** It reads the byte directly from the hardware data register (DR). The comment in the code matters: we read directly to avoid a deadlock with the TX if both run at the same time.

**(2)** `xHigherPriorityTaskWoken` — a flag variable we'll pass to the next function. The idea: when we drop a byte into the queue, we might have **woken** a task that was sleeping waiting on this queue (i.e. `vTask_ESP_Comm`). If the task we woke has a higher priority than the one that was running before the interrupt, we must do a context switch right after the ISR.

**(3) `xQueueSendFromISR`:** the ISR-safe version of `xQueueSend`, meant to be used **inside an ISR**:

- It drops the byte into `G_xESP_RX_Queue`.
- If this send woke a higher-priority task, it sets `xHigherPriorityTaskWoken = pdTRUE`.
- **Why a separate `FromISR` version?** Because the normal functions might put the task to sleep if the queue is full — and that is **strictly forbidden** inside an ISR (the ISR is not a task, it can't sleep). The `FromISR` version never sleeps; it returns immediately.

**(4) `portYIELD_FROM_ISR(xHigherPriorityTaskWoken):`** this tells the kernel: "after this ISR finishes, if `xHigherPriorityTaskWoken == pdTRUE` do a context switch right away to the task you woke".

This makes the reaction **immediate**: the moment a byte arrives, the ISR wakes `vTask_ESP_Comm`, and because it's priority 4 (high), the scheduler gives it the CPU right after the ISR, without waiting for the next tick.

### When exactly is `xHigherPriorityTaskWoken` `true` vs `false`?

It is **always initialized to `pdFALSE`** on line (2). The only thing that can flip it to `pdTRUE` is the `xQueueSendFromISR` call, and it does so **only when both** of these hold:

1. The send **unblocked** `vTask_ESP_Comm` — meaning the task was actually sleeping (`Blocked`) on `xQueueReceive` waiting for this queue, **and**
2. The unblocked task's priority (4) is **strictly higher** than the priority of the task that the UART interrupt happened to preempt.

Concretely, given our priorities:

| Situation when the byte arrived | Result | Why |
| --- | --- | --- |
| ESP_Comm was sleeping on the queue, and the running task was Sensors(3) / Feedback(2) / RPi(1) / Idle(0) | **`pdTRUE`** | ESP_Comm (4) is higher → switch to it right after the ISR |
| ESP_Comm was sleeping on the queue, and the running task was SafetyEngine (also priority 4) | **`pdFALSE`** | equal priority, not *strictly* higher → no preemption; ESP_Comm runs later via round-robin |
| ESP_Comm was **not** sleeping on the queue (it was already running, Ready, doing TX/maintenance, or blocked on a mutex) | **`pdFALSE`** | no task got unblocked at all — the byte just sits in the queue for ESP_Comm to pull later |

So `pdFALSE` is the common, harmless case: it simply means "no context switch needed right now." The byte is safely in the queue either way; the flag only decides **whether to switch immediately** or **let the current task finish its slice** first. `portYIELD_FROM_ISR` does the switch only when the flag is `pdTRUE`, otherwise it's a no-op and the interrupted task resumes exactly where it left off.

> **The golden rule for ISRs in FreeRTOS:** keep it as short as possible, use only `...FromISR` functions, and do nothing that could sleep. All the heavy work (the parsing) is done in the task, not in the ISR.

---

## 7.5 Deep Dive: The Full RX Path — `vESP_UART_RX_Callback` + `vTask_ESP_Comm` Together

This is the trickiest part of the whole project, so here is the complete story of these two functions working as a pair, with every line and every behind-the-scenes mechanism spelled out. If you only read one section to understand the RTOS, read this one.

### Step 0 — The mental model: two separate workers

The single most important thing to get straight: **the ISR and the task are two different workers doing two different jobs at two different times.** A very common misunderstanding is to think the interrupt "reads the byte and stores it in `DSRC_RxCallback`". It does **not**. Here is the actual split:

```text
        BYTE ARRIVES FROM ESP
                │
                ▼
┌──────────────────────────────────────┐
│ WORKER 1 — the ISR                    │   runs INSIDE the interrupt,
│ vESP_UART_RX_Callback                 │   for a few microseconds
│                                       │
│ • read the byte from the UART register│
│ • drop it into the queue              │
│ • (maybe) request a context switch    │
│ • return                              │
└───────────────────┬──────────────────┘
                     │ the byte now lives in G_xESP_RX_Queue
                     ▼
┌──────────────────────────────────────┐
│ WORKER 2 — the task                   │   runs LATER, when the
│ vTask_ESP_Comm                        │   scheduler gives it the CPU
│                                       │
│ • pull the byte out of the queue      │
│ • call DSRC_RxCallback(byte)  ←── parsing happens HERE, not in the ISR
│ • drain any other queued bytes        │
│ • do neighbor-table maintenance + TX  │
└──────────────────────────────────────┘
```

The ISR's only contact with `DSRC` is **none** — it never calls it. `DSRC_RxCallback` (the protocol parser, which can take time) is called **only by the task**. The queue is the conveyor belt between them: the ISR puts bytes on one end, the task takes them off the other end whenever it gets CPU time. They never run at the same instant and never touch the same variables directly — only the queue, which is thread-safe.

### Step 1 — `vESP_UART_RX_Callback`, line by line (Worker 1)

```c
void vESP_UART_RX_Callback(void)
{
    uint8_t rxData = USART_ReceiveByteDirect(USART_CHANNEL1);              // (1)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;                        // (2)
    xQueueSendFromISR(G_xESP_RX_Queue, &rxData, &xHigherPriorityTaskWoken);// (3)
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);                        // (4)
}
```

**Line (1) — grab the byte from hardware.**
When a byte finishes arriving, the UART hardware raises the RXNE flag and the CPU jumps into this interrupt. `USART_ReceiveByteDirect` reads the byte straight out of the hardware Data Register (DR). Reading the DR also clears the RXNE flag, which "acknowledges" the interrupt so it doesn't fire again for the same byte. We read it directly (not via a polling helper) to avoid any busy-flag deadlock if a transmit is happening at the same time. **One interrupt = one byte.** If 10 bytes arrive, this ISR runs 10 separate times.

**Line (2) — prepare the "did I wake someone important?" note.**
`xHigherPriorityTaskWoken` is a local flag, started at `pdFALSE`. Think of it as a **blank note** we hand to the queue so it can write its answer on it. By itself the ISR has no idea whether dropping this byte will wake a more important task — that's the kernel's knowledge, not the ISR's. So we give the kernel a place to tell us.

We pass it by **address** (`&xHigherPriorityTaskWoken`) precisely so the function on line (3) can *write back* into it.

**Line (3) — drop the byte on the conveyor belt (and learn if it woke someone).**
`xQueueSendFromISR` does three things atomically:

1. Copies `rxData` into `G_xESP_RX_Queue`.
2. Checks the queue's internal **list of tasks waiting to receive** from it. If `vTask_ESP_Comm` was sleeping on this queue, it is moved from "Blocked" to "Ready" — i.e. it is **woken**.
3. If that woken task has a **strictly higher** priority than the task the interrupt preempted, it writes `pdTRUE` onto our note. Otherwise the note stays `pdFALSE`.

Why the special `FromISR` variant instead of plain `xQueueSend`? Because `xQueueSend` is allowed to **block the caller to sleep** if the queue is full — and sleeping inside an ISR is forbidden (an ISR is not a task; it has no task context to suspend). `xQueueSendFromISR` never sleeps; if the queue were full it would just return a "failed" status and drop the byte, but never block.

**Line (4) — perform the switch *only if needed*, and only now at the end.**
`portYIELD_FROM_ISR` reads our note:

- If `pdTRUE` → it pends a context switch (sets the PendSV bit), so that **the instant this ISR returns, the CPU does not go back to the interrupted task — it switches to the higher-priority task we just woke.**
- If `pdFALSE` → it does nothing, and the interrupted task simply resumes where it was.

**Why defer the switch to the end of the ISR instead of switching immediately inside line (3)?** Two reasons:

1. **Correctness/architecture on Cortex-M:** the actual register save/restore is done by the PendSV handler, which is designed to run at the lowest interrupt priority, *after* all active interrupts finish. You don't switch tasks in the middle of an ISR.
2. **Efficiency:** if one ISR sent to several queues and woke several tasks, you wouldn't switch multiple times mid-ISR. You accumulate the "should switch" decision in the note and yield **once** at the very end.

> Recap of the ISR: read one byte → put it on the queue → if that woke a more-important task, switch to it the moment we return. Total time: microseconds. No parsing, no loops, no sleeping.

### Step 2 — How does the task *know* it's the one to wake? (the wait-list)

This is the question that confuses everyone: "the interrupt fires — how does it know `vTask_ESP_Comm` is the one whose turn it is?" The answer: **the interrupt does not choose the task. The task registered itself in advance.**

Picture the queue as a **service desk with a sign-up sheet** attached to it:

1. **When the task goes to sleep:** earlier, `vTask_ESP_Comm` called `xQueueReceive` and found the queue empty. Instead of standing there asking "anything yet? anything yet?" (that's the bad bare-metal polling), it **wrote its name on this specific queue's sign-up sheet** — internally a list called `xTasksWaitingToReceive` — and went to sleep (Blocked). That sheet is ordered by priority.
2. **When the byte arrives:** `xQueueSendFromISR` (running inside the ISR) puts the byte down, then **looks at that same sign-up sheet**. It sees `vTask_ESP_Comm`'s name, crosses it off, and moves the task to the Ready list.
3. **The result:** the task didn't "find out" by checking — it was *called back* because it had signed up on that exact queue before sleeping. The queue is the matchmaker.

So the chain is: the task **subscribed** to the queue → the ISR **publishes** to the queue → the queue **notifies** its subscriber. The interrupt is just the trigger that delivers data; the queue's machinery is what knows who to wake.

**Why is `vTask_ESP_Comm` always the one woken here?** Because it is the only task that ever calls `xQueueReceive` on `G_xESP_RX_Queue` — it's the sole subscriber. And because its priority is 4 (high), the moment it becomes Ready it preempts anything lower (via the `portYIELD_FROM_ISR` on line 4), so it runs **immediately after the ISR**, not at some later tick. (If several tasks were subscribed to the same queue, the kernel would wake the highest-priority one first, since the sign-up sheet is priority-ordered.)

### Step 3 — `vTask_ESP_Comm`'s receive lines, and "what if no message ever comes?"

Now the task side. The lines that matter for RX:

```c
if (xQueueReceive(G_xESP_RX_Queue, &byte, pdMS_TO_TICKS(10)) == pdTRUE)   // (A)
{
    DSRC_RxCallback(byte);                                                 // (B)
    while (xQueueReceive(G_xESP_RX_Queue, &byte, 0) == pdTRUE)             // (C)
        DSRC_RxCallback(byte);
}
```

**Line (A) — block-with-timeout (the key line).**
When the task reaches this line, the kernel puts it on **two** lists at once, not one:

- the queue's **wait-list** (`xTasksWaitingToReceive`) → "wake me if a byte arrives," and
- the **delayed-task list** with a 10ms deadline → "wake me anyway after 10ms."

Whichever happens first wins:

- **A byte arrives within 10ms** → the task is woken immediately (Step 2), `xQueueReceive` returns `pdTRUE`, `byte` holds the value, we enter the `if`.
- **10ms elapse with no byte** → the timeout wakes the task, `xQueueReceive` returns `pdFALSE`, the `if` is skipped, and the task **continues with the rest of its loop anyway** (the neighbor-table maintenance and the TX check that come after this block).

> **This directly answers "if no message comes at all, does the task never run?"** — No. The 10ms timeout guarantees the task wakes roughly every 10ms regardless of traffic. So even with a totally silent radio it still runs `DSRC_RemoveStale` (to purge vehicles that went quiet) and still broadcasts every 100ms. If we had used `portMAX_DELAY` (sleep forever until a byte comes) instead of `10ms`, then yes — a silent radio would freeze the task forever, and that maintenance would never run. The timeout is exactly what prevents that.

While the task is blocked on line (A) it is in the **Blocked** state and consumes **zero CPU** — it is genuinely asleep, not spinning. The CPU runs Sensors / Feedback / RPi / Idle in the meantime.

**Line (B) — parse the first byte.**
We got a byte → hand it to `DSRC_RxCallback`, the protocol state machine that accumulates bytes into a complete packet. **This is the parsing the ISR deliberately avoided.** It happens here, in task context, where taking some time is fine because the task can be preempted by higher-priority work.

**Line (C) — drain the rest with a zero timeout.**
After the first byte there may be a whole burst sitting in the queue (several bytes can pile up between two runs of this task). The `while` loop pulls them all out, but note the timeout is **`0`**, not `10ms`. A timeout of `0` means "**never block** — give me a byte if one is there, otherwise return `pdFALSE` instantly." So the loop empties everything currently queued in one tight pass and stops the moment the queue is empty, without ever sleeping between bytes. This is more efficient than going back to the top of the task loop for each byte.

### Putting it all together — one byte's complete journey

```text
1. Byte finishes arriving at UART1  →  RXNE flag set  →  CPU enters the ISR.
2. ISR line (1): read byte from DR (clears RXNE).
3. ISR line (3): xQueueSendFromISR → byte stored in queue; kernel sees
   vTask_ESP_Comm on the queue's wait-list → marks it Ready; since 4 > whatever
   was running, writes pdTRUE on the note.
4. ISR line (4): portYIELD_FROM_ISR(pdTRUE) → pends a context switch.
5. ISR returns → instead of resuming the interrupted task, the CPU switches to
   vTask_ESP_Comm (it's higher priority and now Ready).
6. Task line (A): xQueueReceive returns pdTRUE with the byte.
7. Task line (B): DSRC_RxCallback(byte) parses it.
8. Task line (C): drains any siblings that arrived in the same burst.
9. Task continues: neighbor-table maintenance, then the 100ms TX check.
10. Task loops back to line (A) and blocks again (≤10ms) — back to sleep,
    CPU freed for lower-priority tasks.
```

That is the entire RX pipeline: **hardware → ISR → queue → task → parser**, with the wait-list deciding *who* wakes, the deferred yield deciding *when* the switch happens, and the 10ms timeout guaranteeing the task stays alive and periodic even in total silence.

---

## 8. A Full Timeline Scenario

Assume we're at time 0ms (the scheduler just started). All tasks are in the Ready state.

```text
Time     What happens
─────    ──────────────────────────────────────────────────────────────
0ms      Scheduler looks: highest Ready priority = 4 (SafetyEngine + ESP_Comm).
         Starts with SafetyEngine. Takes both keys, runs the ADAS, returns them.
~2ms     SafetyEngine done → vTaskDelayUntil(50ms) → sleeps until 50ms.
         Scheduler gives the CPU to ESP_Comm (same priority, Ready).
2ms      ESP_Comm: queue is empty → xQueueReceive with timeout 10ms → sleeps.
         Highest Ready now = 3 → Sensors runs.
2ms      Sensors: sends a Trigger to the first US → sleeps waiting for the echo (interrupt).
         Highest Ready now = 2 → Feedback runs.
3ms      Feedback: reads the flags, drives the LEDs/motor, vTaskDelay(25ms) → sleeps.
         Highest Ready now = 1 → RPi_Comm runs.
4ms      RPi_Comm: sends a packet, vTaskDelayUntil(100ms) → sleeps.
         Nobody is Ready → the Idle Task runs (no work).
~5ms     [INTERRUPT] the US echo came back → ISR wakes Sensors.
         Sensors (priority 3) > Idle → preempts Idle immediately and continues reading.
...      and so on — whenever an event happens, the highest task that cares about it
         preempts the lower one.
12ms     [INTERRUPT] a byte from the ESP → ISR wakes ESP_Comm (priority 4).
         ESP_Comm > anything running → preempts it immediately (portYIELD_FROM_ISR).
50ms     SafetyEngine finishes its sleep → preempts anything lower and runs again.
```

The core idea you should take away: **the CPU is never stuck waiting**. Whenever a task sleeps, the highest ready task takes its place, until everyone sleeps and the Idle task runs. And any important event (interrupt) can preempt what's running if the thing that woke up is more important.

---

## 9. Why Are the Priorities and Locks Arranged This Way?

### The priority order (4 → 1)

| Level | Tasks | The rationale |
| --- | --- | --- |
| 4 (highest) | SafetyEngine + ESP_Comm | The brain and V2X comm — time-sensitive, must run on time |
| 3 | Sensors | The data producer — must give fresh data to the brain, but below the brain itself |
| 2 | Feedback | The muscles — a consumer, executes after the brain decides |
| 1 (lowest) | RPi_Comm | Telemetry — the least time-sensitive, runs in the free time |

The principle: **the more safety-critical = the higher the priority**. Safety (brain) above reading above execution above telemetry.

### Deadlock prevention

A **deadlock** happens when task A holds key 1 and waits for key 2, while task B holds key 2 and waits for key 1 → both wait for each other forever.

We have two keys: `G_xNeighborTableMutex` and `G_xDataMutex`. The rule that prevents the deadlock:

1. **`vTask_SafetyEngine`** takes both together, but **always in the same order**: NeighborTable first, Data second.
2. **`vTask_ESP_Comm`** needs both, but **never takes them together** — it takes NeighborTable alone and returns it, then takes Data alone.
3. **`vTask_Sensors`**, **`vTask_Feedback`** and **`vTask_RPi_Comm`** take `G_xDataMutex` **alone** only.

As long as nobody takes the two keys in reverse order, a **deadlock is impossible**. SafetyEngine is the only one that does nested locking, and it always sticks to the same order.

### Why is some data protected by a mutex and some not?

- `G_stHostVehicleState` = a large struct (13 × float = 52 bytes). Writing it takes several instructions and can be preempted mid-write → **needs a mutex**.
- `G_u8SystemFlags` and the module flags = a single byte (`volatile uint8_t`). Reading/writing a byte on Cortex-M is atomic (a single instruction, can't be interrupted) → **needs no mutex**.

The `volatile` keyword matters here: it tells the compiler "read this variable from memory every time, don't cache it in a register" — because it can change from another task at any moment.

---

## Summary in One Sentence

> The bare-metal version was one loop doing everything in sequence while the CPU waited. The RTOS split the work into 5 independent loops with priorities, and the kernel distributes the CPU among them: the most important gets precedence, whatever is waiting for something sleeps and frees the CPU for others, and events (interrupts) can preempt what's running so the reaction is immediate. The Queue safely bridges the ISR to the task, and the Mutex protects shared data from interference.
