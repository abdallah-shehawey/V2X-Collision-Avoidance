"""
Car_client-1.py - V2X On-Board Unit (OBU) — Car Display Client
================================================================
Connects to HiveMQ Cloud (V2N) to receive processed traffic data from
the Intelligent Gateway, then republishes it to the local IPC hub so
all Raspberry Pi processes (V2P, dashboard_bridge, etc.) can consume it.

Data received from Gateway (HiveMQ)
-------------------------------------
    state           — current traffic light color (RED / YELLOW / GREEN)
    remaining_time  — seconds left in the current phase
    is_emergency    — ambulance override active
    transition_flag — what the light is about to become (0/1/2/3/-1)
    warning         — human-readable status string
    closest_vehicle — dict with distance_m to the traffic light

IPC topics published to local hub
-----------------------------------
    "local/traffic/processed"
        Full raw Gateway payload — any subscriber can consume it directly.

    "traffic_light_state"
        Compact traffic light info for V2P.py:
            { state, remaining_time, transition_flag, is_emergency }

    "v2n_frame"
        Compact flags + raw fields for dashboard_bridge.py:
            { timestamp, frame_hex,
              traffic_flag, ambulance_flag, distance_to_light_m,
              state, remaining_time, transition_flag, crossing_flag }

V2N bit-packed frame (2 bytes)
--------------------------------
    byte0 bits [0:2]  traffic_flag    0=no light, 1=stop (red/yellow), 2=go (green)
    byte0 bit  [2]    ambulance_flag  0=normal,   1=ambulance passing
    byte0 bits [3:8]  reserved (0)
    byte1             distance_to_light_m  0-254 m, 255=unknown

transition_flag reference (from trafic_light.py)
--------------------------------------------------
    0  GREEN  -> YELLOW  (caution, start slowing)
    1  YELLOW -> RED     (must stop)
    2  RED    -> YELLOW  (wait, about to go)
    3  YELLOW -> GREEN   (about to go)
   -1  mid-phase, no imminent transition

crossing_flag
--------------
    Calculated from speed (IPC hub "vehicle_speed" topic), distance, and
    remaining_time.
    1 = car will reach the light before it changes (Rt >= t)
    0 = car won't make it, or light is not green / going green
"""

import paho.mqtt.client as mqtt
import ssl
import json
import os
import struct
import time
import threading

from ipc_node import IPCNode

# ============================================================
# SERVER CONFIGURATION
# ============================================================
BROKER          = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT            = 8883
USERNAME        = "v2n_admin"
PASSWORD        = "V2n@2026!"

# Final processed topic coming from the Intelligent Gateway
PROCESSED_TOPIC = "V2X/zone1/traffic/processed"

# ============================================================
# V2N bit-packed frame
# ============================================================
V2N_FRAME_FMT = "<BB"


def pack_v2n_frame(traffic_flag: int, ambulance_flag: int, distance_m) -> bytes:
    """Pack traffic_flag (2 bits) + ambulance_flag (1 bit) + distance (1 byte)."""
    traffic_flag   &= 0b11
    ambulance_flag &= 0b1
    packed = (ambulance_flag << 2) | traffic_flag

    if distance_m is None:
        dist_byte = 255   # sentinel = unknown / no reading
    else:
        dist_byte = max(0, min(254, int(round(distance_m))))

    return struct.pack(V2N_FRAME_FMT, packed, dist_byte)


def compute_traffic_flag(state: str, has_data: bool) -> int:
    """0=no signal data, 1=stop (red/yellow), 2=go (green)."""
    if not has_data:
        return 0
    if state == "GREEN":
        return 2
    if state in ("RED", "YELLOW"):
        return 1
    return 0


def compute_ambulance_flag(is_emergency: bool) -> int:
    """0=normal / no ambulance, 1=ambulance present and cleared to pass."""
    return 1 if is_emergency else 0


def compute_crossing_flag(
    speed_kmh:       float,
    distance_m,
    remaining_time:  int,
    transition_flag: int,
    light_state:     str,
) -> int:
    """
    Decide whether the car can safely cross before the light changes.

    Returns
    -------
    1  — car will make it (remaining_time >= time_to_light)
    0  — car won't make it, or light is not green / not about to go green

    The calculation only makes sense when the light is GREEN or turning
    GREEN (transition_flag in (1, 6)). All other cases return 0. (Note: Codes 1 and 6 are from the new GUI)
    """
    if light_state not in ("GREEN",) and transition_flag not in (1, 6):
        return 0
    if distance_m is None or distance_m <= 0:
        return 0
    if speed_kmh is None or speed_kmh < 1.0:
        return 0

    time_to_light = distance_m / (speed_kmh / 3.6)
    return 1 if remaining_time >= time_to_light else 0


# ============================================================
# INITIALIZE LOCAL IPC NODE
# ============================================================
# ◄--- 1. Create and name the IPC node for the car to connect to the hub
ipc_local = IPCNode("car_display")

# ============================================================
# SPEED — received from IPC hub (published by server.py UART bridge)
# ============================================================
_speed_lock   = threading.Lock()
_latest_speed = 0.0


