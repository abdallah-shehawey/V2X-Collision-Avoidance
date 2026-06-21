# -*- coding: utf-8 -*-
import paho.mqtt.client as mqtt
import ssl
import json
import threading
import time
import csv
import os

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
TRAFFIC_TOPIC         = "v2n/traffic/light/state"      # From ESP32
VEHICLE_TOPIC         = "v2n/vehicle/presence"
PROCESSED_TOPIC       = "V2X/zone1/traffic/processed"    # Command to ESP32

# ============================================================
# LOGGING CONFIGURATION
# ============================================================
LOG_FILE = "traffic_logs.csv"

def log_event(event_type, details):
    """Logs system events to a CSV file for documentation."""
    file_exists = os.path.isfile(LOG_FILE)
    try:
        with open(LOG_FILE, mode='a', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(['Timestamp', 'Event', 'Details'])
            writer.writerow([time.strftime('%Y-%m-%d %H:%M:%S'), event_type, details])
    except Exception as e:
        print(f"❌ Logging Error: {e}")

# Fixed Ambulance ID
AMBULANCE_ID = "REX"

# ============================================================
# SHARED STATE
# ============================================================
latest_state      = "RED"
latest_time       = 10
car_counter       = 0
ambulance_active  = False   # Emergency request from V2X/Car
camera_confirmed  = False   # Emergency confirmation from old camera
camera_ambulance  = False   # Ambulance detected from AI camera code
state_lock        = threading.Lock()

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
                # If the deleted vehicle was the ambulance, reset the flag
                if pid == AMBULANCE_ID:
                    with state_lock:
                        camera_ambulance = False
        
        # If registry is empty, ensure ambulance flag is reset
        if not vehicle_registry:
            with state_lock:
                camera_ambulance = False

# ============================================================
# PUBLISH PROCESSED DATA
# ============================================================
def process_and_publish():
    """Aggregates all data and publishes to the processed topic."""
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time

    with state_lock:
        # Filter nearby vehicles (<= 50 meters)
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

        output = {
            "state"          : latest_state,
            "remaining_time" : latest_time,
            "is_emergency"   : is_emergency,
            "warning"        : "Normal Traffic",
            "density"        : car_counter,
            "closest_vehicle": {
                "plate_id"  : closest["plate_id"]   if closest else None,
                "distance_m": closest["distance_m"] if closest else None
            },
            "nearby_count"   : len(nearby_vehicles),
            "command"        : "emergency" if is_emergency else "normal",
            "mode"           : "ambulance_priority" if is_emergency else "standard"
        }

        # Command Logic: If emergency, send 'emergency' command to ESP32
        command = "emergency" if is_emergency else "normal"
        
        output["command"] = command
        output["mode"] = "ambulance_priority" if is_emergency else "standard"

        client.publish(PROCESSED_TOPIC, json.dumps(output))
        print(f"📤 Published COMMAND -> {command} | emergency:{is_emergency}")
        
        # Log the event
        log_event("TRAFFIC_COMMAND", f"Command: {command} | State: {latest_state} | Emergency: {is_emergency}")

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
        print(f"📡 Subscribed to topics:")
        print(f"   • {CAMERA_TOPIC}")
        print(f"   • {CAMERA_DETECTION_TOPIC}")
        print(f"   • {TRAFFIC_TOPIC}")
        print(f"   • {VEHICLE_TOPIC}")
    else:
        print(f"❌ Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time

    try:
        payload_str = msg.payload.decode().strip()

        # ----------------------------------------------------------
        # 1. Vehicle data from AI Camera
        # ----------------------------------------------------------
        if msg.topic == CAMERA_DETECTION_TOPIC:
            data       = json.loads(payload_str)
            plate_id   = data.get("plate_id", "???")
            distance   = data.get("distance_m", 999)
            is_amb     = data.get("is_ambulance", False)

            # Update Registry
            with registry_lock:
                vehicle_registry[plate_id] = {
                    "plate_id"   : plate_id,
                    "distance_m" : distance,
                    "is_ambulance": is_amb,
                    "last_seen"  : int(time.time())
                }

            print(f"\n🎥 [CameraFeed] plate={plate_id} | dist={distance}m | ambulance={is_amb}")

            # Trigger emergency if it's the fixed ID
            with state_lock:
                if is_amb or (plate_id.strip() == AMBULANCE_ID):
                    camera_ambulance = True
                else:
                    # Don't reset if it's already true from another source
                    pass 

            process_and_publish()

        # ----------------------------------------------------------
        # 2. Legacy Camera detection
        # ----------------------------------------------------------
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
        # 3. Traffic Light State
        # ----------------------------------------------------------
        elif msg.topic == TRAFFIC_TOPIC:
            data = json.loads(payload_str)
            with state_lock:
                latest_state = data.get("state", "RED")
                latest_time  = data.get("remaining_time", 10)
            print(f"🚦 [TrafficLight] state={latest_state} | time={latest_time}s")
            process_and_publish()

        # ----------------------------------------------------------
        # 4. V2X data from Vehicles directly
        # ----------------------------------------------------------
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

# Start Registry Cleanup Thread
threading.Thread(target=cleanup_registry, daemon=True).start()

print("🌐 Core Connecting to HiveMQ Cloud Server...")
client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n🛑 Gateway stopped safely.")