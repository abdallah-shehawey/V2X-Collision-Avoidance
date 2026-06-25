# -*- coding: utf-8 -*-
import paho.mqtt.client as mqtt
import ssl
import json
import threading
import time

# ============================================================
# SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# ============================================================
# TOPICS
# ============================================================
CAMERA_TOPIC          = "v2n/camera/detection"         
CAMERA_DETECTION_TOPIC= "v2n/camera/vehicle_data"      
TRAFFIC_TOPIC         = "v2n/traffic/light/state"
VEHICLE_TOPIC         = "v2n/vehicle/presence"
PROCESSED_TOPIC       = "V2X/zone1/traffic/processed"

# Fixed Ambulance ID
AMBULANCE_ID = "REX"

# ============================================================
# SHARED STATE (تمت إضافة متغيرات الانتقال الجديدة)
# ============================================================
latest_state           = "RED"
latest_next_state      = "GREEN"
latest_transition_code = 1
latest_time            = 10
car_counter            = 0
ambulance_active       = False   
camera_confirmed       = False   
camera_ambulance       = False   
state_lock             = threading.Lock()

# Dictionary to store latest data for each detected plate
vehicle_registry  = {}
registry_lock     = threading.Lock()

# ============================================================
# REGISTRY CLEANUP LOGIC
# ============================================================
def cleanup_registry():
    """Removes vehicles that haven't been seen for more than 5 seconds."""
    global camera_ambulance
    while True:
        time.sleep(2)
        current_time = int(time.time())
        with registry_lock:
            to_delete = []
            for pid, data in vehicle_registry.items():
                if current_time - data.get("last_seen", 0) > 5:
                    to_delete.append(pid)
            
            for pid in to_delete:
                print(f"🧹 Clearing stale vehicle data: {pid}")
                vehicle_registry.pop(pid, None)
                if pid == AMBULANCE_ID:
                    with state_lock:
                        camera_ambulance = False
        
        if not vehicle_registry:
            with state_lock:
                camera_ambulance = False

# ============================================================
# PUBLISH PROCESSED DATA
# ============================================================
def process_and_publish():
    """Aggregates all data and publishes to the processed topic."""
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time, latest_next_state, latest_transition_code

    with state_lock:
        with registry_lock:
            nearby_vehicles = {
                pid: data for pid, data in vehicle_registry.items()
                if data.get("distance_m", 999) <= 50
            }
            closest = min(
                nearby_vehicles.values(),
                key=lambda x: x["distance_m"],
                default=None
            )

        is_emergency = ambulance_active or camera_confirmed or camera_ambulance

        # تضمين الكود والحالة القادمة في الحزمة المرسلة للسيارات
        output = {
            "state"            : latest_state,
            "next_state"       : latest_next_state,
            "transition_code"  : latest_transition_code,
            "remaining_time"   : latest_time,
            "is_emergency"     : is_emergency,
            "warning"          : "Normal Traffic",
            "density"          : car_counter,
            "closest_vehicle": {
                "plate_id"  : closest["plate_id"]   if closest else None,
                "distance_m": closest["distance_m"] if closest else None
            },
            "nearby_count"   : len(nearby_vehicles)
        }

        if is_emergency:
            emergency_plate = None
            with registry_lock:
                if AMBULANCE_ID in vehicle_registry:
                    emergency_plate = AMBULANCE_ID
                else:
                    for pid, data in vehicle_registry.items():
                        if data.get("is_ambulance"):
                            emergency_plate = pid
                            break

            if closest and closest.get("distance_m") is not None:
                dist_val = closest["distance_m"]
                output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}] at {dist_val}m! NORMAL CARS MUST STOP! 🚨"
            else:
                output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}]! NORMAL CARS MUST STOP! 🚨"

        client.publish(PROCESSED_TOPIC, json.dumps(output))
        print(f"📤 Forwarded -> state:{latest_state} | code:{latest_transition_code} | time:{latest_time}s | emergency:{is_emergency}")

# ============================================================
# MQTT CALLBACKS
# ============================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Intelligent Gateway connected to HiveMQ Cloud!")
        client.subscribe(CAMERA_TOPIC)
        client.subscribe(CAMERA_DETECTION_TOPIC)   
        client.subscribe(TRAFFIC_TOPIC)
        client.subscribe(VEHICLE_TOPIC)
    else:
        print(f"❌ Connection failed: {reason_code}")

def on_message(client, userdata, msg):
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time, latest_next_state, latest_transition_code

    try:
        payload_str = msg.payload.decode().strip()

        if msg.topic == CAMERA_DETECTION_TOPIC:
            data       = json.loads(payload_str)
            plate_id   = data.get("plate_id", "???")
            distance   = data.get("distance_m", 999)
            is_amb     = data.get("is_ambulance", False)

            with registry_lock:
                vehicle_registry[plate_id] = {
                    "plate_id"   : plate_id,
                    "distance_m" : distance,
                    "is_ambulance": is_amb,
                    "last_seen"  : int(time.time())
                }

            print(f"\n🎥 [CameraFeed] plate={plate_id} | dist={distance}m | ambulance={is_amb}")

            with state_lock:
                if is_amb or (plate_id.strip() == AMBULANCE_ID):
                    camera_ambulance = True

            process_and_publish()

        elif msg.topic == CAMERA_TOPIC:
            if "Ambulance verified" in payload_str or "ambulance" in payload_str.lower():
                with state_lock:
                    camera_confirmed = True
                print(f"📸 [Gateway AI-Log] Legacy Camera confirmed Ambulance!")
            else:
                with state_lock:
                    camera_confirmed = False
            process_and_publish()

        # ----------------------------------------------------------
        # 3. استقبال البيانات وتحديث كود الانتقال المطور
        # ----------------------------------------------------------
        elif msg.topic == TRAFFIC_TOPIC:
            data = json.loads(payload_str)
            with state_lock:
                latest_state           = data.get("state", "RED")
                latest_next_state      = data.get("next_state", "GREEN")
                latest_transition_code = data.get("transition_code", 1)
                latest_time            = data.get("remaining_time", 10)
            print(f"🚦 [TrafficLight Update] Current: {latest_state} ➔ Next: {latest_next_state} | Code: {latest_transition_code} | Time: {latest_time}s")
            process_and_publish()

        elif msg.topic == VEHICLE_TOPIC:
            data         = json.loads(payload_str)
            vehicle_type = data.get("vehicle_type", "")
            command      = data.get("command", "")
            v_id         = data.get("ambulance_id", data.get("vehicle_id", ""))

            is_v2x_ambulance = (vehicle_type == "AMBULANCE") or (v_id and v_id.strip() == AMBULANCE_ID)

            if is_v2x_ambulance:
                if command == "clear_priority":
                    print("🚑 [Gateway V2X] Ambulance passed. Clearing emergency.")
                    with state_lock:
                        ambulance_active = False
                    with registry_lock:
                        vehicle_registry.pop(v_id, None)
                else:
                    print(f"🚑 [Gateway V2X] Ambulance [{v_id}] requesting priority!")
                    with state_lock:
                        ambulance_active = True
            else:
                with state_lock:
                    car_counter = (car_counter + 1) % 10

            process_and_publish()

    except Exception as e:
        print(f"❌ Error processing MQTT Message: {e}")

# ============================================================
# MAIN
# ============================================================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

client.on_connect = on_connect
client.on_message = on_message

threading.Thread(target=cleanup_registry, daemon=True).start()

print("🌐 Core Connecting to HiveMQ Cloud Server...")
client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n🛑 Gateway stopped safely.")