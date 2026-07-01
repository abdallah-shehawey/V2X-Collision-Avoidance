# -*- coding: utf-8 -*-
"""
Car_client.py — V2X Car On-Board Unit (OBU)
=============================================

Role in the system
------------------
Connects to TWO data sources and merges them:

  1. HiveMQ Cloud (MQTT) — processed traffic packet from Intelligent_Gateway.
  2. IPC Hub (local) — vehicle_speed topic from uart_bridge (STM32 over UART).

What is published to the IPC hub
---------------------------------
Topic: "v2n_frame"
Payload (all keys always present):
  {
    "traffic_flag":        int   — 0=no light, 1=GO, 2=STOP (see encoding below)
    "transition_flag":     int   — 0:G→Y, 1:Y→R, 2:R→Y, 3:Y→G, -1:mid-phase
    "state":               str   — raw "RED"|"GREEN"|"YELLOW"
    "remaining_time":      int   — seconds until state changes
    "is_emergency":        bool
    "warning":             str
    "density":             int
    "nearby_count":        int
    "closest_vehicle": {
        "plate_id":   str|null,
        "distance_m": float|null
    }
  }

NOTE: distance_to_light_m is NOT part of the published frame anymore.
It is still computed internally (from the closest_vehicle data) purely
to work out whether the car can cross before the light changes, but it
never leaves this file — dashboard_bridge / data.json no longer see it.

traffic_flag — single unified flag, ordered from least to most dangerous
--------------------------------------------------------------------------
  0 → no light / unknown        (no traffic light detected — normal driving)
  1 → GO                        (green AND there's enough time to cross,
                                  OR an ambulance/emergency vehicle — the
                                  light will change for it, so it's treated
                                  just like a normal car that can go)
  2 → STOP                      (red or yellow, OR green but NOT enough
                                  time left to cross before it changes)

  The old separate "ambulance_flag" and "crossing_flag" fields have been
  folded into this single traffic_flag:
    - is_emergency == True  →  always resolves to GO (1)
    - otherwise, GREEN is only GO (1) if the crossing-time check passes;
      it becomes STOP (2) if the car would not make it in time.

  crossing-time check (used only to help resolve traffic_flag):
      t_car  = distance_m / (speed_kmh / 3.6)
      if remaining_time >= t_car  →  can cross  →  GO
      else                         →  can't make it → STOP

transition_flag encoding (forwarded unchanged from trafic_light.py)
  0  →  GREEN  → YELLOW
  1  →  YELLOW → RED
  2  →  RED    → YELLOW
  3  →  YELLOW → GREEN
  -1 →  mid-phase
"""

import paho.mqtt.client as mqtt
import ssl
import json
import threading

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
_state_lock      = threading.Lock()

_speed_kmh       = 0.0
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
    speed_ms = speed_kmh / 3.6
    t_car    = distance_m / speed_ms
    return 1 if remaining_time >= t_car else 0


def _compute_traffic_flag(state: str, is_emergency: bool, crossing_flag: int) -> int:
    """
    Unified dashboard flag — least to most dangerous:
      0 = no light / unknown
      1 = GO    (green with enough time to cross, OR an ambulance/emergency
                 vehicle — the light will change for it, so treat it like a
                 normal car that's allowed to go)
      2 = STOP  (red/yellow, OR green but not enough time left to cross)
    """
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
    """Receive speed from uart_bridge; recompute and republish v2n_frame."""
    global _speed_kmh
    with _state_lock:
        _speed_kmh = float(data.get("speed_kmh", 0.0))
    _publish_v2n_frame()


# ============================================================
# IPC publish — full merged frame → hub → dashboard_bridge
# ============================================================
def _publish_v2n_frame():
    """Build and publish v2n_frame with ALL data the hub needs."""
    with _state_lock:
        dist      = _distance_m
        speed     = _speed_kmh
        rt        = _remaining_time
        state     = _light_state
        tf        = _transition_flag
        emg       = _is_emergency
        warn      = _warning
        dens      = _density
        nearby    = _nearby_count
        closest   = dict(_closest_vehicle)

    crossing     = _compute_crossing_flag(dist, speed, rt, state)
    traffic_flag = _compute_traffic_flag(state, emg, crossing)

    frame = {
        # ── Dashboard flag (unified encoding per spec) ───────────
        "traffic_flag":        traffic_flag,
        "transition_flag":     tf,
        # ── Raw data (passed through for V2P and other subscribers) ──
        # NOTE: distance_to_light_m stays internal only — it's used above
        # purely to resolve traffic_flag, and is never published.
        "state":               state,
        "remaining_time":      rt,
        "is_emergency":        emg,
        "warning":             warn,
        "density":             dens,
        "nearby_count":        nearby,
        "closest_vehicle":     closest,
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
        dist_m      = closest_raw.get("distance_m")

        with _state_lock:
            _light_state     = data.get("state", "RED")
            _remaining_time  = int(data.get("remaining_time", 0))
            _is_emergency    = bool(data.get("is_emergency", False))
            _transition_flag = int(data.get("transition_code",
                                   data.get("transition_flag", -1)))
            _warning         = data.get("warning", "Normal Traffic")
            _density         = int(data.get("density", 0))
            _nearby_count    = int(data.get("nearby_count", 0))
            _distance_m      = float(dist_m) if dist_m is not None else None
            _closest_vehicle = {
                "plate_id":   closest_raw.get("plate_id"),
                "distance_m": _distance_m,
            }

        _publish_v2n_frame()

        # ── Console display ──────────────────────────────────────────
        import os
        os.system('cls' if os.name == 'nt' else 'clear')
        crossing     = _compute_crossing_flag(_distance_m, _speed_kmh,
                                              _remaining_time, _light_state)
        traffic_flag = _compute_traffic_flag(_light_state, _is_emergency, crossing)
        tf_lbl = {0:"G→Y", 1:"Y→R", 2:"R→Y", 3:"Y→G", -1:"--"}.get(
                    _transition_flag, str(_transition_flag))
        flag_lbl = {0:"NO LIGHT", 1:"GO", 2:"STOP"}.get(traffic_flag, str(traffic_flag))
        print("=" * 60)
        print("  V2X CAR OBU — LIVE FEED")
        print("=" * 60)
        print(f"  Traffic Light  : {_light_state}")
        print(f"  Remaining Time : {_remaining_time} s")
        print(f"  Transition     : {tf_lbl}")
        print(f"  Distance (int) : {_distance_m} m  (used internally only)")
        print(f"  Speed (UART)   : {_speed_kmh:.1f} km/h")
        print(f"  Traffic Flag   : {traffic_flag}  ({flag_lbl})")
        print(f"  Emergency      : {'YES ⚠️' if _is_emergency else 'No'}")
        print(f"  Nearby Vehicles: {_nearby_count}")
        if _is_emergency:
            print(f"  WARNING        : {_warning}")
        print("=" * 60)

    except Exception as exc:
        print(f"[Car_client] Error in on_message: {exc}")


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
        print("[Car_client] WARNING: IPC hub unreachable. Speed will be 0.")

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
