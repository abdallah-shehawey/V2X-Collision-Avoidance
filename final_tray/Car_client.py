# -*- coding: utf-8 -*-
"""
Car_client.py — V2X Car On-Board Unit (OBU)
=============================================

Role in the system
------------------
This process is the "brain" that lives on the Raspberry Pi inside the car.
It connects to THREE data sources and merges them into a single decision:

  1. HiveMQ Cloud (MQTT) — receives the processed traffic packet from
     Intelligent_Gateway.py.  Contains: traffic-light state, remaining
     time, closest-vehicle distance, is_emergency flag, etc.

  2. IPC Hub (local Unix socket) — receives:
       • "vehicle_speed"      published by uart_bridge (server.py AUTO MODE)
                              which reads speed [km/h] from the STM32.
       • Any other topic any local process publishes in the future.

  3. Derived computation (crossing flag) — every time a new speed OR a new
     gateway packet arrives, computes:
         t_car  = distance_m / speed_ms   (time for car to reach the light)
         RT     = remaining_time          (time until light changes colour)
         crossing_flag:
             1  → RT >= t_car  → car CAN pass before the light changes
             0  → RT <  t_car  → car will NOT make it, must slow down / stop

What is published to the IPC hub
---------------------------------
Topic: "v2n_frame"
Payload (all keys always present):
  {
    "traffic_flag":        int   — 0=GREEN, 1=YELLOW, 2=RED, 3=UNKNOWN
    "ambulance_flag":      int   — 0=no emergency, 1=ambulance detected
    "distance_to_light_m": float|null — metres to the traffic light
    "state":               str   — "RED"|"GREEN"|"YELLOW"
    "remaining_time":      int   — seconds until state changes
    "transition_flag":     int   — see table below
    "crossing_flag":       int   — 0=will NOT make it, 1=CAN pass
  }

transition_flag encoding (set by Traffic_light_GUI.py and forwarded by
the Intelligent Gateway — Car_client just passes it through unchanged):
  0  →  GREEN  → YELLOW
  1  →  YELLOW → RED
  2  →  RED    → YELLOW
  3  →  YELLOW → GREEN
  -1 →  mid-phase (no active transition)

crossing_flag logic
--------------------
Only meaningful when state == "GREEN" (or YELLOW if you want to warn).
  t_car  = distance_m / (speed_kmh / 3.6)
  if remaining_time >= t_car  →  crossing_flag = 1  (safe to cross)
  else                         →  crossing_flag = 0  (must stop)
  Special cases: distance or speed unknown → crossing_flag = 0

data.json fields written by dashboard_bridge.py
------------------------------------------------
  data["v2n"]["trafficLight"]       int   (traffic_flag)
  data["v2n"]["ambulance"]          int   (ambulance_flag)
  data["v2n"]["distanceToLightM"]   float|null
  data["v2n"]["transitionFlag"]     int
  data["v2n"]["crossingFlag"]       int

NOTE: remaining_time and raw state string are NOT written to data.json.
      They are used internally for the crossing_flag calculation only.

Startup order
-------------
  1. hub.py          (IPC broker)
  2. server.py       (optional: web dashboard)
  3. Car_client.py   (this file)
  4. uart_bridge     (server.py AUTO MODE — publishes vehicle_speed)
"""

import paho.mqtt.client as mqtt
import ssl
import json
import threading

from ipc_node import IPCNode

# ============================================================
# MQTT — HiveMQ Cloud (same broker as the Gateway)
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
# Shared state (updated from two independent threads: MQTT + IPC)
# ============================================================
_state_lock      = threading.Lock()

_speed_kmh       = 0.0    # updated by IPC callback (vehicle_speed topic)
_distance_m      = None   # updated by MQTT callback (closest_vehicle.distance_m)
_remaining_time  = 0      # updated by MQTT callback
_light_state     = "RED"  # updated by MQTT callback
_transition_flag = -1     # updated by MQTT callback (passed from Gateway)
_is_emergency    = False  # updated by MQTT callback


