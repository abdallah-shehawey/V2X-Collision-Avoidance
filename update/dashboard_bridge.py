"""
dashboard_bridge.py - Hub → data.json bridge for the V2X Dashboard
=====================================================================
Listens to IPC hub topics and mirrors their data into data.json
so the web dashboard picks it up via polling.

Topics subscribed
-----------------
    "v2n_frame"        — published by Car_client-1.py
    "v2p_frame"        — published by V2P.py
    "motorcycle_alert" — published by V2P.py

data.json shape after this bridge writes it
-------------------------------------------
    "v2n": {
        "trafficLight":       0|1|2,
        "ambulance":          0|1,
        "distanceToLightM":   <float|null>,
        "state":              "RED"|"GREEN"|"YELLOW",
        "remainingTime":      <int>,
        "transitionFlag":     0|1|2|3|-1,
        "crossingFlag":       0|1
    },
    "v2p": {
        "pedestrian":         0|1|2,
        "position":           0|1|2|3|null,
        "motorcycleCollision": 0|1
    }

crossingFlag reference
----------------------
    1 = car will reach the light before it changes
    0 = car won't make it, or light is not green

transitionFlag reference
------------------------
    0  GREEN  → YELLOW
    1  YELLOW → RED
    2  RED    → YELLOW
    3  YELLOW → GREEN
   -1  mid-phase

motorcycleCollision reference
------------------------------
    0 = no risk
    1 = motorcycle in crossing zone + DANGER proximity + HIGH intent
"""

import json
import os
import threading
import time

from ipc_node import IPCNode

# ─────────────────────────────────────────────────────────────
# Configuration
# ─────────────────────────────────────────────────────────────
NODE_NAME = "dashboard_bridge"

HERE      = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(HERE, "data.json")

TOPIC_V2N_FRAME       = "v2n_frame"
TOPIC_V2P_FRAME       = "v2p_frame"
TOPIC_MOTO_ALERT      = "motorcycle_alert"

_file_lock = threading.Lock()


# ─────────────────────────────────────────────────────────────
# data.json helpers
# ─────────────────────────────────────────────────────────────
def _load_data() -> dict:
    try:
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _save_data(data: dict) -> None:
    tmp = DATA_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp, DATA_FILE)


def _update_data(mutate) -> None:
    with _file_lock:
        data = _load_data()
        mutate(data)
        _save_data(data)


# ─────────────────────────────────────────────────────────────
# Hub callbacks
# ─────────────────────────────────────────────────────────────
def _on_v2n_frame(topic: str, payload: dict, sender: str) -> None:
    """Handle v2n_frame from Car_client-1.py → writes data['v2n']."""
    try:
        traffic_flag    = payload.get("traffic_flag",        0)
        ambulance_flag  = payload.get("ambulance_flag",      0)
        distance_m      = payload.get("distance_to_light_m")
        state           = payload.get("state",               "RED")
        remaining_time  = payload.get("remaining_time",      0)
        transition_flag = payload.get("transition_flag",     -1)
        crossing_flag   = payload.get("crossing_flag",       0)

        def mutate(data: dict) -> None:
            data["v2n"] = {
                "trafficLight":       traffic_flag,
                "ambulance":          ambulance_flag,
                "distanceToLightM":   distance_m,
                "state":              state,
                "remainingTime":      remaining_time,
                "transitionFlag":     transition_flag,
                "crossingFlag":       crossing_flag,
            }

        _update_data(mutate)

        tf_labels = {0:"G→Y", 1:"Y→R", 2:"R→Y", 3:"Y→G", -1:"--"}
        print(
            f"[{NODE_NAME}] v2n updated | "
            f"light={traffic_flag} amb={ambulance_flag} "
            f"tf={tf_labels.get(transition_flag,'?')} "
            f"cross={crossing_flag} (from {sender})"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] error on '{topic}': {exc}")


def _on_v2p_frame(topic: str, payload: dict, sender: str) -> None:
    """Handle v2p_frame from V2P.py → writes data['v2p']['pedestrian'] and ['position']."""
    try:
        pedestrian_flag = payload.get("pedestrian_flag", 0)
        position_flag   = payload.get("position_flag")

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["pedestrian"] = pedestrian_flag
            v2p["position"]   = position_flag

        _update_data(mutate)
        print(
            f"[{NODE_NAME}] v2p updated | "
            f"ped={pedestrian_flag} pos={position_flag} (from {sender})"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] error on '{topic}': {exc}")


def _on_motorcycle_alert(topic: str, payload: dict, sender: str) -> None:
    """Handle motorcycle_alert from V2P.py → writes data['v2p']['motorcycleCollision']."""
    try:
        moto_flag = payload.get("motorcycle_collision_flag", 0)

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["motorcycleCollision"] = moto_flag

        _update_data(mutate)
        if moto_flag:
            print(f"[{NODE_NAME}] MOTORCYCLE COLLISION FLAG = 1 (from {sender})")
        else:
            print(f"[{NODE_NAME}] moto_collision=0 (from {sender})")

    except Exception as exc:
        print(f"[{NODE_NAME}] error on '{topic}': {exc}")


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────
def main() -> None:
    print("=" * 55)
    print("  V2X Dashboard Bridge — hub frames → data.json")
    print("=" * 55)

    node = IPCNode(NODE_NAME)
    if not node.connect():
        print(f"[{NODE_NAME}] ERROR: cannot reach hub — start hub.py first.")
        return

    node.subscribe(TOPIC_V2N_FRAME,  _on_v2n_frame)
    node.subscribe(TOPIC_V2P_FRAME,  _on_v2p_frame)
    node.subscribe(TOPIC_MOTO_ALERT, _on_motorcycle_alert)
    node.start_listening()

    print(f"[{NODE_NAME}] listening — data.json updates automatically.\n")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\n[{NODE_NAME}] stopped.")


if __name__ == "__main__":
    main()
