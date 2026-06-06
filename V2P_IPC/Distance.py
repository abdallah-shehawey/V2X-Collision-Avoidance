"""
distance.py  –  Distance Monitor Node
=======================================
Detects vehicles in a video stream using edge detection and estimates
their distance via the pinhole-camera formula. Results are published to
the IPC hub so downstream nodes (V2V, V2N, ADAS) can consume them.

IPC Role
--------
    Node name  : "distance_monitor"
    Subscribes : —  (no inbound topics needed)
    Publishes  : "distance_data"

Published schema  ("distance_data")
------------------------------------
{
    "timestamp"  : float,          # time.time()
    "car_count"  : int,            # total cars visible this frame
    "closest_m"  : float,          # distance to nearest car in metres
    "front_cm"   : float,          # same value converted to cm  (ADAS compat.)
    "cars": [
        {
            "id"         : int,
            "distance_m" : float,
            "status"     : "DANGER" | "WARNING" | "APPROACHING" | "FAR"
        },
        ...
    ]
}

Kernel-level IPC path
---------------------
    connect()   sys_connect()  → Unix Domain Socket to hub
    sendall()   sys_send()     → publish frame  (JSON + newline)

Distance formula
----------------
    distance = (real_width_m × focal_length_px) / bounding_box_width_px
    real_width_m  = 1.8  (average car body width)
    focal_length  = 650  (calibrated for the test camera / video)
"""

import cv2
import numpy as np
import os
import time
import sys

# ── IPC ──────────────────────────────────────────────────────────────────────
# ipc_node.py must be in the same directory (or on PYTHONPATH)
from ipc_node import IPCNode

# ── Constants ─────────────────────────────────────────────────────────────────
VIDEO_PATH      = "traffic_test.mp4"
PROCESS_EVERY   = 5          # analyse every Nth frame to save CPU
PUBLISH_EVERY   = 5          # publish to hub every Nth processed frame

CAR_REAL_WIDTH  = 1.8        # metres
FOCAL_LENGTH    = 650        # pixels  (tune to your camera)

# Distance thresholds (metres)
DANGER_M      = 10.0
WARNING_M     = 20.0
APPROACH_M    = 40.0

NODE_NAME     = "distance_monitor"
TOPIC_PUBLISH = "distance_data"


# ── Distance formula ──────────────────────────────────────────────────────────
def estimate_distance(box_width_px: int) -> float:
    """Return estimated distance in metres; 999 if box width is zero."""
    if box_width_px <= 0:
        return 999.0
    return (CAR_REAL_WIDTH * FOCAL_LENGTH) / box_width_px


def classify_distance(dist_m: float) -> str:
    if dist_m < DANGER_M:
        return "DANGER"
    elif dist_m < WARNING_M:
        return "WARNING"
    elif dist_m < APPROACH_M:
        return "APPROACHING"
    return "FAR"


# ── Video source helper ───────────────────────────────────────────────────────
def open_video(path: str) -> cv2.VideoCapture:
    """Open video file; download sample if missing."""
    if not os.path.exists(path):
        print(f"[DIST] Video not found at '{path}'.")
        print("[DIST] Please place a traffic video at that path and restart.")
        sys.exit(1)

    cap = cv2.VideoCapture(path)
    if not cap.isOpened():
        print("[DIST] ❌ Cannot open video file.")
        sys.exit(1)

    fps    = cap.get(cv2.CAP_PROP_FPS)
    frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    print(f"[DIST] ✅ Video opened: {frames} frames @ {fps:.1f} FPS")
    return cap


# ── Detection ─────────────────────────────────────────────────────────────────
def detect_cars(frame: np.ndarray) -> list[tuple[int, int, int, int]]:
    """
    Simple contour-based car detector.
    Returns a list of (x1, y1, x2, y2) bounding boxes.
    """
    gray  = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blur  = cv2.GaussianBlur(gray, (5, 5), 0)
    edges = cv2.Canny(blur, 50, 150)

    contours, _ = cv2.findContours(
        edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE
    )

    boxes = []
    for cnt in contours:
        x, y, w, h = cv2.boundingRect(cnt)
        # Filter to car-sized rectangles
        if 40 < w < 200 and 30 < h < 150:
            boxes.append((x, y, x + w, y + h))
    return boxes


# ── Main ──────────────────────────────────────────────────────────────────────
def main() -> None:
    # ── Connect to hub ────────────────────────────────────────────────
    node = IPCNode(NODE_NAME)
    if not node.connect():
        print(f"[DIST] ❌ Could not connect to hub. Is hub.py running?")
        sys.exit(1)

    # distance_monitor only publishes — no subscriptions needed.
    node.start_listening()   # keeps socket alive for ack frames

    print(f"[DIST] Node '{NODE_NAME}' ready")
    print(f"[DIST] Publishing to topic : '{TOPIC_PUBLISH}'\n")

    # ── Open video ────────────────────────────────────────────────────
    cap         = open_video(VIDEO_PATH)
    frame_count = 0
    pub_count   = 0          # counts processed frames for publish throttling

    print("=" * 60)
    print("🚀 DISTANCE MONITOR — connected to IPC hub")
    print("=" * 60)

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                # Loop the video for continuous demo
                cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                continue

            frame_count += 1

            # ── Skip frames to save CPU ──────────────────────────────
            if frame_count % PROCESS_EVERY != 0:
                time.sleep(0.01)
                continue

            pub_count += 1

            # ── Detect and estimate ───────────────────────────────────
            boxes    = detect_cars(frame)
            car_list = []

            for idx, (x1, y1, x2, y2) in enumerate(boxes[:10]):
                dist_m  = estimate_distance(x2 - x1)
                status  = classify_distance(dist_m)
                car_list.append({
                    "id":         idx + 1,
                    "distance_m": round(dist_m, 2),
                    "status":     status,
                })

            # Closest car (for ADAS front_cm compatibility)
            closest_m  = min((c["distance_m"] for c in car_list), default=999.0)
            front_cm   = round(closest_m * 100, 1)

            # ── Console display ───────────────────────────────────────
            os.system("clear")
            print("=" * 55)
            print(f"🎥 Frame: {frame_count:>6} | 🚗 Cars: {len(car_list)}")
            print("-" * 55)

            if car_list:
                for car in car_list[:5]:
                    icons = {"DANGER": "🔴", "WARNING": "🟡",
                             "APPROACHING": "🟢", "FAR": "⚪"}
                    icon = icons.get(car["status"], "⚪")
                    print(
                        f"  {icon} Car {car['id']:>2} | "
                        f"{car['distance_m']:>6.1f} m | {car['status']}"
                    )
            else:
                print("  ✅ No cars detected")

            print(f"\n  📡 Publishing to hub  → topic='{TOPIC_PUBLISH}'")
            print("  Press Ctrl+C to stop")
            print("=" * 55)

            # ── Publish to hub every PUBLISH_EVERY processed frames ──
            if pub_count % PUBLISH_EVERY == 0:
                payload = {
                    "timestamp":  time.time(),
                    "car_count":  len(car_list),
                    "closest_m":  closest_m,
                    "front_cm":   front_cm,      # ADAS uses this field
                    "cars":       car_list,
                }
                node.publish(TOPIC_PUBLISH, payload)
                print(
                    f"[DIST] publish → '{TOPIC_PUBLISH}' | "
                    f"cars={len(car_list)} | closest={closest_m:.1f}m"
                )

            time.sleep(0.03)

    except KeyboardInterrupt:
        print("\n\n🛑 Distance monitor stopped.")
    finally:
        cap.release()


if __name__ == "__main__":
    main()
