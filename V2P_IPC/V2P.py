# -*- coding: utf-8 -*-
"""
V2P.py - Vehicle-to-Pedestrian System (Raspberry Pi)
======================================================
Runs a YOLO ONNX model on PiCamera2 frames to detect persons, bicycles,
cars, and motorcycles, and tracks every detected object across frames
with a lightweight centroid tracker (each object gets a stable ID and a
position history). The per-object history is what lets us reason about
*intent* (is this person actually walking into the road, or just
standing there?) instead of only "is something inside this rectangle".

This file replaces the two previous prototypes:
  - the original V2P.py            (hub IPC wiring, but zone-only people
                                     detection, no per-object IDs)
  - the standalone v2p_system.py   (multi-object tracking + intent/
                                     proximity analysis, but no hub
                                     connection at all)

Everything from both is merged here: the tracking/intent engine from
v2p_system.py now drives the hub publishing that the original V2P.py
used to do.

Hub topics
----------
SUBSCRIBES to:
  "traffic_light_state" - published by the V2I bridge so V2P knows the
                           current light state and can warn pedestrians
                           accordingly.
  "v2v_alert"            - published when V2V (UART/STM32) detects a
                            hazard, so V2P can raise its own alert level.

PUBLISHES (same field names/shape as the original V2P.py, so any other
hub client written against the old payloads keeps working unchanged):
  "pedestrian_status"   - {timestamp, pedestrian_count, moving_count,
                            crossing_safe, alert_level, traffic_light}
  "vehicle_data"        - {timestamp, speed_kmh, brake_pressed,
                            car_count, moving_cars, bike_count,
                            ultrasonic: {front_cm}}

PUBLISHES (new, compact bit-packed "frame" topics - the rest of the
hub only needs the final flags, not the raw detections):
  "v2p_frame"           - pedestrian_flag / position_flag, derived
                           ONLY from analyze_intent()'s risk_level for
                           class_id == 0 (person). See
                           _estimate_pedestrian_flag() below.
  "v2n_moto_frame"       - motorcycle_alert flag (0/1). This is a
                           strict filter: a motorcycle riding normally
                           must NEVER raise this flag or appear on the
                           dashboard. It only flips to 1 when the
                           motorcycle is BOTH (a) inside the host
                           vehicle's danger_zone (same proximity DANGER
                           threshold used for the on-screen "too close"
                           label) AND (b) its tracked trajectory shows
                           risk_level-equivalent HIGH (fast, closing
                           approach) - i.e. an actual collision course,
                           not just "a motorcycle exists somewhere on
                           the road". This flag is intentionally
                           independent from ADAS/FCW.
                           See analyze_motorcycle_danger() below.

dashboard_bridge.py forwards "v2p_frame" into data["v2p"] and
"v2n_moto_frame" into data["v2n"]["motorcycleDanger"]; the dashboard
front-end (app.js) is wired to stay completely silent for motorcycles
unless that value is exactly 1.

Run order: hub.py -> V2P.py (+ Car_client-1.py + dashboard_bridge.py)
"""

import cv2
import numpy as np
import time
import struct
import threading
from collections import deque, defaultdict

import onnxruntime as ort
from picamera2 import Picamera2

from ipc_node import IPCNode   # local hub client

print("=" * 60)
print("V2P SYSTEM - Multi-object tracking + intent + IPC hub")
print("=" * 60)

# ============================================================
# 1. Hub topic names
# ============================================================
TOPIC_TRAFFIC_LIGHT_STATE = "traffic_light_state"   # subscribe
TOPIC_V2V_ALERT           = "v2v_alert"              # subscribe
TOPIC_PEDESTRIAN_STATUS   = "pedestrian_status"      # publish (old shape)
TOPIC_VEHICLE_DATA        = "vehicle_data"           # publish (old shape)
TOPIC_V2P_FRAME           = "v2p_frame"              # publish (new, compact)
TOPIC_V2N_MOTO_FRAME      = "v2n_moto_frame"         # publish (new, compact)

# ============================================================
# 2. Compact bit-packed frames
# ============================================================
# Same idea as NEIGHBOR_FMT for V2V in uart.py: the raw detections stay
# local to this process; only the final flags travel across the hub.

