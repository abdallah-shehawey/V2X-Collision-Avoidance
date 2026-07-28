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



# ============================================================
# MQTT SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER         = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT           = 8883
USERNAME       = "v2n_admin"
PASSWORD       = "V2n@2026!"


# ============================================================
# MQTT TOPICS
# ============================================================
CAMERA_TOPIC           = "v2n/camera/detection"
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"
TRAFFIC_STATE_PREFIX   = "v2n/traffic/light/state/"       # + lane id, e.g. .../A
TRAFFIC_TOPIC_WILDCARD = TRAFFIC_STATE_PREFIX + "+"
COMMAND_TOPIC_TEMPLATE  = "v2n/traffic/light/command/{lane}"
VEHICLE_TOPIC          = "v2n/vehicle/presence"
PROCESSED_TOPIC        = "V2X/zone1/traffic/processed"

# Unique hardware identification for prioritized vehicles
AMBULANCE_ID = "REX"

# ============================================================
# CONFLICT RESOLUTION MODULE (multiple lanes / simultaneous emergencies)
# ============================================================
# This gateway is "the brain" for a small intersection with two perpendicular
# lanes: only one of them can safely be granted a Green Corridor at a time.
# The car-facing PROCESSED_TOPIC output below still mirrors a single lane
# (PRIMARY_LANE) for backwards compatibility with Car_client.py, which has
# no lane concept of its own.
LANES                     = ["A", "B"]
CONFLICT_MAP              = {"A": "B", "B": "A"}
PRIMARY_LANE              = "A"
EMERGENCY_REQUEST_TIMEOUT = 5   # seconds a lane's emergency request stays valid without a fresh detection

# lane -> {"distance_m": float, "last_seen": ts, "plate_id": str} | None
emergency_requests   = {lane: None for lane in LANES}
active_corridor_lane  = None   # lane currently holding the Green Corridor grant, or None
conflict_lock         = threading.Lock()

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

    Note: this only ever acquires one lock at a time (registry_lock, then
    separately state_lock). process_and_publish() always acquires them in
    the order state_lock -> registry_lock; nesting them here in the reverse
    order would be an ABBA deadlock waiting to happen against that thread.
    """
    global camera_ambulance
    while True:
        time.sleep(2)
        current_time = int(time.time())
        ambulance_removed = False

        with registry_lock:
            to_delete = []
            for pid, data in vehicle_registry.items():
                if current_time - data.get("last_seen", 0) > 5:
                    to_delete.append(pid)

            for pid in to_delete:
                print(f"🧹 Clearing stale vehicle data: {pid}")
                entry = vehicle_registry[pid]
                # Match on the is_ambulance flag (set by the camera feed itself),
                # not just the ID string, since the gateway's AMBULANCE_ID
                # constant can differ from the one the camera scripts use.
                # camera_ambulance is PRIMARY_LANE-scoped (see on_message), so
                # only that lane's removals should be able to clear it.
                is_amb_entry = entry.get("is_ambulance") or pid == AMBULANCE_ID
                if is_amb_entry and entry.get("lane_id", PRIMARY_LANE) == PRIMARY_LANE:
                    ambulance_removed = True
                vehicle_registry.pop(pid, None)

            primary_lane_empty = not any(
                v.get("lane_id", PRIMARY_LANE) == PRIMARY_LANE
                for v in vehicle_registry.values()
            )

        if ambulance_removed or primary_lane_empty:
            with state_lock:
                camera_ambulance = False


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


def conflict_resolution_watchdog():
    """
    Periodically re-evaluates emergency requests even without a new camera
    message arriving, so a stale request expires (and any held lane gets
    released) in a timely way rather than only on the next detection.
    """
    while True:
        time.sleep(1)
        _resolve_conflicts()


# ============================================================
# CONFLICT RESOLUTION MODULE
# ============================================================

def _send_command(lane, command):
    """Publishes a HOLD_RED / GREEN_CORRIDOR / RELEASE command to one lane's Traffic_light_GUI.py."""
    topic = COMMAND_TOPIC_TEMPLATE.format(lane=lane)
    client.publish(topic, json.dumps({"command": command}))
    print(f"📡 [ConflictResolution] -> lane {lane}: {command}")