def _on_vehicle_speed(topic, data, sender):
    """Callback: receive current vehicle speed from server.py via IPC hub."""
    global _latest_speed
    spd = data.get("speed_kmh", 0.0)
    with _speed_lock:
        _latest_speed = float(spd)


# ============================================================
# VARIABLES TO STORE RECEIVED DATA
# ============================================================
# Updated continuously in the background from incoming Gateway messages
latest_received_distance = None
latest_remaining_time    = None


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
        data        = json.loads(payload_str)

        # ◄--- 2. Core step: forward full raw payload to local hub immediately
        ipc_local.publish("local/traffic/processed", data)

        # ── Extract all fields from Gateway payload ──────────────────
        state           = data.get("state",           "UNKNOWN")
        next_state      = data.get("next_state",      "UNKNOWN")
        remaining_time  = data.get("remaining_time",  0)
        is_emergency    = data.get("is_emergency",    False)
        transition_flag = data.get("transition_code", -1)
        warning_msg     = data.get("warning",         "Normal Traffic")

        closest_vehicle          = data.get("closest_vehicle") or {}
        latest_received_distance = closest_vehicle.get("distance_m", None)
        latest_remaining_time    = remaining_time

        # ◄--- 3. Publish compact traffic light state for V2P.py
        # V2P subscribes to "traffic_light_state" to know the current
        # signal without consuming the full raw Gateway payload.
        ipc_local.publish("traffic_light_state", {
            "state":           state,
            "next_state":      next_state,
            "remaining_time":  remaining_time,
            "transition_flag": transition_flag,
            "is_emergency":    is_emergency,
        })

        # ◄--- 4. Compute V2N flags and publish compact frame for dashboard_bridge
        has_traffic_data = state in ("RED", "YELLOW", "GREEN")
        traffic_flag     = compute_traffic_flag(state, has_traffic_data)
        ambulance_flag   = compute_ambulance_flag(is_emergency)
        distance_m       = latest_received_distance

        v2n_frame_bytes  = pack_v2n_frame(traffic_flag, ambulance_flag, distance_m)

        # Read latest speed from IPC hub (set by server.py UART bridge)
        with _speed_lock:
            current_speed = _latest_speed

        crossing_flag = compute_crossing_flag(
            speed_kmh       = current_speed,
            distance_m      = distance_m,
            remaining_time  = remaining_time,
            transition_flag = transition_flag,
            light_state     = state,
        )

        ipc_local.publish("v2n_frame", {
            "timestamp":           time.time(),
            "frame_hex":           v2n_frame_bytes.hex(),
            # ── Compact flags ──
            "traffic_flag":        traffic_flag,       # 0=no data, 1=stop, 2=go
            "ambulance_flag":      ambulance_flag,     # 0/1
            "distance_to_light_m": distance_m,
            # ── Full light-cycle state (for dashboard) ──
            "state":               state,
            "next_state":          next_state,
            "remaining_time":      remaining_time,
            "transition_flag":     transition_flag,    # 0/1/2/3/-1
            # ── Crossing result ──
            "crossing_flag":       crossing_flag,      # 1=go, 0=stop/caution
        })

        # ── Console display — no state, no remaining time ────────────
        os.system('cls' if os.name == 'nt' else 'clear')
        print("=" * 60)
        print("🚘 V2X CAR ON-BOARD DISPLAY (OBU) 🚘")
        print("=" * 60)
        print(f"  Light Status      : {state} ➔ {next_state} ({remaining_time}s remaining)")
        print(f"  Distance to light : {f'{distance_m:.1f} m' if distance_m else 'N/A'}")
        print(f"  Speed             : {current_speed:.1f} km/h")
        print(f"  Crossing          : {'✅ CAN CROSS' if crossing_flag == 1 else '⛔ STOP / CAUTION'}")
        print("-" * 60)
        if is_emergency:
            print(f"⚠️  NOTIFICATION : {warning_msg}")
        else:
            print(f"ℹ️  STATUS       : {warning_msg}")
        print("=" * 60)

    except Exception as e:
        print(f"❌ Error parsing display data: {e}")


# ============================================================
# MAIN RUNNER
# ============================================================
if __name__ == "__main__":

    print("=" * 60)
    print("  V2X Car Client Initializing")
    print("=" * 60)

    # ◄--- 5. Connect to local hub first and subscribe to vehicle_speed
    print("🏠 Connecting Car Display to Local IPC Hub...")
    if ipc_local.connect():
        ipc_local.subscribe("vehicle_speed", _on_vehicle_speed)
        ipc_local.start_listening()
        print("🔗 Successfully linked to Local Hub via IPC-Node.")
        print("📥 Subscribed to 'vehicle_speed' topic.")
    else:
        print("⚠️ Local Hub not found! Running in Cloud-only mode.")

    # Connect to HiveMQ Cloud broker
    print("🌐 Connecting Car HUD to HiveMQ Cloud...")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n🛑 Car Display disconnected.")