# v2p_frame - 1 byte:
#   bits [0:2] pedestrian_flag  0 = road clear, no person at HIGH risk
#                                1 = person(s) tracked, none crossing yet
#                                2 = person(s) actually crossing now
#   bits [2:4] position_flag    quarter of the frame (0=leftmost .. 3=
#                                rightmost) of the closest crossing
#                                person; meaningless when pedestrian_flag<2
#   bits [4:8] reserved (0)
V2P_FRAME_FMT = "<B"


def pack_v2p_frame(pedestrian_flag: int, position_flag: int) -> bytes:
    """Pack pedestrian_flag (2 bits) + position_flag (2 bits) into 1 byte."""
    pedestrian_flag &= 0b11
    position_flag   &= 0b11
    packed = (position_flag << 2) | pedestrian_flag
    return struct.pack(V2P_FRAME_FMT, packed)


# v2n_moto_frame - 1 byte:
#   bit [0] motorcycle_alert   0 = hidden (default, normal riding)
#                              1 = motorcycle is in the danger_zone AND
#                                  on a HIGH-risk collision trajectory
#   bits [1:8] reserved (0)
V2N_MOTO_FRAME_FMT = "<B"


def pack_moto_frame(motorcycle_alert_flag: int) -> bytes:
    """Pack the single motorcycle_alert bit into 1 byte."""
    motorcycle_alert_flag &= 0b1
    return struct.pack(V2N_MOTO_FRAME_FMT, motorcycle_alert_flag)


# ============================================================
# 3. Settings & Configuration
# ============================================================
MODEL_PATH       = "model2.onnx"
CONF_THRESH      = 0.30
MODEL_INPUT_SIZE = 640
FRAME_W, FRAME_H = 640, 480

TARGET_CLASSES = {
    0: "person",
    1: "bicycle",
    2: "car",
    3: "motorcycle",
}

SPEED_THRESHOLD_FAST = 6.0     # px/frame - "approaching fast"
SPEED_THRESHOLD_SLOW = 1.5     # px/frame - "essentially standing still"
CROSSING_ZONE_RATIO  = 0.35    # bottom fraction of the frame treated as "the road"
APPROACH_FRAMES      = 5       # how many recent history points to look at

FRAME_AREA        = FRAME_W * FRAME_H
PROXIMITY_DANGER  = 0.10       # box covers >= 10% of the frame -> "too close" / danger_zone
PROXIMITY_WARNING = 0.05
PROXIMITY_SAFE    = 0.02

PUB_INTERVAL = 0.5             # seconds between hub publishes (don't flood the hub)

# ============================================================
# 4. IPC Node - connect to the local hub
# ============================================================
ipc = IPCNode("v2p_camera")

# Shared state updated by hub callbacks (background thread) and read by
# the main detection loop.
_traffic_light_state = "RED"   # updated from TOPIC_TRAFFIC_LIGHT_STATE
_v2v_alert_active    = False   # updated from TOPIC_V2V_ALERT
_ipc_lock            = threading.Lock()


def _on_traffic_light(topic, data, sender):
    """Receive the current traffic-light state from the V2I bridge via the hub."""
    global _traffic_light_state
    with _ipc_lock:
        _traffic_light_state = data.get("state", "RED")
    print(f"\n[V2P] traffic light -> {_traffic_light_state}  (from {sender})")


def _on_v2v_alert(topic, data, sender):
    """Receive a V2V hazard alert (e.g. EEBL, FCW) from the UART bridge via the hub."""
    global _v2v_alert_active
    with _ipc_lock:
        _v2v_alert_active = bool(data.get("active", False))
    print(f"\n[V2P] V2V alert active={_v2v_alert_active}  (from {sender})")


print("\nConnecting V2P to the local IPC Hub ...")
if ipc.connect():
    ipc.subscribe(TOPIC_TRAFFIC_LIGHT_STATE, _on_traffic_light)
    ipc.subscribe(TOPIC_V2V_ALERT, _on_v2v_alert)
    ipc.start_listening()
    print(f"IPC connected - subscribed to '{TOPIC_TRAFFIC_LIGHT_STATE}' and '{TOPIC_V2V_ALERT}'\n")
