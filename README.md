# 🚗 V2X Collision Avoidance System

## EECE25 – Embedded Systems Graduation Project

## 📌 Overview

This project presents a **Vehicle-to-Everything (V2X) Collision Avoidance System** aimed at improving road safety and traffic efficiency using embedded systems, wireless communication, computer vision, and real-time decision-making.

A scaled car prototype talks to:

- **Other vehicles (V2V)** — direct ESP-NOW broadcast between STM32-driven cars
- **Road infrastructure (V2I / V2N)** — an "intelligent" traffic light + camera gateway over MQTT
- **Pedestrians (V2P)** — an onboard camera + AI model detecting people, bikes, and motorcycles
- **A phone / laptop (driver interface)** — a live telemetry dashboard and a remote-control page, both served from the car's Raspberry Pi

Our focus is on **collision prevention**, **driver assistance**, and **smart transportation**, with special consideration for real-world traffic conditions in Egypt.

---

## 🎯 Motivation

Road accidents and traffic congestion are major challenges worldwide, especially in developing countries.
Traditional driving systems lack real-time awareness of surrounding vehicles, pedestrians, and infrastructure.

**V2X communication** enables cooperative awareness and has the potential to:

- Reduce road accidents significantly
- Improve traffic flow
- Support smart city initiatives
- Prepare the road for autonomous vehicles

---

## 🧠 System Architecture

```text
STM32 (V2V safety core, FreeRTOS)
   │  UART — packed Neighbor struct
   ▼
ESP32 master (car)  ◄──ESP-NOW──►  ESP32 sniffer (other car / test rig)
   │  UART — telemetry frame
   ▼
Raspberry Pi (car's on-board computer)
   │
   ├── hub/       Pub/Sub IPC broker (Unix socket) — every RPI process talks through this
   ├── V2N/       Car_client.py — bridges HiveMQ Cloud (traffic light) into the hub
   ├── V2P/       V2P.py — camera + ONNX model, publishes pedestrian/moto alerts
   ├── DashBoard/ server.py — reads UART + hub, writes data.json, serves the telemetry UI
   └── Control/   control_server.py — phone remote control, enforces the safety guard

Traffic_Light/ (roadside unit, separate machine)
   │  MQTT (HiveMQ Cloud)
   └── camera(s) + Intelligent_Gateway.py → publishes processed traffic state for Car_client.py
```

The **STM32** is the only safety-critical, hard-real-time component (FreeRTOS, deterministic timing). Everything on the **Raspberry Pi** is soft-real-time Python glued together by a small custom pub/sub hub, so each subsystem (traffic light, pedestrian camera, dashboard, remote control) can be developed, run, and restarted independently.

---

## 📁 Project Structure

```text
V2X-Collision-Avoidance/
├── V2V-STM32/            # STM32F446RE firmware — real-time V2V safety core (FreeRTOS)
│   ├── Inc/Application/  # BSW, DNPW, EEBL, FCW, IMA, SDW, DSRC, SafetyEngine headers
│   ├── Inc/Drivers/      # MCAL (RCC, GPIO, NVIC, ...) + HAL (SPI, USART, TIM, EXTI, ...)
│   ├── Src/               # Implementation for every driver + V2V subsystem
│   ├── Startup/           # Startup & linker scripts
│   └── ThirdParty/        # FreeRTOS kernel
│
├── esp32/                 # ESP-NOW bridge between STM32 UART and the wireless V2V link
│   ├── master/             # On-car node: STM32 UART ⇄ ESP-NOW (broadcasts + receives)
│   └── sniffer/            # Listen-only ESP-NOW node, for testing/monitoring V2V traffic
│
├── RPI/                   # Raspberry Pi — car's on-board computer
│   ├── hub/                 # Central pub/sub IPC broker + dashboard_bridge (writer of data.json)
│   ├── V2N/                 # Car_client.py — HiveMQ (traffic light) ⇄ IPC hub bridge
│   ├── V2P/                 # Camera-based pedestrian/motorcycle detection (ONNX model)
│   ├── DashBoard/            # Read-only telemetry web UI (:8000)
│   ├── Control/              # Phone remote-control web UI + motor driver (:8001)
│   └── systemd/               # systemd unit files to run everything on boot
│
├── Traffic_Light/         # Roadside unit: camera(s) + MQTT gateway + simulator GUI
│
├── CODE_REVIEW.md          # Ongoing code review notes (non-STM32 subsystems)
├── implementation_plan.md  # Project implementation plan
└── README.md                # This file
```

---

## 🚘 Vehicle-to-Vehicle (V2V) — STM32 + ESP-NOW

Direct, infrastructure-free communication between nearby vehicles, running on **STM32F446RE** with **FreeRTOS** for deterministic timing. See [V2V-STM32/README.md](V2V-STM32/README.md) for the full architecture, task breakdown, and directory layout.

**Implemented V2V subsystems:**

| Subsystem | Purpose |
| --- | --- |
| **EEBL** – Electronic Emergency Brake Light | Detects sudden deceleration, broadcasts emergency warnings |
| **FCW** – Forward Collision Warning | Relative distance & speed to prevent rear-end collisions |
| **SDW** – Safe Distance Warning | Maintains a safe buffer distance around the vehicle |
| **DNPW** – Do Not Pass Warning | Evaluates overtaking safety using opposite-lane data |
| **IMA** – Intersection Movement Assist | Analyzes crossing trajectories to avoid intersection crashes |
| **BSW** – Blind Spot Warning | Monitors lateral zones during lane changes |

