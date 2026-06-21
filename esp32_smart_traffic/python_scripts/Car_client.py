# -*- coding: utf-8 -*-
"""
Car_client.py
Simulator for the Car On-Board Unit (OBU) display.
"""

import paho.mqtt.client as mqtt
import ssl
import json
import os

# ============================================================
# SERVER CONFIGURATION (Same as Gateway)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# Topic for processed traffic data from Gateway
PROCESSED_TOPIC = "V2X/zone1/traffic/processed"

# ============================================================
# VARIABLES TO STORE RECEIVED DATA
# ============================================================
latest_received_distance = None
latest_remaining_time = None

# ============================================================
# MQTT CALLBACKS
# ============================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Car Display connected to HiveMQ Cloud!")
        client.subscribe(PROCESSED_TOPIC)
        print(f"📡 Listening to Processed Traffic Feed on: {PROCESSED_TOPIC}\n")
    else:
        print(f"❌ Connection failed: {reason_code}")

def on_message(client, userdata, msg):
    global latest_received_distance, latest_remaining_time
    try:
        payload_str = msg.payload.decode().strip()
        data = json.loads(payload_str)
        
        # 1. Extract and update data in background
        latest_remaining_time = data.get("remaining_time", None)
        
        closest_vehicle = data.get("closest_vehicle", {})
        if closest_vehicle:
            latest_received_distance = closest_vehicle.get("distance_m", None)
        else:
            latest_received_distance = None

        # 2. Extract display-specific info
        state        = data.get("state", "UNKNOWN")
        warning_msg  = data.get("warning", "Normal Traffic")
        is_emergency = data.get("is_emergency", False)

        # 3. Clear screen and display OBU interface
        os.system('cls' if os.name == 'nt' else 'clear')
        print("=" * 60)
        print("🚘 V2X CAR ON-BOARD DISPLAY (OBU) 🚘")
        print("=" * 60)
        print(f"🚦 Traffic Light State : {state}")
        print(f"📡 System Status       : ONLINE")
        print("-" * 60)
        
        if is_emergency:
            print(f"⚠️ NOTIFICATION : {warning_msg}")
        else:
            print(f"ℹ️ STATUS       : {warning_msg}")
            
        print("=" * 60)

    except Exception as e:
        print(f"❌ Error parsing display data: {e}")

# ============================================================
# MAIN RUNNER
# ============================================================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

client.on_connect = on_connect
client.on_message = on_message

print("🌐 Connecting Car HUD to HiveMQ Cloud...")
client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n🛑 Car Display disconnected.")