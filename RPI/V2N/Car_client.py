# -*- coding: utf-8 -*-
"""
Car_client.py — V2X Car On-Board Unit (OBU)  [FIXED]
=====================================================

Fix summary
-----------
The Intelligent_Gateway sends the string "NONE" for closest_vehicle.plate_id
and closest_vehicle.distance_m when no vehicle is nearby. The old code did
float("NONE") -> ValueError -> the exception fired BEFORE _publish_v2n_frame(),
so nothing reached the IPC hub (and V2P / dashboard got no data).

Now every incoming field goes through a safe converter, so a bad/missing
value can never kill the whole message.

Published to IPC hub — topic "v2n_frame":
  {
    "traffic_flag":    0=no light | 1=GO | 2=STOP
    "transition_flag": 0:G->Y, 1:Y->R, 2:R->Y, 3:Y->G, -1:none
    "state":           "RED" | "GREEN" | "YELLOW" | "OFFLINE"
    "remaining_time":  int
    "is_emergency":    bool
    "warning":         str
    "density":         int
    "nearby_count":    int
    "closest_vehicle": {"plate_id": str|None, "distance_m": float|None}
  }

traffic_flag
  0 -> no light / unknown / OFFLINE
  1 -> GO   (green with enough time to cross, OR emergency vehicle)
  2 -> STOP (red/yellow, OR green but not enough time to cross)
"""

import paho.mqtt.client as mqtt
import ssl
import json
import os
import sys
import threading

# ipc_node lives in the shared hub/ folder
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "hub"))
from ipc_node import IPCNode

# ============================================================
# MQTT — HiveMQ Cloud
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

PROCESSED_TOPIC = "V2X/zone1/traffic/processed"

# ============================================================
# IPC Hub
# ============================================================
ipc = IPCNode("car_display")

# ============================================================
# Shared state
# ============================================================
# ── TEST MODE ────────────────────────────────────────────────
# True  -> if the camera sends no distance / STM32 speed is 0, use the values below.
# False -> real data only (set this when everything is connected).
USE_TEST_VALUES = True
TEST_DISTANCE_M = 20.0
TEST_SPEED_KMH  = 30.0

_state_lock      = threading.Lock()
_speed_kmh       = TEST_SPEED_KMH if USE_TEST_VALUES else 0.0
_distance_m      = None
_remaining_time  = 0
_light_state     = "RED"
_transition_flag = -1
_is_emergency    = False
_warning         = "Normal Traffic"
_density         = 0
_nearby_count    = 0
_closest_vehicle = {"plate_id": None, "distance_m": None}


# ============================================================
# Safe converters (the actual fix)
# ============================================================
def _safe_float(value):
    """float, or None for None / 'NONE' / '' / negative / non-numeric."""
    if value is None:
        return None
    try:
        v = float(value)
    except (TypeError, ValueError):
        return None
    return v if v >= 0 else None


def _safe_int(value, default):
    """int, or `default` for None / 'NONE' / non-numeric. Keeps 0 as 0."""
    if value is None:
        return default
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _safe_plate(value):
    """Normalise '' / 'NONE' placeholder to None."""
    if value is None:
        return None
    s = str(value).strip()
    return None if s.upper() in ("", "NONE") else s


# ============================================================
# Helpers
# ============================================================
def _compute_crossing_flag(distance_m, speed_kmh, remaining_time, light_state):
    """1 = car can cross before light changes, 0 = must stop."""
    if light_state != "GREEN":
        return 0
    if distance_m is None or distance_m <= 0:
        return 0
    if speed_kmh is None or speed_kmh < 1.0:
        return 0
    t_car = distance_m / (speed_kmh / 3.6)
    return 1 if remaining_time >= t_car else 0


def _compute_traffic_flag(state, is_emergency, crossing_flag):
    """0 = no light, 1 = GO, 2 = STOP."""
    if state not in ("RED", "YELLOW", "GREEN"):
        return 0
    if is_emergency:
        return 1
    if state == "GREEN":
        return 1 if crossing_flag == 1 else 2
    return 2


# ============================================================
# IPC callback — vehicle speed from UART bridge (STM32)
# ============================================================
def _on_vehicle_speed(topic, data, sender):
    global _speed_kmh
    with _state_lock:
        real = _safe_float(data.get("speed_kmh")) or 0.0
        if USE_TEST_VALUES and real < 1.0:
            real = TEST_SPEED_KMH
        _speed_kmh = real
    _publish_v2n_frame()


