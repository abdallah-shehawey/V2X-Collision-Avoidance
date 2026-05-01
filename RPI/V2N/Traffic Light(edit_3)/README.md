🚦 V2X Smart Traffic Management System
📝 Overview
This project implements a Vehicle-to-Network (V2N) communication system for intelligent traffic light control. It manages traffic density dynamically and provides priority access for emergency vehicles (Ambulances) using MQTT protocol.

📁 Files & Structure
1. Intelligent_Gateway_2.py (The Brain)
Location: Runs on a Cloud Server / MEC.

Role: Acts as the central controller. It calculates traffic density and decides whether to grant priority to ambulances based on their distance.

2. Car_client.py (Vehicle OBU)
Location: Representing the On-Board Unit inside vehicles.

Role: Sends periodic "Presence" messages and requests priority if the vehicle is an emergency type (Ambulance).

3. trafic_light.py (RSU Simulator)
⚠️ Note: This is a Test Simulator used for logic verification. It will be replaced by the physical ESP32 hardware once ready.

Role: Executes the light cycles and overrides its state to "Green" when an emergency command is received.

4. config.py (System Constants)
Role: Contains all fixed parameters to keep the code clean.

Constants: Includes NORMAL_CAR_ID, AMBULANCE_ID, and distance thresholds (e.g., priority is only granted if distance < 100m).

🚀 System Logic (How it works)
Presence Updates: All cars broadcast their ID and distance to the Gateway.

Density Calculation: The Gateway counts normal cars to optimize green light duration.

Emergency Override: If an Ambulance requests priority and meets the distance criteria, the Gateway commands the Traffic Light to force a "Green" state.

Confirmation Loop: The Ambulance client retries the request (up to 3 times) until it receives a confirmation from the Gateway.