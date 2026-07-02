# -*- coding: utf-8 -*-
"""
dashboard_bridge.py — IPC Hub → data.json Bridge
==================================================

Role in the system
------------------
The ONLY writer to data.json.  Subscribes to three IPC hub topics and
writes the exact fields the web dashboard reads — atomically (tmp → rename).

Topics consumed
---------------
    "v2n_frame"        — published by Car_client.py
    "v2p_frame"        — published by V2P.py  (every inference cycle)
    "motorcycle_alert" — published by V2P.py  (only on state change)

data.json fields written (matching the dashboard spec in the screenshots)
--------------------------------------------------------------------------
"v2n": {
    "trafficLight":   int   — unified flag, least → most dangerous:
                              0 = no light / unknown
                              1 = GO   (green with enough time to cross,
                                        or an ambulance/emergency vehicle)
                              2 = STOP (red/yellow, or green but not
                                        enough time left to cross)
}
"v2p": {
    "pedestrian":          int  — 0=safe, 1=standing near, 2=crossing(warning)
    "position":            int  — 0=no one, 1=RIGHT, 2=LEFT
    "motorcycleCollision":  int  — 0=no risk, 1=collision risk
    "leadCarCollision":     int  — 0=normal, 1=WARNING (stopped wrong,
                                   moderate distance), 2=DANGER (stopped
                                   wrong, too close)
}

NOTE: "ambulance", "crossingFlag" and "distanceToLightM" are no longer
written — they've been folded into the single "trafficLight" flag above
(Car_client.py resolves that logic before publishing v2n_frame).
"transitionFlag" is also no longer written — "trafficLight" already
carries everything the dashboard needs, so the extra field was redundant.

Plus all existing data.json keys (drive, adas, ultrasonic, weather, meta)
are left untouched — this bridge only updates v2n and v2p sections.

Startup order
-------------
    1. hub.py              (IPC broker — must be first)
    2. dashboard_bridge.py (this file)
    3. Car_client.py       (publishes v2n_frame)
    4. V2P.py              (publishes v2p_frame + motorcycle_alert)
    5. server.py           (web server)
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
# data.json lives in the DashBoard folder (served by DashBoard/server.py)
DATA_FILE = os.path.abspath(os.path.join(HERE, "..", "DashBoard", "data.json"))

TOPIC_V2N_FRAME  = "v2n_frame"
TOPIC_V2P_FRAME  = "v2p_frame"
TOPIC_MOTO_ALERT = "motorcycle_alert"

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
    """Atomic write: tmp file → os.replace → real file."""
    tmp = DATA_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp, DATA_FILE)


def _update_data(mutate) -> None:
    """Thread-safe read → mutate → write."""
    with _file_lock:
        data = _load_data()
        mutate(data)
        _save_data(data)


# ─────────────────────────────────────────────────────────────
# v2p.position mapping
# ─────────────────────────────────────────────────────────────
def _map_position(raw_position) -> int:
    """
    V2P.py publishes position_flag as:
        0 = no one in a critical zone
        1 = RIGHT zone (384–512 px)
        2 = LEFT  zone (128–256 px)

    This already matches the dashboard spec exactly, so it is a direct
    pass-through — kept as a function so the mapping lives in one
    place if it ever needs to change again.
    """
    if raw_position in (0, 1, 2):
        return raw_position
    return 0   # anything unexpected → treat as "no one"


# ─────────────────────────────────────────────────────────────
# IPC hub callbacks
# ─────────────────────────────────────────────────────────────
def _on_v2n_frame(topic: str, payload: dict, sender: str) -> None:
    """
    Handle v2n_frame from Car_client.py.

    Writes data["v2n"]["trafficLight"] only. Raw state / remaining_time /
    warning / transition_flag / etc. are NOT written to data.json — they
    are Car_client internal values (transition_flag was dropped entirely
    since trafficLight already conveys everything the dashboard needs).

    Payload fields consumed
    -----------------------
    traffic_flag : int — unified flag: 0=no light, 1=GO, 2=STOP
                         (ambulance + crossing-time logic already
                         folded in by Car_client.py)
    """
    try:
        traffic_flag = int(payload.get("traffic_flag", 0))

        def mutate(data: dict) -> None:
            data["v2n"] = {
                "trafficLight": traffic_flag,
            }

        _update_data(mutate)

        flag_lbl = {0:"NO LIGHT", 1:"GO", 2:"STOP"}.get(traffic_flag, str(traffic_flag))
        print(
            f"[{NODE_NAME}] v2n ← {sender} | "
            f"light={traffic_flag} ({flag_lbl})"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] ERROR on '{topic}': {exc}")


def _on_v2p_frame(topic: str, payload: dict, sender: str) -> None:
    """
    Handle v2p_frame from V2P.py.

    Writes data["v2p"]["pedestrian"], data["v2p"]["position"], and
    data["v2p"]["leadCarCollision"].

    pedestrian_flag encoding (dashboard spec from screenshot):
        0 = آمن       (safe — no pedestrian in zone)
        1 = واقفين جنب (standing near crossing)
        2 = بيعدوا    (actively crossing — WARNING)

    position mapping: see _map_position() above (0=no one, 1=RIGHT, 2=LEFT).

    lead_car_collision_flag:
        0 = normal
        1 = WARNING — lead car stopped wrong (light says GO), moderate
            distance
        2 = DANGER  — lead car stopped wrong, too close (real risk)
    """
    try:
        pedestrian_flag = int(payload.get("pedestrian_flag", 0))
        raw_pos         = payload.get("position_flag", 0)      # 0, 1, or 2
        position        = _map_position(raw_pos)
        lead_car_flag   = int(payload.get("lead_car_collision_flag", 0))

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["pedestrian"]      = pedestrian_flag
            v2p["position"]        = position
            v2p["leadCarCollision"] = lead_car_flag

        _update_data(mutate)
        print(
            f"[{NODE_NAME}] v2p ← {sender} | "
            f"ped={pedestrian_flag} pos={position} leadCar={lead_car_flag}"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] ERROR on '{topic}': {exc}")


def _on_motorcycle_alert(topic: str, payload: dict, sender: str) -> None:
    """
    Handle motorcycle_alert from V2P.py.

    motorcycleCollision:
        0 = no risk
        1 = motorcycle in crossing zone + DANGER proximity + HIGH intent
    """
    try:
        moto_flag = int(payload.get("motorcycle_collision_flag", 0))

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["motorcycleCollision"] = moto_flag

        _update_data(mutate)

        if moto_flag:
            print(f"[{NODE_NAME}] ⚠ MOTORCYCLE COLLISION RISK (from {sender})")
        else:
            print(f"[{NODE_NAME}] motorcycle risk cleared (from {sender})")

    except Exception as exc:
        print(f"[{NODE_NAME}] ERROR on '{topic}': {exc}")


# ─────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────
def main() -> None:
    print("=" * 60)
    print("  V2X Dashboard Bridge — hub frames → data.json")
    print("=" * 60)
    print(f"  Writing to: {DATA_FILE}")
    print()
    print("  v2n.trafficLight   : 0=no light | 1=GO | 2=STOP (unified,")
    print("                       ambulance + crossing-time already folded in)")
    print("  v2p.pedestrian     : 0=safe | 1=near | 2=crossing")
    print("  v2p.position       : 0=no one | 1=RIGHT | 2=LEFT")
    print("  v2p.motorcycleCollision: 0=ok | 1=risk")
    print("  v2p.leadCarCollision   : 0=ok | 1=WARNING | 2=DANGER (stopped wrong)")
    print("=" * 60)

    node = IPCNode(NODE_NAME)
    if not node.connect():
        print(f"[{NODE_NAME}] ERROR: cannot connect to hub — start hub.py first.")
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
