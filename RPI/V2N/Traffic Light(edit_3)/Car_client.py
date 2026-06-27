"""
******************************************************************************
* @file           : Car_client.py
* @author         : Eng. Amira
* @brief          : Passive Vehicle V2X Receiver (Listener Only)
* @description    :
* كود العربيات العادية (مستمع فقط). لا يبعت ID ولا يبعت لوكيشن.
* بيستقبل حالة إشارة المرور، الطوارئ، والتحذيرات المجهزة من الـ Gateway مباشرة.
******************************************************************************
"""

import paho.mqtt.client as mqtt
import ssl
import json

# ============================================================
# SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# توبيك البث الموحد والنهائي اللي الـ Gateway بيبعت عليه الحالة بعد فحصها
PROCESSED_TOPIC = "V2X/zone1/traffic/processed"

# ======================== MQTT CALLBACKS ========================

def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Vehicle V2X System successfully connected to HiveMQ Cloud!")
        # الاشتراك في قناة البث العام المجهزة من الـ Gateway
        client.subscribe(PROCESSED_TOPIC)
        print(f"📥 Listening to Infrastructure Broadcast on: {PROCESSED_TOPIC}")
    else:
        print(f"❌ Connection failed with code {reason_code}")

def on_message(client, userdata, msg):
    try:
        # استقبال وفك داتا الـ JSON الموحدة القادمة من الـ Gateway
        data = json.loads(msg.payload.decode())
        
        state        = data.get("state", "UNKNOWN")
        rem_time     = data.get("remaining_time", 0)
        is_emergency = data.get("is_emergency", False)
        warning      = data.get("warning", "Normal Traffic")
        density      = data.get("density", 0)

        # تحديد شكل الإشارة بناءً على اللون القادم
        icon = "🟢" if state == "GREEN" else "🔴" if state == "RED" else "🟡"
        
        print("\n" + "="*60)
        if is_emergency:
            print(f"🚨 [⚠️ V2I EMERGENCY OVERRIDE ALERT] : {warning}")
            print(f"🚦 Dashboard Display -> Light: {icon} {state}")
            print(f"⏱️ Clear Path Timer: {rem_time}s (Do Not Block the Intersection!)")
        else:
            print(f"📡 [V2N Live Broadcast] Road Status: {warning}")
            print(f"🚦 Dashboard Display -> Light: {icon} {state}")
            print(f"⏱️ Remaining Time: {rem_time}s")
            print(f"🚗 Intersection Vehicle Density: {density}")
        print("="*60)

    except Exception as e:
        print(f"❌ Error decoding infrastructure broadcast: {e}")

# ======================== MAIN EXECUTION ========================

if __name__ == "__main__":
    print("🚗 Starting Anonymous Passive Vehicle Client...")
    
    # تهيئة الـ Client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    client.on_connect = on_connect
    client.on_message = on_message

    print("🌐 Connecting to Cloud Server...")
    client.connect(BROKER, PORT, 60)

    # تشغيل الاستقبال المستمر في الخلفية
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Vehicle Client stopped by user.")
    finally:
        client.disconnect()
        print("🔌 Disconnected from Server.")