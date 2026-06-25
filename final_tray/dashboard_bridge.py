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
    "trafficLight":     int   — 0=no light, 1=STOP(red/yellow), 2=GO(green)
    "ambulance":        int   — 0=blocked(normal), 1=ambulance present(allow)
    "transitionFlag":   int   — 0:G→Y, 1:Y→R, 2:R→Y, 3:Y→G, -1:mid-phase
    "crossingFlag":     int   — 0=must stop, 1=can cross
    "distanceToLightM": float | null
}
"v2p": {
    "pedestrian":          int       — 0=safe, 1=standing near, 2=crossing(warning)
    "position":            int|null  — 0=zone1, 1=zone2, 2=zone3, 3=zone4, null=none
    "motorcycleCollision": int       — 0=no risk, 1=collision risk
}

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
DATA_FILE = os.path.join(HERE, "data.json")

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
def _map_position(raw_position) -> int | None:
    """
    V2P.py publishes position_flag as:
        0 = LEFT zone  (128–256 px)
        1 = RIGHT zone (384–512 px)
        None = centre / extreme edge

    Dashboard spec (from screenshot):
        0 = أول الطريق  (zone 1 — leftmost)
        1 = الجزء الثاني (zone 2)
        2 = الجزء الثالث (zone 3)
        3 = آخر الطريق  (zone 4 — rightmost)
        null = not in a critical zone

    Mapping:
        V2P 0 (LEFT  zone, 128–256 px)  → dashboard 0  (أول الطريق)
        V2P 1 (RIGHT zone, 384–512 px)  → dashboard 3  (آخر الطريق)
        V2P None                         → null
    """
    if raw_position == 0:
        return 0
    if raw_position == 1:
        return 3
    return None   # centre / edge → null


# ─────────────────────────────────────────────────────────────
# IPC hub callbacks
# ─────────────────────────────────────────────────────────────
def _on_v2n_frame(topic: str, payload: dict, sender: str) -> None:
    """
    Handle v2n_frame from Car_client.py.

    Writes data["v2n"] with the five dashboard fields only.
    Raw state / remaining_time / warning / etc. are NOT written to
    data.json — they are Car_client internal values.

    Payload fields consumed
    -----------------------
    traffic_flag        : int   — 0=no light, 1=STOP, 2=GO
    ambulance_flag      : int   — 0=blocked, 1=ambulance
    transition_flag     : int   — forwarded unchanged
    crossing_flag       : int   — 0=stop, 1=can cross
    distance_to_light_m : float | None
    """
    try:
        traffic_flag    = int(payload.get("traffic_flag",        0))
        ambulance_flag  = int(payload.get("ambulance_flag",      0))
        transition_flag = int(payload.get("transition_flag",    -1))
        crossing_flag   = int(payload.get("crossing_flag",       0))
        distance_m      = payload.get("distance_to_light_m")          # float | None

        def mutate(data: dict) -> None:
            data["v2n"] = {
                "trafficLight":     traffic_flag,
                "ambulance":        ambulance_flag,
                "transitionFlag":   transition_flag,
                "crossingFlag":     crossing_flag,
                "distanceToLightM": distance_m,
            }

        _update_data(mutate)

        tf_lbl = {0:"G→Y", 1:"Y→R", 2:"R→Y", 3:"Y→G", -1:"--"}.get(
                    transition_flag, str(transition_flag))
        print(
            f"[{NODE_NAME}] v2n ← {sender} | "
            f"light={traffic_flag} amb={ambulance_flag} "
            f"tf={tf_lbl} cross={crossing_flag} dist={distance_m}"
        )

    except Exception as exc:
        print(f"[{NODE_NAME}] ERROR on '{topic}': {exc}")


def _on_v2p_frame(topic: str, payload: dict, sender: str) -> None:
    """
    Handle v2p_frame from V2P.py.

    Writes data["v2p"]["pedestrian"] and data["v2p"]["position"].

    pedestrian_flag encoding (dashboard spec from screenshot):
        0 = آمن       (safe — no pedestrian in zone)
        1 = واقفين جنب (standing near crossing)
        2 = بيعدوا    (actively crossing — WARNING)

    position mapping: see _map_position() above.
    """
    try:
        pedestrian_flag = int(payload.get("pedestrian_flag", 0))
        raw_pos         = payload.get("position_flag")         # 0, 1, or None
        position        = _map_position(raw_pos)

        def mutate(data: dict) -> None:
            v2p = data.setdefault("v2p", {})
            v2p["pedestrian"] = pedestrian_flag
            v2p["position"]   = position

        _update_data(mutate)
        print(
            f"[{NODE_NAME}] v2p ← {sender} | "
            f"ped={pedestrian_flag} pos={position}"
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
    print("  v2n.trafficLight   : 0=no light | 1=STOP | 2=GO")
    print("  v2n.ambulance      : 0=blocked  | 1=ambulance present")
    print("  v2n.transitionFlag : -1=mid | 0:G→Y | 1:Y→R | 2:R→Y | 3:Y→G")
    print("  v2n.crossingFlag   : 0=stop | 1=can cross")
    print("  v2p.pedestrian     : 0=safe | 1=near | 2=crossing")
    print("  v2p.position       : 0-3=zone | null=none")
    print("  v2p.motorcycleCollision: 0=ok | 1=risk")
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