def _resolve_conflicts():
    """
    Decides which lane (if any) gets Green Corridor priority right now, and
    holds every lane that conflicts with it at RED. Policy: closest reported
    distance wins. Only sends commands when the winner actually changes, so
    lanes aren't spammed with redundant HOLD_RED/GREEN_CORRIDOR every tick.
    """
    global active_corridor_lane

    with conflict_lock:
        now = time.time()
        for lane in LANES:
            req = emergency_requests[lane]
            if req and now - req["last_seen"] > EMERGENCY_REQUEST_TIMEOUT:
                print(f"🧹 [ConflictResolution] Lane {lane}'s emergency request expired")
                emergency_requests[lane] = None

        active = {lane: req for lane, req in emergency_requests.items() if req}

        if not active:
            if active_corridor_lane is not None:
                print("✅ [ConflictResolution] No active emergencies -> releasing all lanes")
                for lane in LANES:
                    _send_command(lane, "RELEASE")
                active_corridor_lane = None
            return

        winner_lane = min(active, key=lambda l: active[l]["distance_m"])

        if winner_lane != active_corridor_lane:
            print(f"🚨 [ConflictResolution] Lane {winner_lane} wins Green Corridor "
                  f"(closest active request at {active[winner_lane]['distance_m']}m)")
            _send_command(winner_lane, "GREEN_CORRIDOR")
            for lane in LANES:
                if lane != winner_lane and CONFLICT_MAP.get(winner_lane) == lane:
                    _send_command(lane, "HOLD_RED")
            active_corridor_lane = winner_lane


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
                "distance_m": closest["distance_m"] if closest else  "NONE" 
            },
            "nearby_count"   : len(nearby_vehicles)
        }

        # Handle infrastructure blackout notification
        if not traffic_light_online:
            output["warning"] = "🚫 No Traffic Light detected in this zone (Infrastructure Offline)"

        # Handle active priority preemption logging and alerting
        if is_emergency:
            emergency_plate = None
            emergency_distance = None
            with registry_lock:
                if AMBULANCE_ID in vehicle_registry:
                    emergency_plate = AMBULANCE_ID
                    emergency_distance = vehicle_registry[AMBULANCE_ID].get("distance_m")
                else:
                    for pid, data in vehicle_registry.items():
                        if data.get("is_ambulance"):
                            emergency_plate = pid
                            emergency_distance = data.get("distance_m")
                            break

            # Report the ambulance's own distance, not the closest vehicle's
            # (which may be an unrelated nearer car) - `closest` above is only
            # the nearest vehicle overall, not necessarily the ambulance.
            if emergency_distance is not None:
                output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}] at {emergency_distance}m! NORMAL CARS MUST STOP! 🚨"
            else:
                output["warning"] = f"🚨 AMBULANCE APPROACHING [{emergency_plate or AMBULANCE_ID}]! NORMAL CARS MUST STOP! 🚨"

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
        client.subscribe(TRAFFIC_TOPIC_WILDCARD)
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
            lane_id    = data.get("lane_id", PRIMARY_LANE)
            if lane_id not in LANES:
                lane_id = PRIMARY_LANE

            with registry_lock:
                vehicle_registry[plate_id] = {
                    "plate_id"   : plate_id,
                    "distance_m" : distance,
                    "is_ambulance": is_amb,
                    "lane_id"    : lane_id,
                    "last_seen"  : int(time.time())
                }

            print(f"\n🎥 [CameraFeed] lane={lane_id} plate={plate_id} | dist={distance}m | ambulance={is_amb}")

            # camera_ambulance drives the car-facing (PRIMARY_LANE) is_emergency
            # flag, so it should only latch for that lane - an ambulance on a
            # different, perpendicular lane is not this car's emergency.
            if lane_id == PRIMARY_LANE:
                with state_lock:
                    if is_amb or (plate_id.strip() == AMBULANCE_ID):
                        camera_ambulance = True

            if is_amb or (plate_id.strip() == AMBULANCE_ID):
                with conflict_lock:
                    emergency_requests[lane_id] = {
                        "distance_m": distance,
                        "last_seen": time.time(),
                        "plate_id": plate_id,
                    }
                _resolve_conflicts()

            process_and_publish()

        # Handle binary legacy classification feeds
        elif msg.topic == CAMERA_TOPIC:
            # Only trust the explicit positive marker - a loose "ambulance" in
            # payload_str.lower() substring check would also match messages
            # like "No ambulance detected", falsely latching the emergency state.
            if "ambulance verified" in payload_str.lower():
                with state_lock:
                    camera_confirmed = True
                print(f"📸 [Gateway AI-Log] Legacy Camera confirmed Ambulance!")
            else:
                with state_lock:
                    camera_confirmed = False
            process_and_publish()

        # Handle state broadcast synchronizations received from the RSU Traffic Light
        # Controller(s) - one per lane, topic suffix is the lane id (.../state/A, .../state/B).
        # Only PRIMARY_LANE drives the legacy single-lane globals/output below;
        # other lanes' heartbeats are logged but don't affect Car_client.py's contract.
        elif msg.topic.startswith(TRAFFIC_STATE_PREFIX):
            lane = msg.topic[len(TRAFFIC_STATE_PREFIX):]
            data = json.loads(payload_str)
            print(f"🚦 [TrafficLight:{lane}] {data.get('state','?')} ➔ {data.get('next_state','?')} "
                  f"| Code: {data.get('transition_code','?')} | Time: {data.get('remaining_time','?')}s")

            if lane == PRIMARY_LANE:
                with state_lock:
                    latest_state           = data.get("state", "RED")
                    latest_next_state      = data.get("next_state", "GREEN")
                    latest_transition_code = data.get("transition_code", 1)
                    latest_time            = data.get("remaining_time", 10)
                    traffic_light_online   = True
                    last_traffic_update    = time.time()
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
threading.Thread(target=conflict_resolution_watchdog, daemon=True).start()

print("🌐 Core Connecting to HiveMQ Cloud Server...")
client.connect(BROKER, PORT, 60)

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\n🛑 Gateway stopped safely.")