else:
    print("WARNING: hub not found - V2P will run without IPC (no hub publishing)\n")


# ============================================================
# 5. Centroid Tracker
# ============================================================
class CentroidTracker:
    """
    Minimal multi-object tracker: assigns a stable integer ID to every
    detected box across frames by matching centroids to the closest
    previous centroid (within max_distance), and keeps a short position
    history per ID (used for speed/intent analysis).
    """

    def __init__(self, max_disappeared=20, max_distance=80):
        self.next_id     = 0
        self.objects     = {}
        self.disappeared = {}
        self.max_dis     = max_disappeared
        self.max_dist    = max_distance
        self.history     = defaultdict(lambda: deque(maxlen=30))

    def register(self, cx, cy):
        self.objects[self.next_id]     = (cx, cy)
        self.disappeared[self.next_id] = 0
        self.history[self.next_id].append((cx, cy))
        self.next_id += 1

    def deregister(self, obj_id):
        del self.objects[obj_id]
        del self.disappeared[obj_id]

    def update(self, rects):
        if len(rects) == 0:
            for obj_id in list(self.disappeared):
                self.disappeared[obj_id] += 1
                if self.disappeared[obj_id] > self.max_dis:
                    self.deregister(obj_id)
            return {}

        input_centroids = []
        for (x1, y1, x2, y2, _) in rects:
            cx = int((x1 + x2) / 2)
            cy = int((y1 + y2) / 2)
            input_centroids.append((cx, cy))

        if len(self.objects) == 0:
            for i, (cx, cy) in enumerate(input_centroids):
                self.register(cx, cy)
        else:
            obj_ids   = list(self.objects.keys())
            obj_cents = list(self.objects.values())

            D = np.zeros((len(obj_cents), len(input_centroids)))
            for r, (ox, oy) in enumerate(obj_cents):
                for c, (ix, iy) in enumerate(input_centroids):
                    D[r, c] = np.sqrt((ox - ix) ** 2 + (oy - iy) ** 2)

            rows = D.min(axis=1).argsort()
            cols = D.argmin(axis=1)[rows]

            used_rows, used_cols = set(), set()
            for (row, col) in zip(rows, cols):
                if row in used_rows or col in used_cols:
                    continue
                if D[row, col] > self.max_dist:
                    continue
                obj_id = obj_ids[row]
                cx, cy = input_centroids[col]
                self.objects[obj_id]     = (cx, cy)
                self.disappeared[obj_id] = 0
                self.history[obj_id].append((cx, cy))
                used_rows.add(row)
                used_cols.add(col)

            for col in range(len(input_centroids)):
                if col not in used_cols:
                    self.register(*input_centroids[col])

            for row in range(len(obj_cents)):
                if row not in used_rows:
                    obj_id = obj_ids[row]
                    self.disappeared[obj_id] += 1
                    if self.disappeared[obj_id] > self.max_dis:
                        self.deregister(obj_id)

        result = {}
        for i, (x1, y1, x2, y2, class_id) in enumerate(rects):
            cx = int((x1 + x2) / 2)
            cy = int((y1 + y2) / 2)
            for obj_id, (ox, oy) in self.objects.items():
                if ox == cx and oy == cy:
                    result[obj_id] = (cx, cy, x1, y1, x2, y2, class_id)
                    break
        return result


