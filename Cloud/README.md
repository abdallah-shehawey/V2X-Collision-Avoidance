# Cloud Logic - Intelligent_Gateway

This directory contains the central backend logic responsible for managing traffic lights and coordinating data within the V2X (Vehicle-to-Everything) system.

## Core Functionalities:
- **Data Acquisition:** Receiving traffic light states and vehicle data via the MQTT protocol.
- **Decision Making:** Analyzing traffic density and dynamically adjusting traffic light timing.
- **Emergency Priority:** Detecting emergency vehicles (e.g., AMB123) and granting them immediate right-of-way.
  
## How to Run:
1. Ensure the `paho-mqtt` library is installed in your Python environment.
2. Run the main script using the following command:
   ```bash
   python Intelligent_Gateway.py