The STM32 exchanges a packed `Neighbor` struct with the outside world over UART. The **ESP32** (see [esp32/README.md](esp32/README.md)) forwards those bytes verbatim over **ESP-NOW**, so every field's order/size/type must match exactly between the STM32 side and both ESP32 sketches (`master`, `sniffer`).

---

## 🚦 Vehicle-to-Infrastructure / Vehicle-to-Network (V2I / V2N)

The `Traffic_Light/` roadside unit runs camera-based vehicle/plate detection and a traffic-light state machine, and publishes the processed state to **HiveMQ Cloud (MQTT)**. On the car side, `RPI/V2N/Car_client.py` subscribes to that feed, merges it with local vehicle speed (from the STM32 over UART/hub), and decides a single `traffic_flag` (GO/STOP) — including whether the car can actually cross before the light changes.

Used for:

- Red-light / crossing-time warnings
- Emergency-vehicle preemption
- Traffic density awareness

---

## 🚸 Vehicle-to-Pedestrian (V2P)

`RPI/V2P/V2P.py` runs an ONNX object-detection model (person / bicycle / car / motorcycle) on a Raspberry Pi camera feed, tracks approach speed and crossing-zone proximity, and publishes pedestrian and motorcycle-collision alerts onto the IPC hub. This avoids relying on pedestrians carrying any device.

---

## 🖥️ Raspberry Pi Software (on-board computer)

Every RPI process is decoupled through a small custom **pub/sub IPC hub** over a Unix domain socket — publishers and subscribers never talk directly to each other. See [RPI/README.md](RPI/README.md) for the full data-flow diagram and how to bring the whole stack up (systemd or manual).

| Component | Role |
| --- | --- |
| [`RPI/hub/`](RPI/hub/README.md) | `hub.py` (broker) + `ipc_node.py` (client library) + `dashboard_bridge.py` (the only writer of `data.json`) |
| [`RPI/V2N/`](RPI/V2N/README.md) | `Car_client.py` — bridges HiveMQ traffic data + local speed into `v2n_frame` |
| [`RPI/V2P/`](RPI/V2P/README.md) | `V2P.py` — camera AI, publishes `v2p_frame` + `motorcycle_alert` |
| [`RPI/DashBoard/`](RPI/DashBoard/README.md) | Read-only telemetry web UI, serves `data.json` on `:8000` |
| [`RPI/Control/`](RPI/Control/README.md) | Phone remote-control web UI + L298N motor driver on `:8001`, enforces the ADAS/V2N/V2P safety guard |
| [`RPI/systemd/`](RPI/systemd/README.md) | systemd units + installer script to run the whole stack on boot |

---

## ⚙️ Hardware Components

- STM32F446RE (Nucleo)
- ESP32-S3 (×2 — master + sniffer/DSRC bridge)
- Raspberry Pi (5, camera-equipped)
- MPU9250 IMU (speed/heading/attitude)
- Ultrasonic sensors (×6, obstacle proximity)
- Pi Camera (pedestrian/vehicle detection)
- L298N motor driver + DC motors
- Buzzer / LED indicators
- Traffic-light rig with its own camera(s)

---

## 🧩 Software & Technologies

- Embedded C (STM32 HAL/MCAL from scratch) + FreeRTOS
- Arduino/C++ (ESP32, ESP-NOW)
- Python (Raspberry Pi services, custom pub/sub IPC over Unix sockets)
- MQTT / HiveMQ Cloud (V2N/V2I)
- ONNX Runtime + OpenCV (V2P computer vision)
- YOLOv8 (traffic-light vehicle/plate detection)
- systemd (service orchestration on the Pi)

---

## 🔔 Alerts & Feedback

- Visual alerts (LEDs, dashboard UI)
- Audio alerts (Buzzer)
- Wireless warning messages to nearby vehicles (ESP-NOW)
- Blocked-move banner + vibration on the phone remote when a drive command is refused for safety

---

## 🧪 Project Scope

- Prototype-level implementation (scaled car, not a full-size vehicle)
- Simulation- and testbed-based validation
- Focus on safety-critical use cases
- Not intended for commercial deployment

> See [CODE_REVIEW.md](CODE_REVIEW.md) for a running list of known issues across the non-STM32 subsystems.

---

## 🚀 Future Enhancements

- 5G C-V2X integration
- AI-based collision prediction improvements
- Cloud analytics dashboard
- Large-scale multi-vehicle simulation
- Enhanced security (the current MQTT/IPC links are not hardened for production)

---

## 👨‍💻 Team Members

- [Abdallah AbdelMomen Abdallah](http://www.linkedin.com/in/abdallah-shehawey) (Team Leader)
- [Abdallah Saleh Mohamed](https://www.linkedin.com/in/abd-allahsaleh)
- [Ahmad Gamal Ali](https://www.linkedin.com/in/ahmadgamalmansour)
- [Alaa Hassan Wanas](https://www.linkedin.com/in/alaa-hassan-647a35263/)
- [Amira Atef Roshdy](https://www.linkedin.com/in/amira-atef-614463258/)
- [Aya Gamal Taha](http://www.linkedin.com/in/ayagamalpro)
- [Gamila Adel Mohamed](https://www.linkedin.com/in/gamila-elkomy-1556a82a1)
- [Asmaa Saad Fouda](https://www.linkedin.com/in/asmaa-saad-4a7bb3269)

---

## 📄 References

- V2X Survey & Relevant Theory Papers
- IEEE 802.11p / DSRC
- LTE-V2X (3GPP Release 14/15)

---

## 🏁 Conclusion

This project demonstrates how **V2X communication combined with embedded systems and AI** can significantly enhance road safety and contribute to smarter transportation systems.

---