# ============================================================
# 6. Intent Analyzer (persons only)
# ============================================================
def analyze_intent(obj_id, history, class_id):
    """
    Classify a tracked PERSON's intent from its recent position history.

    Returns (label, risk_level, color):
        risk_level == "HIGH"  -> the person is actually crossing now.
                                  This is the ONLY signal used to drive
                                  pedestrian_flag == 2 (see
                                  _estimate_pedestrian_flag()).
        risk_level == "MED"   -> approaching the road, not crossing yet.
        risk_level == "LOW"   -> standing / walking away / not relevant.

    Non-person classes always return risk_level "LOW" (this function is
    intentionally person-only; motorcycles use a separate, stricter
    check - see analyze_motorcycle_danger()).
    """
    if class_id != 0:
        return None, "LOW", (0, 255, 0)

    pts = list(history[obj_id])
    if len(pts) < 3:
        return "Observing", "LOW", (0, 255, 0)

    recent    = pts[-APPROACH_FRAMES:] if len(pts) >= APPROACH_FRAMES else pts
    dists     = [np.sqrt((recent[i][0] - recent[i - 1][0]) ** 2 +
                          (recent[i][1] - recent[i - 1][1]) ** 2)
                 for i in range(1, len(recent))]
    avg_speed = np.mean(dists) if dists else 0.0

    dx = pts[-1][0] - pts[max(0, len(pts) - APPROACH_FRAMES)][0]
    dy = pts[-1][1] - pts[max(0, len(pts) - APPROACH_FRAMES)][1]

    moving_toward_road = dy > 3
    cy_now              = pts[-1][1]
    in_road_zone         = cy_now > FRAME_H * (1 - CROSSING_ZONE_RATIO)
    moving_laterally     = abs(dx) > abs(dy) * 0.8

    if in_road_zone and avg_speed > SPEED_THRESHOLD_FAST:
        return "CROSSING FAST", "HIGH", (0, 0, 255)
    if in_road_zone and moving_laterally:
        return "CROSSING", "HIGH", (0, 0, 255)
    if moving_toward_road and avg_speed > SPEED_THRESHOLD_SLOW:
        return "Approaching", "MED", (0, 165, 255)
    if avg_speed < SPEED_THRESHOLD_SLOW:
        return "Standing", "LOW", (0, 255, 0)
    return "Walking", "LOW", (0, 200, 100)


# ============================================================
# 7. Proximity Estimator
# ============================================================
def estimate_proximity(x1, y1, x2, y2):
    """
    Fraction of the frame a box occupies, used both for the on-screen
    "too close" label and as the danger_zone test for motorcycles.
    """
    box_area = (x2 - x1) * (y2 - y1)
    ratio    = box_area / FRAME_AREA

    if ratio >= PROXIMITY_DANGER:
        return ratio, "DANGER",  "TOO CLOSE!", (0, 0, 255)
    elif ratio >= PROXIMITY_WARNING:
        return ratio, "WARNING", "CLOSE",      (0, 165, 255)
    else:
        return ratio, "SAFE",    "",            (0, 220, 0)


# ============================================================
# 8. Flag derivation - pedestrian / position / motorcycle danger
# ============================================================
def _track_avg_speed_px(history, obj_id, frames=APPROACH_FRAMES):
    """Average per-frame centroid displacement (pixels) over the last
    `frames` tracked positions - used by analyze_motorcycle_danger()."""
    pts = list(history[obj_id])
    if len(pts) < 2:
        return 0.0
    recent = pts[-frames:] if len(pts) >= frames else pts
    dists = [
        float(np.hypot(recent[i][0] - recent[i - 1][0],
                        recent[i][1] - recent[i - 1][1]))
        for i in range(1, len(recent))
    ]
    return float(np.mean(dists)) if dists else 0.0


def analyze_motorcycle_danger(obj_id, history, class_id, x1, y1, x2, y2):
    """
    Strict filter for the motorcycle_alert flag.

    Product decision: a motorcycle riding normally must NEVER show up
    on the dashboard or trigger any alert, and has nothing to do with
    FCW. This flag only flips to True when BOTH conditions hold at the
    same time:

      1) danger_zone  - the motorcycle's bounding box already meets the
                         same PROXIMITY_DANGER threshold used for the
                         "too close" label, i.e. it is right next to /
                         right in front of the host vehicle.
      2) HIGH risk     - its recent trajectory shows it closing in fast
                         (same fast-approach test used for pedestrian
                         risk_level == HIGH), meaning an actual
                         collision is likely, not just "a motorcycle is
                         somewhere on the road".

    Returns True/False. Only ever evaluated for class_id == 3.
    """
    if class_id != 3:
        return False

    _, prox_level, _, _ = estimate_proximity(x1, y1, x2, y2)
    if prox_level != "DANGER":
        return False

    pts = list(history[obj_id])
    if len(pts) < 3:
        return False

    avg_speed = _track_avg_speed_px(history, obj_id)
    dy = pts[-1][1] - pts[max(0, len(pts) - APPROACH_FRAMES)][1]
    approaching_fast = (dy > 3) and (avg_speed > SPEED_THRESHOLD_FAST)

    return approaching_fast


