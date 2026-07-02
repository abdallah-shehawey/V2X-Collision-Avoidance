# -*- coding: utf-8 -*-
"""
Author: Eng. Amira
Date: June 2026
Description: Central V2X Gateway node coordinating traffic telemetry and multi-modal emergency preemption.
"""

import paho.mqtt.client as mqtt
import ssl
import json
import threading
import time

# Shared, env-overridable connection + identity config (single source of truth).
# Fixes the startup NameError (BROKER/PORT/USERNAME/PASSWORD were never defined here)
# and the AMBULANCE_ID drift ("REX" here vs "T4RR" in the cameras).
from v2x_config import BROKER, PORT, USERNAME, PASSWORD, AMBULANCE_ID


# ============================================================
# MQTT TOPICS
# ============================================================
CAMERA_TOPIC          = "v2n/camera/detection"
CAMERA_DETECTION_TOPIC= "v2n/camera/vehicle_data"
TRAFFIC_TOPIC         = "v2n/traffic/light/state"
VEHICLE_TOPIC         = "v2n/vehicle/presence"
PROCESSED_TOPIC       = "V2X/zone1/traffic/processed"

# ============================================================
# GLOBAL SHARED RUNTIME STATE
# ============================================================
latest_state           = "RED"
latest_next_state      = "GREEN"
latest_transition_code = 1
latest_time            = 10
car_counter            = 0
ambulance_active       = False   
camera_confirmed       = False   
camera_ambulance       = False   
state_lock             = threading.Lock() # Enforces thread isolation for state mutations

# ------------------------------------------------------------
# Traffic Light Watchdog Variables
# ------------------------------------------------------------
traffic_light_online    = False   # Boolean flag indicating RSU availability
last_traffic_update     = 0       # Unix epoch timestamp of the last received heartbeat
TRAFFIC_LIGHT_TIMEOUT   = 8       # Threshold in seconds before declaring infrastructure drop

# Local registry mapping for actively tracked vehicle plate data
vehicle_registry  = {}
registry_lock     = threading.Lock()

# ============================================================
# BACKGROUND SYSTEM THREADS
# ============================================================

def cleanup_registry():
    """
    Background worker thread that continually flushes stale vehicle records.
    Removes vehicles from the local tracking registry if unseen for > 5 seconds
    to optimize system memory and maintain data fresh accuracy.
    """
    global camera_ambulance
    while True:
        time.sleep(2)
        current_time = int(time.time())

        # Phase 1 — registry mutation under registry_lock ONLY.
        # We must NOT take state_lock while holding registry_lock here: cleanup takes
        # registry_lock first, but process_and_publish() takes state_lock first, so
        # nesting them in this thread would create an ABBA deadlock. Collect now,
        # update shared state after the lock is released.
        with registry_lock:
            to_delete = [
                pid for pid, data in vehicle_registry.items()
                if current_time - data.get("last_seen", 0) > 5
            ]
            for pid in to_delete:
                print(f"🧹 Clearing stale vehicle data: {pid}")
                vehicle_registry.pop(pid, None)

            # Derive the emergency flag purely from what the camera currently sees.
            # This makes it impossible to stay latched ON after the ambulance record
            # expires while normal cars keep the registry non-empty.
            any_ambulance = any(
                (pid == AMBULANCE_ID) or data.get("is_ambulance")
                for pid, data in vehicle_registry.items()
            )

        # Phase 2 — shared-state update under state_lock ONLY (registry_lock released).
        with state_lock:
            camera_ambulance = any_ambulance


def traffic_light_watchdog():
    """
    Monitors infrastructure connectivity status (Heartbeat Watchdog).
    If the traffic light RSU drops connection past the designated timeout,
    it transitions state variables to a safe 'OFFLINE' configuration to 
    protect downstream connected vehicle clients from type parsing exceptions.
    """
    global traffic_light_online, latest_state, latest_next_state
    global latest_transition_code, latest_time

    while True:
        time.sleep(2)
        became_offline = False

        with state_lock:
            if traffic_light_online and (time.time() - last_traffic_update > TRAFFIC_LIGHT_TIMEOUT):
                traffic_light_online    = False
                latest_state            = "OFFLINE"   
                latest_next_state       = "OFFLINE"   
                latest_transition_code  = -1          
                latest_time             = 0           
                became_offline = True

        if became_offline:
            print("🚦❌ Traffic light not responding -> Zone has NO traffic light now (No physical unit active).")
            process_and_publish()

# ============================================================
# DATA ARBITRATION AND CORRELATION PIPELINE
# ============================================================