# ============================================================
# Crossing flag computation
# ============================================================
def _compute_crossing_flag(distance_m, speed_kmh, remaining_time, light_state):
    """
    Compute whether the car can safely cross the intersection before
    the traffic light changes.

    Returns
    -------
    1 — remaining time is enough for the car to reach the light
    0 — car will not make it (must slow down / stop)

    The flag is only set to 1 when light is GREEN.
    If distance or speed are unknown the flag defaults to 0 (safe/stop).
    """
    # Only attempt to cross on GREEN; on YELLOW/RED always flag 0.
    if light_state != "GREEN":
        return 0

    # Can't compute without both values.
    if distance_m is None or distance_m <= 0:
        return 0
    if speed_kmh is None or speed_kmh < 1.0:   # < 1 km/h → essentially stopped
        return 0

    speed_ms = speed_kmh / 3.6                 # convert km/h → m/s
    t_car    = distance_m / speed_ms           # seconds for car to reach light

    return 1 if remaining_time >= t_car else 0


# ============================================================
# IPC callback — vehicle speed from UART bridge
# ============================================================
def _on_vehicle_speed(topic, data, sender):
    """
    Receive speed published by uart_bridge (reads STM32 over UART).

    Expected payload:
        {"speed_kmh": <float>}

    Updates the shared _speed_kmh; then recomputes and republishes
    the v2n_frame so the dashboard always has a fresh crossing_flag.
    """
    global _speed_kmh
    with _state_lock:
        _speed_kmh = float(data.get("speed_kmh", 0.0))

    _publish_v2n_frame()


# ============================================================
# IPC publish — send merged frame to hub (→ dashboard_bridge)
# ============================================================
def _publish_v2n_frame():
    """Build and publish the v2n_frame to the IPC hub."""
    with _state_lock:
        dist     = _distance_m
        speed    = _speed_kmh
        rt       = _remaining_time
        state    = _light_state
        tf       = _transition_flag
        emg      = _is_emergency

    crossing = _compute_crossing_flag(dist, speed, rt, state)

    # traffic_flag encoding: 0=GREEN, 1=YELLOW, 2=RED, 3=UNKNOWN
    flag_map = {"GREEN": 0, "YELLOW": 1, "RED": 2}
    traffic_flag = flag_map.get(state, 3)

    frame = {
        "traffic_flag":        traffic_flag,
        "ambulance_flag":      1 if emg else 0,
        "distance_to_light_m": dist,
        "state":               state,
        "remaining_time":      rt,
        "transition_flag":     tf,
        "crossing_flag":       crossing,
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
    """
    Handle a processed traffic packet from the Intelligent Gateway.

    Reads:
      state            — "RED" | "GREEN" | "YELLOW"
      remaining_time   — int seconds
      is_emergency     — bool
      transition_code  — int (forwarded as transition_flag)
      closest_vehicle.distance_m — float metres
    """
    global _distance_m, _remaining_time, _light_state, _transition_flag, _is_emergency

    try:
        data = json.loads(msg.payload.decode().strip())

        closest  = data.get("closest_vehicle") or {}
        dist_m   = closest.get("distance_m")

        with _state_lock:
            _light_state     = data.get("state", "RED")
            _remaining_time  = int(data.get("remaining_time", 0))
            _is_emergency    = bool(data.get("is_emergency", False))
            _transition_flag = int(data.get("transition_code", -1))
            _distance_m      = float(dist_m) if dist_m is not None else None

        _publish_v2n_frame()

        # ── Console display ──────────────────────────────────────────
        import os
        os.system('cls' if os.name == 'nt' else 'clear')
        print("=" * 60)
        print("  V2X CAR OBU — LIVE FEED")
        print("=" * 60)
        print(f"  Traffic Light  : {_light_state}")
        print(f"  Remaining Time : {_remaining_time} s")
        print(f"  Distance       : {_distance_m} m")
        print(f"  Speed (UART)   : {_speed_kmh:.1f} km/h")
        crossing = _compute_crossing_flag(_distance_m, _speed_kmh,
                                          _remaining_time, _light_state)
        print(f"  Crossing Flag  : {crossing}  "
              f"({'CAN PASS' if crossing else 'MUST STOP'})")
        print(f"  Transition     : {_transition_flag}")
        print(f"  Emergency      : {'YES ⚠️' if _is_emergency else 'No'}")
        if _is_emergency:
            print(f"  WARNING        : {data.get('warning','')}")
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

    # 1. Connect to local IPC hub first
    print("[Car_client] Connecting to IPC hub …")
    if ipc.connect():
        ipc.subscribe("vehicle_speed", _on_vehicle_speed)
        ipc.start_listening()
        print("[Car_client] IPC hub connected. Listening for vehicle_speed.")
    else:
        print("[Car_client] WARNING: IPC hub unreachable. Speed will be 0.")

    # 2. Connect to HiveMQ Cloud
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
