# -*- coding: utf-8 -*-
"""
dashboard_bridge.py — IPC Hub → data.json Bridge
==================================================

Role in the system
------------------
This process is the only writer to data.json.  It subscribes to three IPC hub
topics, extracts the fields the web dashboard needs, and writes them atomically
to data.json (via a .tmp → rename so the dashboard never reads a half-written
file).

Topics consumed (all published to the local IPC hub)
-----------------------------------------------------
    "v2n_frame"        — published by Car_client.py every time a new
                         processed packet arrives from the Gateway or a new
                         speed reading arrives from uart_bridge.

    "v2p_frame"        — published by V2P.py on every inference cycle.

    "motorcycle_alert" — published by V2P.py when a motorcycle enters the
                         crossing zone with DANGER proximity + HIGH intent.

data.json shape written by this bridge
---------------------------------------
The file contains ONLY the fields the web dashboard reads.
Intentionally EXCLUDED: state (raw string), remaining_time.
Those are internal values used by Car_client for crossing_flag computation;
exposing them to the dashboard would cause a second data.json to be created
with extra fields when those fields are written separately.

    "v2n": {
        "trafficLight":       int   — 0=GREEN, 1=YELLOW, 2=RED, 3=UNKNOWN
        "ambulance":          int   — 0=no emergency, 1=ambulance detected
        "distanceToLightM":   float | null  — metres to the nearest vehicle
        "transitionFlag":     int   — see table below
        "crossingFlag":       int   — 0=must stop, 1=can pass
    }
    "v2p": {
        "pedestrian":          int  — 0=clear, 1=detected, 2=crossing
        "position":            int | null — see position_flag table below
        "motorcycleCollision": int  — 0=no risk, 1=high-risk collision alert
    }

transitionFlag reference (computed by trafic_light.py, forwarded via Gateway
and Car_client — this bridge just passes it through unchanged)
    0   GREEN  → YELLOW  (green ending soon)
    1   YELLOW → RED     (yellow ending soon)
    2   RED    → YELLOW  (red ending soon)
    3   YELLOW → GREEN   (yellow ending soon, green coming)
   -1   mid-phase (no imminent transition)

crossingFlag reference (computed by Car_client.py)
    1 = car will reach the light before it changes colour → safe to cross
    0 = car will NOT make it, or light is not GREEN → slow down / stop

pedestrian_flag reference (computed by V2P.py)
    0 = no pedestrian detected or clear of road zone
    1 = pedestrian detected near crossing zone
    2 = pedestrian actively crossing (CROSSING intent, high risk)

position_flag reference (computed by V2P.py)
    0 = LEFT zone  (~128-256 px column)
    1 = RIGHT zone (~384-512 px column)
    null = centre / edge (not a critical zone)

motorcycleCollision reference
    0 = no risk
    1 = motorcycle in crossing zone + DANGER proximity + HIGH intent
        → driver alert should be shown immediately

Startup order
-------------
    1. hub.py              (IPC broker — must be first)
    2. dashboard_bridge.py (this file)
    3. Car_client.py       (publishes v2n_frame)
    4. V2P.py              (publishes v2p_frame + motorcycle_alert)
    5. server.py           (web server — serves data.json to the browser)
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

TOPIC_V2N_FRAME  = "v2n_frame"
TOPIC_V2P_FRAME  = "v2p_frame"
TOPIC_MOTO_ALERT = "motorcycle_alert"

_file_lock = threading.Lock()


# ─────────────────────────────────────────────────────────────
# data.json helpers
# ─────────────────────────────────────────────────────────────
def _load_data() -> dict:
    """
    Read data.json from disk.

    Returns an empty dict if the file is missing or corrupt so subsequent
    writes start from a clean state rather than crashing.
    """
    try:
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _save_data(data: dict) -> None:
    """
    Write *data* to data.json atomically.

    Writes to a .tmp file first, then renames it over the real file.
    The rename is atomic on POSIX systems (os.replace), so the web
    dashboard's polling loop never reads a partially-written file.
    """
    tmp = DATA_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp, DATA_FILE)


def _update_data(mutate) -> None:
    """
    Thread-safe read → mutate → write cycle.

    Parameters
    ----------
    mutate : callable(dict) → None
        Function that receives the loaded dict and modifies it in-place.
        Called while the file lock is held.
    """
    with _file_lock:
        data = _load_data()
        mutate(data)
        _save_data(data)


# ─────────────────────────────────────────────────────────────
# IPC hub callbacks
# ─────────────────────────────────────────────────────────────
def _on_v2n_frame(topic: str, payload: dict, sender: str) -> None:
    """
    Handle v2n_frame published by Car_client.py.

    Extracts only the five fields the dashboard needs and writes them to
    data["v2n"].  Intentionally does NOT write state or remaining_time —
    those are internal Car_client values used for crossing_flag computation
    only and must NOT appear in data.json.

    Payload fields consumed
    -----------------------
    traffic_flag        : int   — 0=GREEN, 1=YELLOW, 2=RED, 3=UNKNOWN
    ambulance_flag      : int   — 0 or 1
    distance_to_light_m : float | None
    transition_flag     : int   — forwarded unchanged from trafic_light.py
    crossing_flag       : int   — 0 or 1 (computed by Car_client)
    """
    try:
        traffic_flag    = payload.get("traffic_flag",        0)
        ambulance_flag  = payload.get("ambulance_flag",      0)
        distance_m      = payload.get("distance_to_light_m")   # may be None
        transition_flag = payload.get("transition_flag",    -1)
        crossing_flag   = payload.get("crossing_flag",       0)

        def mutate(data: dict) -> None:
            data["v2n"] = {
                "trafficLight":     traffic_flag,
                "ambulance":        ambulance_flag,
                "distanceToLightM": distance_m,
                "transitionFlag":   transition_flag,
                "crossingFlag":     crossing_flag,
            }

        _update_data(mutate)

        tf_labels = {0: "G→Y", 1: "Y→R", 2: "R→Y", 3: "Y→G", -1: "--"}
        print(
            f"[{NODE_NAME}] v2n ← {sender} | "
            f"light={traffic_flag} amb={ambulance_flag} "
            f"dist={distance_m} tf={tf_labels.get(transition_flag,'?')} "
            f"cross={crossing_flag}"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] ERROR on '{topic}': {exc}")


def _on_v2p_frame(topic: str, payload: dict, sender: str) -> None:
    """
    Handle v2p_frame published by V2P.py.

    Updates data["v2p"]["pedestrian"] and data["v2p"]["position"].
    Does NOT touch motorcycleCollision (handled by _on_motorcycle_alert).

    Payload fields consumed
    -----------------------
    pedestrian_flag : int        — 0=clear, 1=detected, 2=crossing
    position_flag   : int | None — 0=LEFT, 1=RIGHT, null=other zone
    """
    try:
        pedestrian_flag = payload.get("pedestrian_flag", 0)
        position_flag   = payload.get("position_flag")       # may be None

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["pedestrian"] = pedestrian_flag
            v2p["position"]   = position_flag

        _update_data(mutate)
        print(
            f"[{NODE_NAME}] v2p ← {sender} | "
            f"ped={pedestrian_flag} pos={position_flag}"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] ERROR on '{topic}': {exc}")


def _on_motorcycle_alert(topic: str, payload: dict, sender: str) -> None:
    """
    Handle motorcycle_alert published by V2P.py.

    Updates data["v2p"]["motorcycleCollision"] only.
    A value of 1 means the dashboard should show an immediate collision alert.

    Payload fields consumed
    -----------------------
    motorcycle_collision_flag : int — 0=no risk, 1=high-risk alert
    """
    try:
        moto_flag = payload.get("motorcycle_collision_flag", 0)

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["motorcycleCollision"] = moto_flag

        _update_data(mutate)

        if moto_flag:
            print(f"[{NODE_NAME}] ⚠ MOTORCYCLE COLLISION FLAG=1 (from {sender})")
        else:
            print(f"[{NODE_NAME}] motorcycle_collision=0 (from {sender})")

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
    print("=" * 60)

    node = IPCNode(NODE_NAME)
    if not node.connect():
        print(f"[{NODE_NAME}] ERROR: cannot connect to hub — start hub.py first.")
        return

    # Subscribe to all three topics
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