def _estimate_pedestrian_flag(tracked, history):
    """
    pedestrian_flag for data["v2p"]["pedestrian"], derived ONLY from
    analyze_intent()'s risk_level for class_id == 0 (person):

        0 -> no person tracked at all (road clear)
        1 -> person(s) tracked, but none at risk_level HIGH
             (Standing / Walking / Approaching)
        2 -> at least one person has risk_level == HIGH
             (actually crossing right now)

    Returns (flag, crossing_people) where crossing_people is the list
    of (obj_id, x1, y1, x2, y2) for the HIGH-risk persons, used by
    _estimate_position_flag().
    """
    persons = [(oid, v) for oid, v in tracked.items() if v[6] == 0]
    if not persons:
        return 0, []

    crossing_people = []
    for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in persons:
        _, risk_level, _ = analyze_intent(obj_id, history, class_id)
        if risk_level == "HIGH":
            crossing_people.append((obj_id, x1, y1, x2, y2))

    return (2, crossing_people) if crossing_people else (1, [])


def _estimate_position_flag(crossing_people, frame_w):
    """
    Quarter of the frame (0=leftmost .. 3=rightmost) of the closest
    HIGH-risk (actually crossing) person. None if nobody is crossing.
    "Closest" = the box with the largest y2 (lowest in frame = nearest
    to the camera).
    """
    if not crossing_people or frame_w <= 0:
        return None

    closest = max(crossing_people, key=lambda p: p[4])  # p = (obj_id, x1, y1, x2, y2)
    _, x1, _, x2, _ = closest
    cx = (x1 + x2) / 2

    quarter_w = frame_w / 4.0
    col = int(cx // quarter_w)
    return max(0, min(3, col))


def _estimate_alert_level(persons_count: int,
                           moving_count: int,
                           crossing_safe: bool,
                           light: str,
                           v2v: bool) -> str:
    """Return 'none', 'caution', or 'danger' for pedestrian_status."""
    if v2v or (persons_count > 0 and light == "GREEN" and not crossing_safe):
        return "danger"
    if persons_count > 0 and light != "RED":
        return "caution"
    if moving_count > 0:
        return "caution"
    return "none"


def _estimate_speed(boxes_prev, boxes_curr, dt: float) -> float:
    """
    Very rough pixel-motion -> km/h estimate for the closest tracked
    car, used to fill vehicle_data["speed_kmh"]. Placeholder until a
    proper radar/GPS speed source is available; STM32/V2V supplies the
    authoritative host-vehicle speed elsewhere.
    """
    if not boxes_prev or not boxes_curr or dt <= 0:
        return 0.0
    prev_cy = boxes_prev[0][1] + boxes_prev[0][3] / 2
    curr_cy = boxes_curr[0][1] + boxes_curr[0][3] / 2
    delta_px = abs(curr_cy - prev_cy)
    speed_mps = (delta_px * 0.003) / dt   # ~1 px ~= 0.003 m at ~5 m distance (very rough)
    return round(min(speed_mps * 3.6, 120.0), 1)


# ============================================================
# 9. Camera Initialization
# ============================================================
print("\nOpening camera...")
picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"format": "RGB888", "size": (FRAME_W, FRAME_H)},
    controls={"FrameRate": 30}
)
picam2.configure(config)
picam2.start()
time.sleep(1)
print("Camera ready!")

# ============================================================
# 10. Model Loading
# ============================================================
print(f"\nLoading model: {MODEL_PATH}")
opts = ort.SessionOptions()
opts.intra_op_num_threads = 2
session = ort.InferenceSession(
    MODEL_PATH, sess_options=opts,
    providers=["CPUExecutionProvider"]
)
input_name = session.get_inputs()[0].name
print("Model loaded!")

