# -*- coding: utf-8 -*-
"""
Car_client.py
Simulator for the Car On-Board Unit (OBU) display (Advanced Professional HUD).
"""

import paho.mqtt.client as mqtt
import ssl
import json
import os

# ============================================================
# SERVER CONFIGURATION
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

PROCESSED_TOPIC = "V2X/zone1/traffic/processed"

# قاموس ترجمة الأكواد بالإنجليزية لضمان مظهر احترافي وثابت بالشاشة
TRANSITION_MAP = {
    1: "RED -> GREEN (Prep to Drive)",
    2: "GREEN -> RED (EMERGENCY BRAKE!)",
    3: "YELLOW -> RED (Slow Down & Stop)",
    4: "GREEN -> YELLOW (Caution - Clearing Intersection)",
    5: "RED -> YELLOW (Get Ready)",
    6: "YELLOW -> GREEN (Intersection Cleared)"
}

latest_received_distance = None
latest_remaining_time = None

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
        
        # 1. قراءة البيانات وحسابات الخلفية
        latest_remaining_time = data.get("remaining_time", 0)
        state                 = data.get("state", "UNKNOWN")
        trans_code            = data.get("transition_code", 0)
        warning_msg           = data.get("warning", "Normal Traffic")
        is_emergency          = data.get("is_emergency", False)
        density               = data.get("density", 0)

        closest_vehicle = data.get("closest_vehicle", {})
        if closest_vehicle:
            latest_received_distance = closest_vehicle.get("distance_m", None)
        else:
            latest_received_distance = None

        # جلب وصف الانتقال بالإنجليزية
        transition_desc = TRANSITION_MAP.get(trans_code, "Stable Signal")

        # 2. مسح الشاشة وبناء الـ HUD بتنسيق ثابت ومنظم ومحاذى لليسار
        os.system('cls' if os.name == 'nt' else 'clear')
        print("=" * 65)
        print(" 🚘             V2X CAR ON-BOARD DISPLAY HUD             🚘 ")
        print("=" * 65)
        print(f" 🚦 Current TL State    : {state}")
        print(f" 🔮 Active Transition   : {transition_desc}")
        print(f" ⏱️ Time to Next State  : {latest_remaining_time} sec")
        print(f" 📊 Local Vehicle Count : {density} Vehicles")
        print("-" * 65)
        
        if is_emergency:
            print(f" ⚠️  EMERGENCY ALERT     : {warning_msg}")
        else:
            print(f" ℹ️  System Status       : {warning_msg}")
            
        print("=" * 65)

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