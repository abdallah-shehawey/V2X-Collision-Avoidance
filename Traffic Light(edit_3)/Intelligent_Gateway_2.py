"""
******************************************************************************
* @file           : Intelligent_Gateway_unified.py
* @brief          : Smart Gateway — Unified Vehicle Handler
 @author         : Amira Atef
* @description    :
*   بيستقبل كل العربيات من توبيك واحد v2n/vehicle/presence
*   وبيفرق بناءً على الـ vehicle_id:
*     A-xxx → سيناريو الأولوية (إسعاف)
*     C-xxx → يبعتله حالة الإشارة بس (عادية)
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

ZONE               = "zone1"
DISTANCE_THRESHOLD = 100
EMERGENCY_TIMEOUT  = 60

# --- Topics ---
VEHICLE_TOPIC       = "v2n/vehicle/presence"           # من كل العربيات
TRAFFIC_STATE_TOPIC = "v2n/traffic/light/state"        # من الإشارة
CAR_ID_TOPIC        = f"V2X/{ZONE}/car_id"
TRAFFIC_OUT_TOPIC   = f"V2X/{ZONE}/traffic/processed"  # للإشارة
CONFIRM_TOPIC       = "v2n/ambulance/confirmation"     # للإسعاف
TRAFFIC_INFO_TOPIC  = "v2n/traffic/light/state"        # للعربيات العادية

# --- State ---
latest_state     = None
car_counter      = 0
ambulance_active = False
emergency_timer  = None
state_lock       = threading.Lock()


# ======================== Helpers ========================

def is_ambulance(vehicle_id: str) -> bool:
    return str(vehicle_id).upper().startswith("A")

def calculate_green_time(count: int) -> int:
    if count < 5:    return 8
    elif count < 15: return 12
    else:            return 20

def reset_counter():
    global car_counter
    while True:
        time.sleep(5)
        with state_lock:
            print(f"📊 Traffic Density: {car_counter} cars/5s")
            car_counter = 0

def _auto_cancel_emergency():
    global ambulance_active
    print("⏰ Emergency timeout — returning to normal.")
    with state_lock:
        ambulance_active = False
    _publish_normal_traffic()

def _start_emergency_timer():
    global emergency_timer
    if emergency_timer:
        emergency_timer.cancel()
    emergency_timer = threading.Timer(EMERGENCY_TIMEOUT, _auto_cancel_emergency)
    emergency_timer.daemon = True
    emergency_timer.start()

def _cancel_emergency_timer():
    global emergency_timer
    if emergency_timer:
        emergency_timer.cancel()
        emergency_timer = None

def _send_confirmation(vehicle_id: str, intersection_id: str,
                       status: str, duration: int = 20, reason: str = ""):
    msg = {
        "ambulance_id":    vehicle_id,
        "vehicle_id":      vehicle_id,
        "status":          status,
        "intersection_id": intersection_id,
        "duration":        duration,
        "reason":          reason,
        "timestamp":       int(time.time() * 1000)
    }
    client.publish(CONFIRM_TOPIC, json.dumps(msg))
    print(f"📨 Confirmation → {vehicle_id}: {status}")

def _publish_normal_traffic():
    msg = {
        "state":          "RED",
        "warning":        "Normal Traffic",
        "remaining_time": 10,
        "density":        car_counter
    }
    client.publish(TRAFFIC_OUT_TOPIC, json.dumps(msg))


# ======================== Vehicle Handlers ========================

def handle_ambulance(payload: dict):
    """بيعالج الـ messages الجاية من الإسعاف."""
    global ambulance_active

    vehicle_id  = payload.get("ambulance_id") or payload.get("vehicle_id")
    command     = payload.get("command", "")
    int_id      = payload.get("intersection_id", "INT-001")
    distance    = payload.get("distance", 999)

    if command == "request_priority":
        if distance <= DISTANCE_THRESHOLD:
            with state_lock:
                ambulance_active = True

            print(f"🚨 EMERGENCY: {vehicle_id} at {distance}m — Priority activated")

            # بعت confirmation للإسعاف
            _send_confirmation(vehicle_id, int_id, status="granted", duration=20)

            # بعت أمر طوارئ للإشارة
            client.publish(TRAFFIC_OUT_TOPIC, json.dumps({
                "state":          "YELLOW",
                "warning":        "Ambulance Passing",
                "remaining_time": 3
            }))

            _start_emergency_timer()

        else:
            print(f"ℹ️ {vehicle_id} at {distance}m — Too far, denying")
            _send_confirmation(vehicle_id, int_id, status="denied",
                               reason=f"Distance {distance}m > {DISTANCE_THRESHOLD}m threshold")

    elif command == "clear_priority":
        print(f"✅ {vehicle_id} cleared intersection {int_id}")
        with state_lock:
            ambulance_active = False
        _cancel_emergency_timer()
        _publish_normal_traffic()


def handle_normal_car(payload: dict):
    """
    بيعالج الـ messages الجاية من العربيات العادية.
    بس بيحسب الكثافة — الإشارة بتبعت حالتها للكل تلقائياً.
    """
    global car_counter
    vehicle_id = payload.get("vehicle_id", "UNKNOWN")
    distance   = payload.get("distance", 999)

    with state_lock:
        car_counter += 1

    print(f"🚗 Normal car: {vehicle_id} | distance={distance}m | total count={car_counter}")


# ======================== MQTT Callbacks ========================

def on_connect(mqttc, userdata, flags, rc):
    if rc == 0:
        print("✅ Gateway Connected to HiveMQ Cloud")
        mqttc.subscribe(VEHICLE_TOPIC)          # كل العربيات
        mqttc.subscribe(TRAFFIC_STATE_TOPIC)    # حالة الإشارة
        mqttc.subscribe(CAR_ID_TOPIC)           # عداد قديم (متوافق)
    else:
        print(f"❌ Connection failed, rc={rc}")


def on_message(mqttc, userdata, msg):
    global latest_state

    try:
        topic   = msg.topic
        payload = json.loads(msg.payload.decode())

        # ---- رسايل العربيات الموحدة ----
        if topic == VEHICLE_TOPIC:
            vehicle_id = payload.get("ambulance_id") or payload.get("vehicle_id", "")

            if is_ambulance(vehicle_id):
                handle_ambulance(payload)
            else:
                handle_normal_car(payload)

        # ---- حالة الإشارة ----
        elif topic == TRAFFIC_STATE_TOPIC:
            with state_lock:
                latest_state = payload
            process_and_publish()

        # ---- عداد قديم — للتوافق ----
        elif topic == CAR_ID_TOPIC:
            with state_lock:
                car_counter += 1

    except Exception as e:
        print(f"⚠️ Gateway error: {e}")


def process_and_publish():
    """يعالج حالة الإشارة ويبعت الـ output."""
    with state_lock:
        if latest_state is None:
            return
        output        = latest_state.copy()
        is_amb_active = ambulance_active
        count         = car_counter

    if is_amb_active:
        output["state"]          = "GREEN"
        output["warning"]        = "Ambulance Passing"
        output["remaining_time"] = 20
    else:
        if output.get("state") == "GREEN":
            output["remaining_time"] = calculate_green_time(count)
        output["density"] = count
        output["warning"] = "Normal Traffic"

    client.publish(TRAFFIC_OUT_TOPIC, json.dumps(output))


# ======================== Main ========================

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(tls_version=ssl.PROTOCOL_TLS)
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT)
threading.Thread(target=reset_counter, daemon=True).start()
client.loop_forever()
