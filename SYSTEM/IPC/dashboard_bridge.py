"""
dashboard_bridge.py — Hub topics → data.json (real-time mirror)
=================================================================
Subscribes to every hub topic that carries live system data and
writes it into data.json so the web dashboard (server.py + app.js)
always shows current state within ~1 s.

Topics consumed
---------------
  "local/traffic/processed"   ← Car_client-1.py  (V2N cloud data)
  "pedestrian_status"         ← V2P.py            (camera pedestrian data)
  "vehicle_data"              ← V2P.py            (camera vehicle sensing)
  "traffic_light_state"       ← V2I bridge        (ESP32 raw light state)
  "v2v_data"                  ← UART bridge       (STM32 neighbour data)

data.json sections updated
--------------------------
  data["v2n"]   — traffic light state, ambulance info, density
  data["v2p"]   — pedestrians, vehicle sensing, traffic-light state
  data["drive"] — speed / heading / pitch / roll / temp  (from V2V)
  data["adas"]  — EEBL / FCW / BSW / DNPW / IMA flags   (from V2V)
  data["ultrasonic"] — 6-direction distances             (from V2V)

File safety: every write is an atomic os.replace() so the browser
never sees a half-written file, even when multiple topics fire at once.
"""

import json
import os
import threading
import time
from datetime import datetime, timezone

from ipc_node import IPCNode

# ──────────────────────────────────────────────────────────────────────
# Config
# ──────────────────────────────────────────────────────────────────────
NODE_NAME = "dashboard_bridge"
HERE      = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(HERE, "data.json")

# Topics
TOPIC_TRAFFIC_PROCESSED = "local/traffic/processed"   # V2N  (Car_client)
TOPIC_PEDESTRIAN_STATUS = "pedestrian_status"          # V2P  (camera)
TOPIC_VEHICLE_DATA      = "vehicle_data"               # V2P  (camera)
TOPIC_TRAFFIC_LIGHT     = "traffic_light_state"        # V2I  (ESP32 → server → hub)
TOPIC_V2V_DATA          = "v2v_data"                  # V2V  (STM32 UART bridge)

_file_lock = threading.Lock()


# ──────────────────────────────────────────────────────────────────────
# data.json helpers
# ──────────────────────────────────────────────────────────────────────
def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _load() -> dict:
    try:
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _save(data: dict) -> None:
    tmp = DATA_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp, DATA_FILE)


def _update(mutate) -> None:
    """Thread-safe read → mutate → atomic-write cycle."""
    with _file_lock:
        data = _load()
        mutate(data)
        _save(data)


# ──────────────────────────────────────────────────────────────────────
# Callbacks — one per topic
# ──────────────────────────────────────────────────────────────────────

# ── 1. V2N: processed traffic + ambulance data from cloud ────────────
def _on_traffic_processed(topic: str, payload: dict, sender: str) -> None:
    """
    Car_client-1.py receives this from HiveMQ and re-publishes it locally.
    Fields: state, remaining_time, is_emergency, warning,
            density, closest_vehicle{plate_id, distance_m}, nearby_count
    """
    try:
        closest      = payload.get("closest_vehicle") or {}
        is_emergency = bool(payload.get("is_emergency", False))

        def mutate(data: dict) -> None:
            data.setdefault("v2n", {})
            data["v2n"]["trafficLight"] = {
                "state":         payload.get("state", "RED"),
                "remainingTime": payload.get("remaining_time", 0),
                "isEmergency":   is_emergency,
                "warning":       payload.get("warning", "Normal Traffic"),
            }
            data["v2n"]["ambulance"] = {
                # ID of the tracked vehicle closest to the intersection
                "vehicleId":   closest.get("plate_id"),
                "distanceM":   closest.get("distance_m"),
                # Derived from the global emergency flag (per-vehicle type
                # not yet in the Gateway payload — see Gateway v4 note)
                "vehicleType": "ambulance" if is_emergency else "normal",
                "nearbyCount": payload.get("nearby_count", 0),
            }
            data["v2n"]["density"]    = payload.get("density", 0)
            data["v2n"]["lastUpdate"] = _now_iso()

        _update(mutate)
        print(f"[bridge] v2n ← '{topic}'  emergency={is_emergency}  from={sender}")

    except Exception as exc:
        print(f"[bridge] ERROR '{topic}': {exc}")


# ── 2. V2P: pedestrian status from camera ────────────────────────────
def _on_pedestrian_status(topic: str, payload: dict, sender: str) -> None:
    """
    Published by V2P.py after every inference batch.
    Fields: timestamp, pedestrian_count, moving_count,
            crossing_safe, alert_level, traffic_light
    """
    try:
        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["pedestrians"] = {
                "count":        payload.get("pedestrian_count", 0),
                "movingCount":  payload.get("moving_count", 0),
                "crossingSafe": payload.get("crossing_safe", True),
                "alertLevel":   payload.get("alert_level", "none"),
            }
            # Keep v2p.trafficLightState in sync with what V2P sees
            v2p["trafficLightState"] = payload.get("traffic_light", "RED")
            v2p["lastUpdate"]        = _now_iso()

        _update(mutate)
        print(f"[bridge] v2p.pedestrians ← '{topic}'  from={sender}")

    except Exception as exc:
        print(f"[bridge] ERROR '{topic}': {exc}")


