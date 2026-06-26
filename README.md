# 🚗 V2X Collision Avoidance System

## EECE25 – Embedded Systems Graduation Project

## 📌 Overview

This project presents a **Vehicle-to-Everything (V2X) Collision Avoidance System** aimed at improving road safety and traffic efficiency using modern embedded systems, wireless communication, and intelligent decision-making.

The system enables vehicles to communicate with:

- Other vehicles (V2V)
- Road infrastructure (V2I)
- Pedestrians (V2P)
- Cloud / network services (V2N)

Our focus is on **collision prevention**, **driver assistance**, and **smart transportation**, with special consideration for real-world challenges in Egypt.

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
## 📁 Project Structure
```
Graduation-Project/
├── V2V-STM32/        # STM32 (V2V – ADAS use cases)
│   ├── Inc/          # Headers لكل subsystem (BSW, FCW_DNPW, EEBL, IMA, ...)
│   ├── Src/          # Implementation لكل subsystem
│   └── Startup/     # Startup & linker scripts
│
├── RPI/              # High-level processing & networking
│   ├── V2I/          # Vehicle-to-Infrastructure
│   ├── V2N/          # Vehicle-to-Network (MQTT / Cloud)
│   └── V2P/          # Vehicle-to-Pedestrian (Vision / AI)
│
└── README.md         # Main project documentation
```

---

## 🧠 System Architecture

The project is divided into **four main V2X subsystems**:

### 1️⃣ Vehicle-to-Vehicle (V2V)

Enables direct communication between nearby vehicles to exchange speed, position, and warning messages.

**Implemented V2V Subsystems:**

- Electronic Emergency Brake Light (EEBL)
- Blind Spot Warning (BSW)
- Intersection Movement Assist (IMA)
- Do Not Pass Warning (DNPW)
- Forward Collision Warning (FCW)
- Safe Distance Warning (SDW)
- Advanced Driver Assistance System (ADAS)

---

### 2️⃣ Vehicle-to-Infrastructure (V2I)

Communication between vehicles and roadside infrastructure such as:

- Traffic lights
- Road signs
- Roadside units (RSUs)

Used for:

- Traffic optimization
- Hazard warnings
- Intelligent signaling

---

### 3️⃣ Vehicle-to-Pedestrian (V2P)

Enhances pedestrian safety using:

- Camera-based detection
- Computer vision and AI
- Real-time alerts to drivers

This approach avoids reliance on pedestrians carrying special devices.

---

### 4️⃣ Vehicle-to-Network (V2N)

Connects vehicles to cloud or edge servers using:

- Wi-Fi / Cellular (4G/5G)
- MQTT protocol

Used for:

- Traffic data analytics
- Hazard notifications
- Future OTA updates

---

## ⚙️ Hardware Components

- ESP32
- STM32 (or Raspberry Pi if needed)
- GPS Module
- Ultrasonic Sensors
- Camera Module
- Wheel Speed Sensor
- Buzzer / Speaker
- LED Indicators
- LCD / OLED Display
- DSRC / C-V2X Module

---

## 🧩 Software & Technologies

- Embedded C / C++
- MQTT
- Computer Vision (YOLO – optional)
- Sensor Fusion (GPS + IMU)
- Real-time decision logic
- V2X communication concepts (DSRC, LTE-V2X)

---

## 🔔 Alerts & Feedback

- Visual alerts (LEDs, LCD)
- Audio alerts (Buzzer / Speaker)
- Wireless warning messages to nearby vehicles

---

## 🧪 Project Scope

- Prototype-level implementation
- Simulation-based validation
- Focus on safety-critical use cases
- Not intended for commercial deployment

---

## 🚀 Future Enhancements

- 5G C-V2X integration
- AI-based collision prediction
- Cloud analytics dashboard
- Large-scale simulation
- Enhanced security mechanisms

---

## 👨‍💻 Team Members

- [Abdallah AbdelMomen Abdallah](http://www.linkedin.com/in/abdallah-shehawey)
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