def process_and_publish():
    """
    Compiles, formats, and publishes the unified state telemetry payload.
    Resolves data fields, manages missing values securely with fallback constraints 
    ('NONE' / -1) to defend connected car platforms against NoneType parsing crashes.
    """
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time, latest_next_state, latest_transition_code

    with state_lock:
        with registry_lock:
            # Filter vehicles detected within the immediate 50-meter safety zone
            nearby_vehicles = {
                pid: data for pid, data in vehicle_registry.items()
                if data.get("distance_m", 999) <= 50
            }
            closest = min(
                nearby_vehicles.values(),
                key=lambda x: x["distance_m"],
                default=None
            )

        # Evaluate global multi-modal emergency preemption status
        is_emergency = ambulance_active or camera_confirmed or camera_ambulance

        # Build secure, standardized output JSON schema
        output = {
            "traffic_light_present": traffic_light_online,
            "state"            : latest_state,
            "next_state"       : latest_next_state,
            "transition_code"  : latest_transition_code,
            "remaining_time"   : latest_time,
            "is_emergency"     : is_emergency,
            "warning"          : "Normal Traffic",
            "density"          : car_counter,
            "closest_vehicle": {
                "plate_id"  : closest["plate_id"]   if closest else "NONE", 
                "distance_m": closest["distance_m"] if closest else -1      
            },
            "nearby_count"   : len(nearby_vehicles)
        }

        # Handle infrastructure blackout notification
        if not traffic_light_online:
            output["warning"] = "🚫 No Traffic Light detected in this zone (Infrastructure Offline)"

        # Handle active priority preemption logging and alerting
        if is_emergency:
            # Report the AMBULANCE's own plate + distance, not whichever vehicle
            # happens to be closest to the camera (a nearer normal car would otherwise
            # make the warning read "AMBULANCE at 3 m" while it is actually 40 m away).
            emergency_plate, emergency_dist = None, None
            with registry_lock:
                for pid, data in vehicle_registry.items():
                    if pid == AMBULANCE_ID or data.get("is_ambulance"):
                        emergency_plate = pid
                        emergency_dist  = data.get("distance_m")
                        break

            if emergency_dist is not None:
                output["warning"] = (f"🚨 AMBULANCE APPROACHING [{emergency_plate}] "
                                     f"at {emergency_dist}m! NORMAL CARS MUST STOP! 🚨")
            else:
                output["warning"] = (f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}]! "
                                     f"NORMAL CARS MUST STOP! 🚨")

        client.publish(PROCESSED_TOPIC, json.dumps(output))
        print(f"📤 Forwarded -> state:{latest_state} | code:{latest_transition_code} | time:{latest_time}s | emergency:{is_emergency}")

# ============================================================
# ASYNCHRONOUS MQTT PROTOCOL CALLBACKS
# ============================================================

def on_connect(client, userdata, flags, reason_code, properties=None):
    """Callback triggered automatically upon establishing a broker handshake connection."""
    if reason_code == 0:
        print("✅ Intelligent Gateway connected to HiveMQ Cloud!")
        client.subscribe(CAMERA_TOPIC)
        client.subscribe(CAMERA_DETECTION_TOPIC)   
        client.subscribe(TRAFFIC_TOPIC)
        client.subscribe(VEHICLE_TOPIC)
    else:
        print(f"❌ Connection failed: {reason_code}")


def on_message(client, userdata, msg):
    """Asynchronous pipeline routing arriving payloads to respective processors."""
    global ambulance_active, camera_confirmed, camera_ambulance
    global car_counter, latest_state, latest_time, latest_next_state, latest_transition_code
    global traffic_light_online, last_traffic_update

    try:
        payload_str = msg.payload.decode().strip()

        # Handle telemetry payloads originating from edge computer vision tracking nodes
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

        # Handle binary legacy classification feeds
        elif msg.topic == CAMERA_TOPIC:
            # Only an explicit positive phrase confirms an ambulance. A bare substring
            # match on "ambulance" also fires on "no ambulance", "ambulance cleared",
            # "searching for ambulance", etc. — any of which would wrongly latch the
            # junction into emergency preemption.
            p = payload_str.lower()
            POSITIVE_PHRASES = ("ambulance verified", "ambulance detected", "ambulance confirmed")
            NEGATIVE_HINTS   = ("no ambulance", "not ambulance", "cleared")
            is_positive = (any(s in p for s in POSITIVE_PHRASES)
                           and not any(s in p for s in NEGATIVE_HINTS))
            with state_lock:
                camera_confirmed = is_positive
            if is_positive:
                print("📸 [Gateway AI-Log] Legacy Camera confirmed Ambulance!")
            process_and_publish()

        # Handle state broadcast synchronizations received from the RSU Traffic Light Controller
        elif msg.topic == TRAFFIC_TOPIC:
            data = json.loads(payload_str)
            with state_lock:
                latest_state           = data.get("state", "RED")
                latest_next_state      = data.get("next_state", "GREEN")
                latest_transition_code = data.get("transition_code", 1)
                latest_time            = data.get("remaining_time", 10)
                traffic_light_online   = True
                last_traffic_update    = time.time()
            print(f"🚦 [TrafficLight Update] Current: {latest_state} ➔ Next: {latest_next_state} | Code: {latest_transition_code} | Time: {latest_time}s")
            process_and_publish()

        # Handle direct localized V2X alerts and standard induction loops loop messages
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
# ENTRY POINT
# ============================================================
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(USERNAME, PASSWORD)
client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

client.on_connect = on_connect
client.on_message = on_message

# Spawn persistent asynchronous ecosystem daemon workers
threading.Thread(target=cleanup_registry, daemon=True).start()
threading.Thread(target=traffic_light_watchdog, daemon=True).start()

print("🌐 Core Connecting to HiveMQ Cloud Server...")
client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n🛑 Gateway stopped safely.")