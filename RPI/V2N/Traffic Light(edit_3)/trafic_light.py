"""
******************************************************************************
* @file           : trafic_light_fixed.py
* @brief          : Traffic Light Simulator
* @fixes          :
*   1. Thread-safe باستخدام threading.Lock على كل الـ shared state
*   2. مفيش race condition بين traffic_cycle_logic وـ on_message
******************************************************************************
"""

import paho.mqtt.client as mqtt
import ssl
import json
import time
import threading

# --- الإعدادات ---
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

OVERRIDE_TOPIC = "V2X/zone1/traffic/processed"
STATE_TOPIC    = "v2n/traffic/light/state"

# --- Shared State ---
current_state      = "RED"
is_override        = False
remaining_time     = 10
ambulance_distance = 0
state_lock         = threading.Lock()   # FIX: Thread safety


# ======================== MQTT Callbacks ========================

def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("✅ Traffic Light Simulator Online")
        client.subscribe(OVERRIDE_TOPIC)
    else:
        print(f"❌ Connection failed, rc={reason_code}")


def on_message(client, userdata, msg):
    global current_state, is_override, remaining_time, ambulance_distance

    try:
        payload     = json.loads(msg.payload.decode())
        warning_msg = payload.get("warning", "")

        with state_lock:   # FIX: حماية الـ shared state
            if warning_msg in ("Ambulance Passing", "AMBULANCE APPROACHING - CLEAR Road"):
                if not is_override:
                    ambulance_distance = payload.get("distance", "Unknown")
                    print(f"\n📢 WARNING: Ambulance detected at {ambulance_distance}m!")
                    print("⚠️ ATTENTION PEDESTRIANS: Clear the crosswalk immediately!")

                    is_override    = True
                    current_state  = "YELLOW"
                    remaining_time = 3

            elif warning_msg == "Normal Traffic" and is_override:
                print("\n✅ Ambulance has passed. Returning to normal operation.")
                is_override    = False
                current_state  = "RED"
                remaining_time = 10

    except Exception as e:
        print(f"⚠️ Error: {e}")


# ======================== Traffic Cycle ========================

def traffic_cycle_logic(client):
    global current_state, remaining_time, is_override

    while True:
        with state_lock:   # FIX: اقرأ وعدّل جوه الـ lock
            if remaining_time <= 0:
                if is_override:
                    if current_state == "YELLOW":
                        current_state  = "GREEN"
                        remaining_time = 20
                        print("\n🚑 STATUS: Ambulance is passing NOW. Light is GREEN.")
                    else:
                        # وقت الطوارئ خلص ومجاش clear — ارجع طبيعي
                        is_override    = False
                        current_state  = "RED"
                        remaining_time = 10
                else:
                    # الدورة الطبيعية
                    if   current_state == "RED":    current_state = "GREEN";  remaining_time = 15
                    elif current_state == "GREEN":  current_state = "YELLOW"; remaining_time = 3
                    elif current_state == "YELLOW": current_state = "RED";    remaining_time = 10
            else:
                remaining_time -= 1

            # اقرأ القيم للطباعة والنشر
            snap_state   = current_state
            snap_time    = remaining_time
            snap_emg     = is_override

        # النشر بره الـ lock عشان ما نبلوكش
        status_msg = {
            "state":        snap_state,
            "remaining_time": snap_time,
            "is_emergency": snap_emg
        }
        client.publish(STATE_TOPIC, json.dumps(status_msg))

        icon = "🔴" if snap_state == "RED" else "🟢" if snap_state == "GREEN" else "🟡"
        print(f"{icon} Light: {snap_state} | Time: {snap_time}s | Emergency: {snap_emg}")

        time.sleep(1)


# ======================== Main ========================

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(tls_version=ssl.PROTOCOL_TLS)
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT)
threading.Thread(target=traffic_cycle_logic, args=(client,), daemon=True).start()
client.loop_forever()