# ============================================================
# 11. Tracker Init
# ============================================================
tracker = CentroidTracker(max_disappeared=25, max_distance=100)

# ============================================================
# 12. Runtime Loop
# ============================================================
frame_count  = 0
skip_frames  = 2
fps          = 0
fps_counter  = 0
fps_time     = time.time()
last_stats   = {"cars": 0, "persons": 0, "bikes": 0}
last_objects = {}

# Speed estimate state (closest tracked car, frame to frame)
prev_car_boxes: list = []
prev_time: float     = time.time()
speed_kmh: float      = 0.0

# Hub publish throttle
_last_pub_time = 0.0

print("\nSystem ready! Press 'q' to stop\n")

try:
    while True:
        rgb_frame = picam2.capture_array()
        frame_count += 1

        fps_counter += 1
        if time.time() - fps_time >= 1.0:
            fps         = fps_counter
            fps_counter = 0
            fps_time    = time.time()

        frame = cv2.cvtColor(rgb_frame, cv2.COLOR_RGB2BGR)

        if frame_count % skip_frames == 0:
            blob = cv2.resize(rgb_frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))
            blob = blob.astype(np.float32) / 255.0
            blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

            outputs = session.run(None, {input_name: blob})
            preds   = np.squeeze(outputs[0]).T

            raw_boxes, raw_scores, raw_classes = [], [], []
            scale_x = FRAME_W / MODEL_INPUT_SIZE
            scale_y = FRAME_H / MODEL_INPUT_SIZE

            for p in preds:
                class_scores = p[4:]
                class_id     = int(np.argmax(class_scores))
                score        = float(class_scores[class_id])
                if score > CONF_THRESH and class_id in TARGET_CLASSES:
                    cx, cy, wb, hb = p[0:4]
                    x1 = int((cx - wb / 2) * scale_x)
                    y1 = int((cy - hb / 2) * scale_y)
                    x2 = int((cx + wb / 2) * scale_x)
                    y2 = int((cy + hb / 2) * scale_y)
                    raw_boxes.append([x1, y1, x2 - x1, y2 - y1])
                    raw_scores.append(score)
                    raw_classes.append(class_id)

            rects_for_tracker = []
            if raw_boxes:
                indices = cv2.dnn.NMSBoxes(raw_boxes, raw_scores, CONF_THRESH, 0.4)
                if len(indices) > 0:
                    for i in indices.flatten():
                        x1, y1, wb, hb = raw_boxes[i]
                        x2, y2 = x1 + wb, y1 + hb
                        x1 = max(0, x1);       y1 = max(0, y1)
                        x2 = min(FRAME_W, x2); y2 = min(FRAME_H, y2)
                        rects_for_tracker.append((x1, y1, x2, y2, raw_classes[i]))

            tracked      = tracker.update(rects_for_tracker)
            last_objects = tracked

            last_stats = {
                "cars":    sum(1 for v in tracked.values() if v[6] == 2),
                "persons": sum(1 for v in tracked.values() if v[6] == 0),
                "bikes":   sum(1 for v in tracked.values() if v[6] in (1, 3)),
            }

            # Closest-car speed estimate (same rough technique as before,
            # now fed from the tracker's current detections).
            car_boxes_now = [
                [x1, y1, x2 - x1, y2 - y1]
                for (x1, y1, x2, y2, cid) in rects_for_tracker if cid == 2
            ]
            now_t = time.time()
            dt    = now_t - prev_time
            speed_kmh = _estimate_speed(prev_car_boxes, car_boxes_now, dt)
            prev_car_boxes = car_boxes_now
            prev_time      = now_t

        warnings_list = []

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            class_name = TARGET_CLASSES[class_id]

            if class_name == "car":
                color = (0, 0, 255)
            elif class_name == "person":
                color = (0, 255, 0)
            else:
                color = (255, 100, 0)

            ratio, prox_level, prox_label, prox_color = estimate_proximity(
                x1, y1, x2, y2
            )
            prox_pct = f"{ratio*100:.1f}%"

            if prox_level == "DANGER":
                color = (0, 0, 255)
                warnings_list.append((
                    f"! TOO CLOSE: {class_name} #{obj_id} ({prox_pct})",
                    (0, 0, 255)
                ))
            elif prox_level == "WARNING":
                if class_name != "person":
                    color = (0, 165, 255)
                warnings_list.append((
                    f"~ CLOSE: {class_name} #{obj_id} ({prox_pct})",
                    (0, 165, 255)
                ))

            # Road-zone entry warnings - cars and bicycles only.
            # Motorcycles are deliberately EXCLUDED here: per spec they
            # must stay invisible while riding normally. A motorcycle
            # only ever appears below, and only when
            # analyze_motorcycle_danger() returns True.
            if class_id in (1, 2):
                cy_obj = (y1 + y2) // 2
                if cy_obj > FRAME_H * (1 - CROSSING_ZONE_RATIO):
                    zone_msgs = {
                        2: (f"! CAR IN ROAD ZONE (#{obj_id})", (0, 0, 255)),
                        1: (f"! BICYCLE IN ROAD (#{obj_id})",  (0, 140, 255)),
                    }
                    warnings_list.append(zone_msgs[class_id])
                    color = (0, 0, 255)

            # Strict motorcycle danger check (danger_zone AND HIGH-risk
            # closing trajectory). Silent / invisible otherwise.
            if class_id == 3 and analyze_motorcycle_danger(
                obj_id, tracker.history, class_id, x1, y1, x2, y2
            ):
                color = (0, 0, 255)
                warnings_list.append(
                    (f"!! MOTORCYCLE COLLISION RISK (#{obj_id})", (0, 0, 255))
                )

            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id
            )

            if class_id == 0:
                if risk_level == "HIGH":
                    color = intent_color
                    warnings_list.append(
                        (f"! PERSON CROSSING (#{obj_id})", (0, 0, 255))
                    )
                elif risk_level == "MED":
                    color = intent_color
                    warnings_list.append(
                        (f"~ Person Approaching (#{obj_id})", (0, 165, 255))
                    )

            thickness = 3 if prox_level == "DANGER" or risk_level == "HIGH" else 2
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)

            cv2.putText(frame, f"#{obj_id} {class_name}",
                        (x1, max(y1 - 22, 12)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

            if prox_level != "SAFE":
                cv2.putText(frame, prox_label,
                            (x2 + 4, (y1 + y2) // 2),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, prox_color, 2)

            if intent_label:
                cv2.putText(frame, intent_label,
                            (x1, max(y1 - 6, 22)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)

            bar_x  = min(x2 + 2, FRAME_W - 8)
            bar_h  = y2 - y1
            fill_h = int(bar_h * min(ratio / PROXIMITY_DANGER, 1.0))
            cv2.rectangle(frame, (bar_x, y1), (bar_x + 5, y2), (80, 80, 80), -1)
            cv2.rectangle(frame, (bar_x, y2 - fill_h), (bar_x + 5, y2), prox_color, -1)

            hist = list(tracker.history[obj_id])
            for k in range(1, len(hist)):
                alpha       = k / len(hist)
                trail_color = tuple(int(c * alpha) for c in color)
                cv2.line(frame, hist[k-1], hist[k], trail_color, 1)

        zone_y = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        cv2.line(frame, (0, zone_y), (FRAME_W, zone_y), (0, 200, 200), 1)
        cv2.putText(frame, "-- Road Zone --", (5, zone_y - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 200, 200), 1)

        if warnings_list:
            panel_h = 30 + len(warnings_list) * 28
            overlay = frame.copy()
            cv2.rectangle(overlay,
                          (0, FRAME_H - panel_h),
                          (FRAME_W, FRAME_H),
                          (20, 20, 20), -1)
            frame = cv2.addWeighted(overlay, 0.6, frame, 0.4, 0)

            if any(w[1] == (0, 0, 255) for w in warnings_list):
                flash = frame.copy()
                cv2.rectangle(flash, (0, 0), (FRAME_W, FRAME_H), (0, 0, 180), -1)
                frame = cv2.addWeighted(flash, 0.12, frame, 0.88, 0)

            cv2.putText(frame, "DRIVER ALERTS:",
                        (10, FRAME_H - panel_h + 20),
                        cv2.FONT_HERSHEY_DUPLEX, 0.55, (255, 255, 255), 1)

            for idx, (warn_text, warn_color) in enumerate(warnings_list):
                y_pos = FRAME_H - panel_h + 20 + (idx + 1) * 28
                cv2.rectangle(frame, (8, y_pos - 14), (18, y_pos - 4), warn_color, -1)
                cv2.putText(frame, warn_text, (24, y_pos - 3),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, warn_color, 2)

        cv2.putText(frame, f"Cars: {last_stats['cars']}",       (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}", (10, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",     (10, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 0), 2)
        cv2.putText(frame, f"FPS: {fps}",                       (10, 105),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        print(
            f"\rCars:{last_stats['cars']} | "
            f"Persons:{last_stats['persons']} | "
            f"Bikes:{last_stats['bikes']} | "
            f"FPS:{fps}",
            end=""
        )

        # ── Flags derived for this frame ─────────────────────────────
        pedestrian_flag, crossing_people = _estimate_pedestrian_flag(
            last_objects, tracker.history
        )
        position_flag = _estimate_position_flag(crossing_people, FRAME_W)

        motorcycle_alert_flag = 0
        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            if class_id == 3 and analyze_motorcycle_danger(
                obj_id, tracker.history, class_id, x1, y1, x2, y2
            ):
                motorcycle_alert_flag = 1
                break

        # ── Publish to hub (throttled) ───────────────────────────────
        now = time.time()
        if now - _last_pub_time >= PUB_INTERVAL:
            _last_pub_time = now

            with _ipc_lock:
                light  = _traffic_light_state
                v2v_ok = _v2v_alert_active

            # moving_count: persons whose intent isn't "Standing"/"Observing"
            # (i.e. anyone genuinely walking/approaching/crossing).
            moving_count = 0
            for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
                if class_id != 0:
                    continue
                label, _, _ = analyze_intent(obj_id, tracker.history, class_id)
                if label not in (None, "Standing", "Observing"):
                    moving_count += 1

            crossing_safe = (pedestrian_flag != 2)
            alert = _estimate_alert_level(
                last_stats["persons"], moving_count, crossing_safe, light, v2v_ok
            )

            # 1) pedestrian_status - SAME shape as the original V2P.py
            ipc.publish(TOPIC_PEDESTRIAN_STATUS, {
                "timestamp":        now,
                "pedestrian_count": last_stats["persons"],
                "moving_count":     moving_count,
                "crossing_safe":    crossing_safe,
                "alert_level":      alert,
                "traffic_light":    light,
            })

            # 2) vehicle_data - SAME shape as the original V2P.py
            ipc.publish(TOPIC_VEHICLE_DATA, {
                "timestamp":     now,
                "speed_kmh":     speed_kmh,
                "brake_pressed": False,   # no brake sensor here; STM32 sends this via V2V
                "car_count":     last_stats["cars"],
                "moving_cars":   last_stats["cars"],   # all detected cars assumed moving
                "bike_count":    last_stats["bikes"],
                "ultrasonic": {
                    "front_cm": 999.0    # camera can't give cm; STM32 fills this in
                },
            })

            # 3) v2p_frame - compact pedestrian_flag / position_flag
            v2p_frame_bytes = pack_v2p_frame(
                pedestrian_flag, position_flag if position_flag is not None else 0
            )
            ipc.publish(TOPIC_V2P_FRAME, {
                "timestamp":       now,
                "frame_hex":       v2p_frame_bytes.hex(),
                "pedestrian_flag": pedestrian_flag,
                "position_flag":   position_flag,   # None if nobody is crossing
            })

            # 4) v2n_moto_frame - strict, mostly-silent motorcycle flag
            moto_frame_bytes = pack_moto_frame(motorcycle_alert_flag)
            ipc.publish(TOPIC_V2N_MOTO_FRAME, {
                "timestamp":         now,
                "frame_hex":         moto_frame_bytes.hex(),
                "motorcycle_alert":  motorcycle_alert_flag,
            })

        cv2.imshow("V2P System", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\nDone. Total frames: {frame_count}")