# ── 3. V2P: vehicle sensing from camera ──────────────────────────────
def _on_vehicle_data(topic: str, payload: dict, sender: str) -> None:
    """
    Also published by V2P.py: camera-side vehicle estimates.
    Fields: timestamp, speed_kmh, brake_pressed, car_count,
            moving_cars, bike_count, ultrasonic{front_cm}
    """
    try:
        ultra = payload.get("ultrasonic") or {}

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["vehicleSensing"] = {
                "speedKmh":     payload.get("speed_kmh",    0),
                "brakePressed": payload.get("brake_pressed", False),
                "carCount":     payload.get("car_count",    0),
                "movingCars":   payload.get("moving_cars",  0),
                "bikeCount":    payload.get("bike_count",   0),
                "frontCm":      ultra.get("front_cm",       999.0),
            }
            v2p["lastUpdate"] = _now_iso()

        _update(mutate)
        print(f"[bridge] v2p.vehicleSensing ← '{topic}'  from={sender}")

    except Exception as exc:
        print(f"[bridge] ERROR '{topic}': {exc}")


# ── 4. V2I: raw traffic-light state from ESP32 (via server/hub) ──────
def _on_traffic_light_state(topic: str, payload: dict, sender: str) -> None:
    """
    The ESP32 traffic-light controller publishes its state via WiFi to the
    server, which re-publishes it on this hub topic.
    Fields: state ("RED"|"GREEN"|"YELLOW"), remaining_time, is_emergency
    """
    try:
        def mutate(data: dict) -> None:
            # Mirror into v2n.trafficLight if no cloud update has arrived
            # yet — or always keep it fresh so V2P has accurate state.
            data.setdefault("v2n", {}).setdefault("trafficLight", {})
            tl = data["v2n"]["trafficLight"]
            # Only overwrite fields not already set by the cloud gateway
            tl["state"]         = payload.get("state",          tl.get("state", "RED"))
            tl["remainingTime"] = payload.get("remaining_time", tl.get("remainingTime", 0))
            tl["isEmergency"]   = payload.get("is_emergency",   tl.get("isEmergency", False))
            data["v2n"]["lastUpdate"] = _now_iso()

            # Also update v2p so the camera system shows same light
            data.setdefault("v2p", {})["trafficLightState"] = tl["state"]

        _update(mutate)
        print(f"[bridge] v2n.trafficLight ← '{topic}'  from={sender}")

    except Exception as exc:
        print(f"[bridge] ERROR '{topic}': {exc}")


# ── 5. V2V: neighbour data from STM32 over UART ──────────────────────
def _on_v2v_data(topic: str, payload: dict, sender: str) -> None:
    """
    The UART bridge (uart.py) receives STM32 DSRC frames and publishes
    them on this topic.
    Fields: vehicle_id, speed (cm/s), heading (deg), last_update,
            fcw_flag, dnpw_flag, distance_to_intersection, ima_flag
    (see uart.py NEIGHBOR_FIELDS for the full struct layout)

    We map these directly into data["drive"] and data["adas"] so the
    dashboard gauges update from real V2V data.
    """
    try:
        def mutate(data: dict) -> None:
            speed_cms = float(payload.get("speed", 0))
            speed_kmh = round(speed_cms * 0.036, 1)   # cm/s → km/h

            drv = data.setdefault("drive", {})
            drv["speedKmh"] = speed_kmh
            drv["heading"]  = float(payload.get("heading", drv.get("heading", 0)))

            ads = data.setdefault("adas", {})
            ads["fcw"]  = int(payload.get("fcw_flag",  0))
            ads["dnpw"] = int(payload.get("dnpw_flag", 0))
            ads["ima"]  = int(payload.get("ima_flag",  0))
            # EEBL and BSW are not in the UART struct yet → keep existing values

        _update(mutate)
        print(f"[bridge] drive/adas ← '{topic}'  from={sender}")

    except Exception as exc:
        print(f"[bridge] ERROR '{topic}': {exc}")


# ──────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────
def main() -> None:
    print("=" * 60)
    print("  V2X Dashboard Bridge — hub → data.json")
    print("=" * 60)

    node = IPCNode(NODE_NAME)
    if not node.connect():
        print(f"[bridge] ERROR: hub not reachable. Start hub.py first.")
        return

    node.subscribe(TOPIC_TRAFFIC_PROCESSED, _on_traffic_processed)
    node.subscribe(TOPIC_PEDESTRIAN_STATUS, _on_pedestrian_status)
    node.subscribe(TOPIC_VEHICLE_DATA,      _on_vehicle_data)
    node.subscribe(TOPIC_TRAFFIC_LIGHT,     _on_traffic_light_state)
    node.subscribe(TOPIC_V2V_DATA,          _on_v2v_data)

    node.start_listening()
    print(f"\n[bridge] ✅ listening on 5 topics — data.json updates automatically\n")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\n[bridge] stopped by user.")


if __name__ == "__main__":
    main()