# ============================================================
# IPC publish — merged frame -> hub
# ============================================================
def _publish_v2n_frame():
    with _state_lock:
        dist    = _distance_m
        speed   = _speed_kmh
        rt      = _remaining_time
        state   = _light_state
        tf      = _transition_flag
        emg     = _is_emergency
        warn    = _warning
        dens    = _density
        nearby  = _nearby_count
        closest = dict(_closest_vehicle)

    crossing     = _compute_crossing_flag(dist, speed, rt, state)
    traffic_flag = _compute_traffic_flag(state, emg, crossing)

    frame = {
        "traffic_flag":    traffic_flag,
        "transition_flag": tf,
        "state":           state,
        "remaining_time":  rt,
        "is_emergency":    emg,
        "warning":         warn,
        "density":         dens,
        "nearby_count":    nearby,
        "closest_vehicle": closest,
    }
    ipc.publish("v2n_frame", frame)


# ============================================================
# MQTT callbacks
# ============================================================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("[Car_client] Connected to HiveMQ Cloud.")
        client.subscribe(PROCESSED_TOPIC)
        print(f"[Car_client] Subscribed to: {PROCESSED_TOPIC}")
    else:
        print(f"[Car_client] MQTT connection failed: {reason_code}")


def on_message(client, userdata, msg):
    global _distance_m, _remaining_time, _light_state, _transition_flag
    global _is_emergency, _warning, _density, _nearby_count, _closest_vehicle

    try:
        data = json.loads(msg.payload.decode().strip())

        closest_raw = data.get("closest_vehicle") or {}
        dist_m      = _safe_float(closest_raw.get("distance_m"))
        plate_id    = _safe_plate(closest_raw.get("plate_id"))

        tc = data.get("transition_code", data.get("transition_flag", -1))

        with _state_lock:
            _light_state     = str(data.get("state", "RED")).upper()
            _remaining_time  = _safe_int(data.get("remaining_time"), 0)
            _is_emergency    = bool(data.get("is_emergency", False))
            _transition_flag = _safe_int(tc, -1)
            _warning         = data.get("warning", "Normal Traffic")
            _density         = _safe_int(data.get("density"), 0)
            _nearby_count    = _safe_int(data.get("nearby_count"), 0)
            _distance_m      = dist_m if dist_m is not None else (
                                   TEST_DISTANCE_M if USE_TEST_VALUES else None)
            _closest_vehicle = {"plate_id": plate_id, "distance_m": dist_m}

        _publish_v2n_frame()

        # ── Console display ──────────────────────────────────────
        with _state_lock:
            st, rt, tfv = _light_state, _remaining_time, _transition_flag
            dist, spd, emg = _distance_m, _speed_kmh, _is_emergency
            nb, wn = _nearby_count, _warning

        crossing     = _compute_crossing_flag(dist, spd, rt, st)
        traffic_flag = _compute_traffic_flag(st, emg, crossing)
        tf_lbl   = {0: "G->Y", 1: "Y->R", 2: "R->Y", 3: "Y->G", -1: "--"}.get(tfv, str(tfv))
        flag_lbl = {0: "NO LIGHT", 1: "GO", 2: "STOP"}.get(traffic_flag, str(traffic_flag))

        os.system('cls' if os.name == 'nt' else 'clear')
        print("=" * 60)
        print("  V2X CAR OBU — LIVE FEED")
        print("=" * 60)
        print(f"  Traffic Light  : {st}")
        print(f"  Remaining Time : {rt} s")
        print(f"  Transition     : {tf_lbl}")
        print(f"  Distance (int) : {dist if dist is not None else '--'} m")
        print(f"  Speed          : {spd:.1f} km/h")
        print(f"  Traffic Flag   : {traffic_flag}  ({flag_lbl})")
        print(f"  Emergency      : {'YES' if emg else 'No'}")
        print(f"  Nearby Vehicles: {nb}")
        if emg:
            print(f"  WARNING        : {wn}")
        print("=" * 60)

    except Exception as exc:
        print(f"[Car_client] Error in on_message: {exc!r}")


# ============================================================
# Main
# ============================================================
if __name__ == "__main__":
    print("=" * 60)
    print("  V2X Car Client starting …")
    print("=" * 60)

    print("[Car_client] Connecting to IPC hub …")
    if ipc.connect():
        ipc.subscribe("vehicle_speed", _on_vehicle_speed)
        ipc.start_listening()
        print("[Car_client] IPC hub connected. Listening for vehicle_speed.")
    else:
        print("[Car_client] WARNING: IPC hub unreachable. Speed will stay at default.")

    print("[Car_client] Connecting to HiveMQ Cloud …")
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(USERNAME, PASSWORD)
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(BROKER, PORT, 60)
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[Car_client] Stopped.")
