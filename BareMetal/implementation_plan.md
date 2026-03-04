# V2X RTOS Integration and Testing Plan

This document outlines the step-by-step approach to safely integrate and test the FreeRTOS architecture for the ADAS V2X System. The implementation is broken down into modular phases to isolate bugs and ensure stability at each level before adding complexity.

## Proposed Changes

We will introduce functionality gradually across different files, primarily focusing on [System.c](file:///d:/Grad_Project/APP/Bare_Metal/Src/System.c) and creating task-specific routines.

### Phase 1: Basic Sensors & Actuators (Local ADAS)
Initial test to ensure we can read hardware and manipulate outputs through the RTOS scheduler without any communication overhead.

1. **Task 1 (`vTask_Sensors`)**: Periodically reads distance from a single Ultrasonic Sensor (e.g., S1 Front Left) **and speed, acceleration, and Z-axis distance (altitude) from the MPU9250**.
2. **Task 2 (`vTask_ADAS_Core`)**: Extremely simplified logic. If `Distance <= 10 cm` or `Speed > limit`, set a global flag or send a queue message.
3. **Task 3 (`vTask_Feedback`)**: Reads the flag/queue and turns the Buzzer ON/OFF based on the state.

**Goal:** Verify Timer configurations, MPU SPI communication within RTOS ticks, and basic task synchronization (Queues/Semaphores).

### Phase 2: Introducing V2X Transmission (ESP-NOW TX)
Once local sensing is stable, we begin broadcasting our state.

1. **Activate UART1** for ESP-NOW communication.
2. **Task 4 (`vTask_ESP_TX`)**: Periodically packages the current Ultrasonic distance, **MPU9250 speed data, Z-axis distance, and the vehicle's movement Direction** into a struct and sends it over UART1.

**Goal:** Ensure UART transmission doesn't block the RTOS tick and that the ESP receives valid frames containing all sensor state data and direction.

### Phase 3: Introducing V2X Reception & Emergency Handling (ESP-NOW RX)
Testing the critical high-priority reception path.

1. **Task 5 (`vTask_ESP_RX`)**: Configured with Highest Priority (4). Waits on a UART RX Interrupt/Semaphore.
2. **Logic Update**: If a simulated "Emergency Brake" message is received from the ESP, immediately trigger the `vTask_Feedback` to sound the Buzzer, regardless of the local ultrasonic distance.

**Goal:** Prove that the highest priority task can preempt the core logic and react instantly to external network warnings.

### Phase 4: Full System Integration (The Real ADAS)
Replacing the simplified logic with the actual application modules.

1. Connect `vTask_Sensors` to all 6 Ultrasonics and the MPU9250.
2. Update `vTask_ADAS_Core` to call the actual [FCW_voidUpdate()](file:///d:/Grad_Project/APP/Bare_Metal/Src/FCW_program.c#39-45), [EEBL_voidUpdate()](file:///d:/Grad_Project/APP/Bare_Metal/Src/EEBL_program.c#38-43), etc.
3. Replace hardcoded V2X messages with real dynamic system states.
4. Add `vTask_RPi_TX` for the display interface.

## Verification Plan

### Manual Verification
- **Phase 1**: Place an object in front of Sensor 1. The buzzer should sound immediately. Remove it, and the buzzer should silence.
- **Phase 2**: Use a logic analyzer or a secondary serial monitor attached to UART1 to verify that distance data is being transmitted formatted correctly every 100ms.
- **Phase 3**: Manually inject a UART frame simulating an incoming V2V warning into the RX pin. The system must sound the buzzer instantly.
- **Phase 4**: Full bench test of all sensors and communication interfaces simultaneously.
