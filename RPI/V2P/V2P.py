# -*- coding: utf-8 -*-
"""
V2P.py — Vehicle-to-Pedestrian Safety System (Raspberry Pi)
=============================================================

Role in the system
------------------
This process runs the real-time computer-vision pipeline that detects
pedestrians, cyclists, motorcycles, and stationary vehicles in front of
the host vehicle's camera.  It integrates with the V2X hub so it can:

    1. RECEIVE live traffic-light state from the hub  (published by
       Car_client.py after it merges the Gateway packet).
    2. PUBLISH its own detection results to the hub so the dashboard
       and any other subscriber can act on them.

Connections to the IPC Hub
---------------------------
Subscribes:
    "v2n_frame"
        Published by Car_client.py.  V2P reads only:
            traffic_flag  (int, unified encoding: 0=no light, 1=GO, 2=STOP)
        V2P converts this back to a string for its internal logic
        ("GREEN" for GO, "RED" for STOP/no-light — the RED/YELLOW
        distinction from before is no longer available since it's now
        folded into the single STOP value).

Publishes:
    "v2p_frame"
        Emitted once per inference cycle.
        Fields:
            pedestrian_flag : int
                0 = no pedestrian in road zone or clear
                1 = pedestrian detected near crossing zone
                2 = pedestrian actively crossing (HIGH intent)
            position_flag   : int
                0 = no one in a critical zone
                1 = RIGHT zone (384–512 px)
                2 = LEFT  zone (128–256 px)
            lead_car_collision_flag : int
                0 = normal (no risk)
                1 = WARNING — a lead car is stopped in the crossing zone
                    despite the light saying GO (or about to change),
                    at a moderate ("close") distance
                2 = DANGER  — same wrong-stop situation, but "too close" —
                    a rear-end collision is plausible

    "motorcycle_alert"
        Emitted only when a motorcycle is in the crossing zone with
        DANGER proximity AND HIGH intent — the highest-risk scenario.
        Fields:
            motorcycle_collision_flag : int — 0=no risk, 1=high risk

Traffic-light integration
--------------------------
The traffic light state (CAR_TRAFFIC_LIGHT) drives two extra warnings
inside the existing V2I logic block:

    Car stationary at RED  →  "Lead Car at RED Light — Safe Stop"
    Car stationary at GREEN→  "Car Stopped on GREEN — Check Road!"
    Car stationary at AMBER→  "Car Stopped on AMBER — Prepare"
    Pedestrian crossing + WALK       → "Pedestrian Legally Crossing"
    Pedestrian crossing + DONT_WALK  → "JAYWALKING ON RED!"

Startup order
-------------
    1. hub.py              (IPC broker — must be first)
    2. Car_client.py       (publishes v2n_frame with traffic state)
    3. dashboard_bridge.py (bridges hub → data.json)
    4. V2P.py              (this file — subscribes then publishes)
"""

import os
import sys
import cv2
import numpy as np
import time
import math
import threading
import onnxruntime as ort
from picamera2 import Picamera2
from collections import deque, defaultdict

_HERE = os.path.dirname(os.path.abspath(__file__))

# ipc_node lives in the shared hub/ folder
sys.path.insert(0, os.path.join(_HERE, "..", "hub"))
from ipc_node import IPCNode

print("=" * 60)
print("V2P SYSTEM - RASPBERRY PI (Proximity Fix + LEFT/RIGHT Zones)")
print("=" * 60)

# ============================================================
# 1. Settings & Configuration (OPTIMIZED)
# ============================================================
MODEL_PATH       = os.path.join(_HERE, "model2.onnx")
CONF_THRESH      = 0.25          # ★ OPTIMIZED: reduced from 0.30 for better detection
MODEL_INPUT_SIZE = 640
FRAME_W, FRAME_H = 640, 480

TARGET_CLASSES = {
    0: "person",
    1: "bicycle",
    2: "car",
    3: "motorcycle",
}

SPEED_THRESHOLD_FAST = 0.45
SPEED_THRESHOLD_SLOW = 0.08
CROSSING_ZONE_RATIO  = 0.35
APPROACH_FRAMES      = 5

FRAME_AREA = FRAME_W * FRAME_H

# Hybrid proximity thresholds (normalised 0.0–1.0)
PROXIMITY_DANGER  = 0.60
PROXIMITY_WARNING = 0.35
PROXIMITY_SAFE    = 0.15

# Warning priority: higher = shown instead of lower ones
WARN_PRIORITY = {
    "EMERGENCY":  5,
    "CROSSING":   4,
    "TOO_CLOSE":  3,
    "APPROACHING":2,
    "CLOSE":      1,
    "NONE":       0,
}

# ============================================================
# 2. IPC Hub — connect and subscribe
# ============================================================
_ipc = IPCNode("v2p_camera")

# Shared traffic light state (updated by hub callback, read by main loop)
_tl_lock         = threading.Lock()
_CAR_TL_FLAG     = 2          # default STOP (traffic_flag int: 0=no light,1=GO,2=STOP)
_CAR_TL_STR      = "RED"      # human-readable string for V2I logic
_PED_TL_STR      = "DONT_WALK"

# Unified traffic_flag → human string (V2I logic uses "GREEN"/"RED").
# 0 (no light) defaults to the safe/conservative "RED" behaviour, same as
# the initial default above. The old RED-vs-YELLOW distinction is gone
# now that both fold into the single STOP value (2).
_TL_FLAG_TO_STR = {0: "RED", 1: "GREEN", 2: "RED"}
_TL_STR_TO_PED  = {"GREEN": "WALK", "RED": "DONT_WALK"}


def _on_v2n_frame(topic: str, data: dict, sender: str) -> None:
    """
    Receive v2n_frame from Car_client.py.

    Reads traffic_flag and maps it to the CAR/PED string pair used by
    the V2I warning logic.  All other v2n fields are ignored by V2P.
    """
    global _CAR_TL_FLAG, _CAR_TL_STR, _PED_TL_STR
    flag = int(data.get("traffic_flag", 2))   # default RED
    car_str = _TL_FLAG_TO_STR.get(flag, "RED")
    ped_str = _TL_STR_TO_PED.get(car_str, "DONT_WALK")
    with _tl_lock:
        _CAR_TL_FLAG = flag
        _CAR_TL_STR  = car_str
        _PED_TL_STR  = ped_str
    print(f"[V2P] traffic update ← {sender}: car={car_str} ped={ped_str}")


def _connect_hub() -> None:
    """Connect to the IPC hub and subscribe to v2n_frame."""
    if _ipc.connect():
        _ipc.subscribe("v2n_frame", _on_v2n_frame)
        _ipc.start_listening()
        print("[V2P] IPC hub connected — receiving traffic light state.")
    else:
        print("[V2P] WARNING: IPC hub unreachable — traffic state will stay RED.")


# ── Helper: publish v2p_frame ────────────────────────────────────────────────
def _publish_v2p_frame(pedestrian_flag: int, position_flag: int,
                       lead_car_collision_flag: int = 0) -> None:
    """
    Publish pedestrian detection + lead-car-collision results to the hub.

    Parameters
    ----------
    pedestrian_flag : int
        0 = clear, 1 = detected near zone, 2 = actively crossing
    position_flag   : int
        0 = no one in a critical zone, 1 = RIGHT zone, 2 = LEFT zone
    lead_car_collision_flag : int
        0 = normal, 1 = WARNING (wrongly-stopped lead car at moderate
            distance), 2 = DANGER (wrongly-stopped lead car too close)
    """
    _ipc.publish("v2p_frame", {
        "pedestrian_flag":          pedestrian_flag,
        "position_flag":            position_flag,
        "lead_car_collision_flag":  lead_car_collision_flag,
    })


def _publish_motorcycle_alert(collision_flag: int) -> None:
    """
    Publish motorcycle collision alert to the IPC hub.

    Parameters
    ----------
    collision_flag : int
        0 = no risk, 1 = motorcycle in crossing zone + DANGER + HIGH intent
    """
    _ipc.publish("motorcycle_alert", {
        "motorcycle_collision_flag": collision_flag,
    })


# ============================================================
# 3. Centroid Tracker (Hybrid IOU + Distance cost)
# ============================================================
class CentroidTracker:
    """
    Multi-object tracker that assigns persistent IDs to detections.

    Uses a combined IOU + Euclidean-distance cost matrix so IDs stay
    stable even when objects overlap or move quickly.
    """

    def __init__(self, max_disappeared: int = 20, max_distance: int = 80) -> None:
        self.next_id      = 0
        self.objects      = {}          # id → (cx, cy)
        self.objects_bbox = {}          # id → (x1, y1, x2, y2)
        self.disappeared  = {}
        self.max_dis      = max_disappeared
        self.max_dist     = max_distance
        self.history      = defaultdict(lambda: deque(maxlen=30))

    def register(self, cx: int, cy: int, bbox: tuple) -> None:
        self.objects[self.next_id]      = (cx, cy)
        self.objects_bbox[self.next_id] = bbox
        self.disappeared[self.next_id]  = 0
        self.history[self.next_id].append((cx, cy))
        self.next_id += 1

    def deregister(self, obj_id: int) -> None:
        self.objects.pop(obj_id, None)
        self.objects_bbox.pop(obj_id, None)
        self.disappeared.pop(obj_id, None)
        # history is keyed by the same ids and has no other cleanup path — drop it too,
        # otherwise every object that ever crossed the camera leaks a deque forever.
        self.history.pop(obj_id, None)

    def compute_iou(self, boxA: tuple, boxB: tuple) -> float:
        xA = max(boxA[0], boxB[0]);  yA = max(boxA[1], boxB[1])
        xB = min(boxA[2], boxB[2]);  yB = min(boxA[3], boxB[3])
        inter = max(0, xB - xA) * max(0, yB - yA)
        areaA = (boxA[2]-boxA[0]) * (boxA[3]-boxA[1])
        areaB = (boxB[2]-boxB[0]) * (boxB[3]-boxB[1])
        return inter / float(areaA + areaB - inter + 1e-6)

    def update(self, rects: list) -> dict:
        """
        Update tracker with a new list of bounding boxes.

        Parameters
        ----------
        rects : list of (x1, y1, x2, y2, class_id)

        Returns
        -------
        dict : {obj_id: (cx, cy, x1, y1, x2, y2, class_id)}
        """
        if len(rects) == 0:
            for obj_id in list(self.disappeared):
                self.disappeared[obj_id] += 1
                if self.disappeared[obj_id] > self.max_dis:
                    self.deregister(obj_id)
            return {}

        input_centroids = []
        input_bboxes    = []
        for (x1, y1, x2, y2, _) in rects:
            input_centroids.append((int((x1+x2)/2), int((y1+y2)/2)))
            input_bboxes.append((x1, y1, x2, y2))

        if len(self.objects) == 0:
            for i, (cx, cy) in enumerate(input_centroids):
                self.register(cx, cy, input_bboxes[i])
        else:
            obj_ids    = list(self.objects.keys())
            obj_cents  = list(self.objects.values())
            obj_bboxes = [self.objects_bbox[oid] for oid in obj_ids]

            D = np.zeros((len(obj_cents), len(input_centroids)))
            for r, (ox, oy) in enumerate(obj_cents):
                for c, (ix, iy) in enumerate(input_centroids):
                    dist = np.sqrt((ox-ix)**2 + (oy-iy)**2)
                    iou  = self.compute_iou(obj_bboxes[r], input_bboxes[c])
                    D[r, c] = dist * (1.0 - iou * 0.6)

            rows = D.min(axis=1).argsort()
            cols = D.argmin(axis=1)[rows]
            used_rows, used_cols = set(), set()

            for row, col in zip(rows, cols):
                if row in used_rows or col in used_cols:
                    continue
                if D[row, col] > self.max_dist * 1.5:
                    continue
                obj_id = obj_ids[row]
                cx, cy = input_centroids[col]
                self.objects[obj_id]      = (cx, cy)
                self.objects_bbox[obj_id] = input_bboxes[col]
                self.disappeared[obj_id]  = 0
                self.history[obj_id].append((cx, cy))
                used_rows.add(row)
                used_cols.add(col)

            for col in range(len(input_centroids)):
                if col not in used_cols:
                    self.register(*input_centroids[col], input_bboxes[col])

            for row in range(len(obj_cents)):
                if row not in used_rows:
                    obj_id = obj_ids[row]
                    self.disappeared[obj_id] += 1
                    if self.disappeared[obj_id] > self.max_dis:
                        self.deregister(obj_id)

        result = {}
        for i, (x1, y1, x2, y2, class_id) in enumerate(rects):
            cx, cy = int((x1+x2)/2), int((y1+y2)/2)
            for obj_id, (ox, oy) in self.objects.items():
                if ox == cx and oy == cy:
                    result[obj_id] = (cx, cy, x1, y1, x2, y2, class_id)
                    break
        return result


# ============================================================
# 4. Intent Analyzer (normalised speed, persons only)
# ============================================================
def analyze_intent(obj_id: int, history: dict, class_id: int,
                   bbox: tuple) -> tuple:
    """
    Determine a pedestrian's crossing intent from their motion history.

    Returns
    -------
    (intent_label, risk_level, draw_color)
        intent_label : str   — human description
        risk_level   : str   — "HIGH" | "MED" | "LOW"
        draw_color   : tuple — BGR colour for OpenCV drawing
    """
    if class_id != 0:
        return None, "LOW", (0, 255, 0)

    pts = list(history[obj_id])
    if len(pts) < 3:
        return "Observing", "LOW", (0, 255, 0)

    recent = pts[-APPROACH_FRAMES:] if len(pts) >= APPROACH_FRAMES else pts
    dists  = [math.hypot(recent[i][0]-recent[i-1][0],
                         recent[i][1]-recent[i-1][1])
              for i in range(1, len(recent))]
    avg_pixel_speed = float(np.mean(dists)) if dists else 0.0

    avg_size         = max(5, ((bbox[2]-bbox[0]) + (bbox[3]-bbox[1])) / 2.0)
    normalized_speed = avg_pixel_speed / avg_size

    dx = pts[-1][0] - pts[max(0, len(pts)-APPROACH_FRAMES)][0]
    dy = pts[-1][1] - pts[max(0, len(pts)-APPROACH_FRAMES)][1]

    moving_toward_road = dy > 3
    in_road_zone       = pts[-1][1] > FRAME_H * (1 - CROSSING_ZONE_RATIO)
    moving_laterally   = abs(dx) > abs(dy) * 0.8

    if in_road_zone and normalized_speed > SPEED_THRESHOLD_FAST:
        return "CROSSING FAST", "HIGH", (0, 0, 255)
    if in_road_zone and moving_laterally:
        return "CROSSING",      "HIGH", (0, 0, 255)
    if moving_toward_road and normalized_speed > SPEED_THRESHOLD_SLOW:
        return "Approaching",   "MED",  (0, 165, 255)
    if normalized_speed < SPEED_THRESHOLD_SLOW:
        return "Standing",      "LOW",  (0, 255, 0)
    return "Walking",           "LOW",  (0, 200, 100)


# ============================================================
# 5. Proximity Estimator (hybrid: vertical position + area)
# ============================================================
def estimate_proximity(x1: int, y1: int, x2: int, y2: int) -> tuple:
    """
    Estimate how close an object is using a hybrid of bounding-box
    vertical position and area ratio.

    Returns
    -------
    (hybrid_ratio, level, label, draw_color)
        hybrid_ratio : float  — 0.0 (far) to 1.0+ (very close)
        level        : str    — "DANGER" | "WARNING" | "SAFE"
        label        : str    — short warning text for overlay
        draw_color   : tuple  — BGR colour
    """
    area_ratio     = (x2-x1)*(y2-y1) / FRAME_AREA
    vertical_ratio = min(1.0, y2 / (FRAME_H * 0.9))
    hybrid         = min(1.0, (vertical_ratio * 0.7) + (area_ratio * 2.5))

    if hybrid >= PROXIMITY_DANGER:
        return hybrid, "DANGER",  "TOO CLOSE!", (0, 0, 255)
    if hybrid >= PROXIMITY_WARNING:
        return hybrid, "WARNING", "CLOSE",      (0, 165, 255)
    return hybrid, "SAFE", "", (0, 220, 0)


# ============================================================
# 5b. Distance Estimator (metres — pinhole camera approximation)
# ============================================================
FOCAL_PX = 600.0   # calibrate after physical testing

REAL_HEIGHT_M = {
    0: 1.70,   # person
    1: 1.10,   # bicycle
    2: 1.50,   # car
    3: 1.20,   # motorcycle
}

def estimate_distance_meters(class_id: int, y1: int, y2: int):
    """Return estimated distance in metres using pinhole projection, or None."""
    px_height = y2 - y1
    if px_height < 10:
        return None
    real_h = REAL_HEIGHT_M.get(class_id, 1.70)
    return round((real_h * FOCAL_PX) / px_height, 1)


# ============================================================
# 6. Warning Deduplication (UPDATED to support position label)
# ============================================================
def add_warning(warnings_dict: dict, obj_id: int, key: str,
                text: str, color: tuple, pos_label: str = None) -> None:
    """
    Keep only the highest-priority warning per tracked object ID.
    If pos_label is provided, it is appended to the warning text.
    """
    new_pri = WARN_PRIORITY.get(key, 0)
    
    # Append position label to text if provided and not None
    if pos_label:
        text = f"{text} [{pos_label}]"
        
    if obj_id not in warnings_dict or new_pri > warnings_dict[obj_id][0]:
        warnings_dict[obj_id] = (new_pri, text, color)


# ============================================================
# 7. Position Flag Helper
# ============================================================
def get_position_flag(cx: int) -> int:
    """
    Map the object's centroid x-coordinate to a position_flag.

    Zones (640 px wide frame divided into 5 equal sections of 128 px):
        0–128      : far left  (ignored)
        128–256    : LEFT zone   → flag 2
        256–384    : centre     (ignored)
        384–512    : RIGHT zone  → flag 1
        512–640    : far right  (ignored)

    Returns 0 (no one / not in a critical zone), 1 (RIGHT), or 2 (LEFT).
    """
    if 128 <= cx < 256:
        return 2    # LEFT
    if 384 <= cx < 512:
        return 1    # RIGHT
    return 0        # no one in a critical zone


def get_position_label(cx: int, x1: int, y1: int,
                       x2: int, y2: int, class_id: int) -> str | None:
    """Return a display string like "LEFT  ~3.2m" or None for irrelevant zones."""
    flag = get_position_flag(cx)
    if flag == 0:
        return None
    h_label  = "LEFT" if flag == 2 else "RIGHT"
    dist_m   = estimate_distance_meters(class_id, y1, y2)
    d_label  = (f"~{dist_m}m" if dist_m is not None
                else ("VERY CLOSE" if estimate_proximity(x1,y1,x2,y2)[1]=="DANGER"
                      else "CLOSE"))
    return f"{h_label}  {d_label}"


# ============================================================
# 7b. Radar Drawing
# ============================================================
RADAR_W      = 160
RADAR_H      = 180
RADAR_MARGIN = 10

def draw_radar(frame: np.ndarray, objects: dict) -> np.ndarray:
    """Draw a top-down radar overlay in the top-right corner of the frame."""
    rx = FRAME_W - RADAR_W - RADAR_MARGIN
    ry = RADAR_MARGIN

    overlay = frame.copy()
    cv2.rectangle(overlay, (rx, ry), (rx+RADAR_W, ry+RADAR_H), (20, 20, 20), -1)
    frame[:] = cv2.addWeighted(overlay, 0.75, frame, 0.25, 0)
    cv2.rectangle(frame, (rx, ry), (rx+RADAR_W, ry+RADAR_H), (100, 100, 100), 1)
    cv2.putText(frame, "RADAR", (rx+RADAR_W//2-22, ry+14),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200, 200, 200), 1)

    radar_cx = rx + RADAR_W // 2
    for zf in (0.35, 0.65):
        zy = int(ry + RADAR_H * zf)
        cv2.line(frame, (rx, zy), (rx+RADAR_W, zy), (50, 50, 50), 1)
    cv2.line(frame, (radar_cx, ry+18), (radar_cx, ry+RADAR_H-20), (50, 50, 50), 1)

    car_y = ry + RADAR_H - 15
    cv2.rectangle(frame, (radar_cx-8, car_y-10), (radar_cx+8, car_y+5), (0, 200, 255), -1)
    cv2.putText(frame, "YOU", (radar_cx-10, car_y+16),
                cv2.FONT_HERSHEY_SIMPLEX, 0.3, (0, 200, 255), 1)

    for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in objects.items():
        cname = TARGET_CLASSES[class_id]
        dc = ((0, 0, 255) if cname == "car"
              else (0, 255, 0) if cname == "person"
              else (255, 100, 0))

        dot_rx = max(rx+5, min(rx+RADAR_W-5, int(rx + (cx/FRAME_W)*RADAR_W)))
        area_ratio     = (x2-x1)*(y2-y1) / FRAME_AREA
        vertical_ratio = min(1.0, y2 / (FRAME_H * 0.9))
        hybrid         = min(1.0, (vertical_ratio*0.7) + (area_ratio*2.5))
        nd             = min(hybrid / PROXIMITY_DANGER, 1.0)
        dot_ry         = max(ry+20, min(ry+RADAR_H-25,
                             int((ry+20) + (1.0-nd)*(RADAR_H-50))))

        cv2.circle(frame, (dot_rx, dot_ry), 5, dc, -1)
        cv2.putText(frame, f"{cname[0].upper()}{obj_id}",
                    (dot_rx+6, dot_ry+4), cv2.FONT_HERSHEY_SIMPLEX, 0.3, dc, 1)

    return frame


# ============================================================
# 8. Camera & Model Initialisation (OPTIMIZED)
# ============================================================
_connect_hub()   # connect to IPC hub before starting the camera

print("\nOpening camera …")
picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"format": "RGB888", "size": (FRAME_W, FRAME_H)},
    controls={"FrameRate": 30}
)
picam2.configure(config)
picam2.start()
time.sleep(1)
print("Camera ready!")

print(f"\nLoading ONNX model: {MODEL_PATH}")
# The model is a large binary that is easy to lose (a 0-byte placeholder is committed
# in git). Fail loudly and clearly instead of with a cryptic protobuf error.
if not os.path.exists(MODEL_PATH) or os.path.getsize(MODEL_PATH) == 0:
    sys.exit(f"[V2P] FATAL: {MODEL_PATH} is missing or empty — copy the real model first.")
opts = ort.SessionOptions()
# ★ OPTIMIZED: use 4 threads instead of 2 for better performance on Pi 5
opts.intra_op_num_threads = 4
session = ort.InferenceSession(
    MODEL_PATH, sess_options=opts,
    providers=["CPUExecutionProvider"]
)
input_name = session.get_inputs()[0].name
print("Model loaded!")

tracker = CentroidTracker(max_disappeared=25, max_distance=100)

# ============================================================
# 9. Runtime State
# ============================================================
frame_count  = 0
# Infer at ~10 Hz and draw at full FPS (the overlay reuses last_objects between
# inferences). Running the detector on every frame makes the Pi inference-bound —
# real FPS drops and the intent analyzer sees jerky, unevenly-spaced motion.
# Tune with the on-screen FPS counter.
skip_frames  = 3
fps          = 0
fps_counter  = 0
fps_time     = time.time()
last_stats   = {"cars": 0, "persons": 0, "bikes": 0}
last_objects = {}

print("\n" + "=" * 60)
print("CONTROLS (keyboard simulation for traffic light):")
print("  [R] → Car RED    | Ped DONT_WALK")
print("  [G] → Car GREEN  | Ped WALK")
print("  [Y] → Car AMBER  | Ped DONT_WALK")
print("  [Q] → Quit")
print("=" * 60 + "\n")

# ============================================================
# 10. Main Loop (UPDATED with LEFT/RIGHT in terminal)
# ============================================================
_prev_moto_flag = 0    # track previous motorcycle flag to avoid redundant publishes
_prev_v2p_frame = None  # track previous (ped, pos, lead) tuple — publish only on change

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

        # Read current traffic state (thread-safe snapshot)
        with _tl_lock:
            CAR_TRAFFIC_LIGHT = _CAR_TL_STR
            PED_TRAFFIC_LIGHT = _PED_TL_STR

        # ── Inference (every skip_frames) ────────────────────────────
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
                cs    = p[4:]
                cid   = int(np.argmax(cs))
                score = float(cs[cid])
                if score > CONF_THRESH and cid in TARGET_CLASSES:
                    cx, cy, wb, hb = p[0:4]
                    x1 = int((cx - wb/2) * scale_x)
                    y1 = int((cy - hb/2) * scale_y)
                    x2 = int((cx + wb/2) * scale_x)
                    y2 = int((cy + hb/2) * scale_y)
                    raw_boxes.append([x1, y1, x2-x1, y2-y1])
                    raw_scores.append(score)
                    raw_classes.append(cid)

            rects = []
            if raw_boxes:
                indices = cv2.dnn.NMSBoxes(raw_boxes, raw_scores, CONF_THRESH, 0.4)
                if len(indices) > 0:
                    for i in indices.flatten():
                        x1, y1, wb, hb = raw_boxes[i]
                        x2, y2 = x1+wb, y1+hb
                        x1 = max(0, x1);        y1 = max(0, y1)
                        x2 = min(FRAME_W, x2);  y2 = min(FRAME_H, y2)
                        rects.append((x1, y1, x2, y2, raw_classes[i]))

            tracked      = tracker.update(rects)
            last_objects = tracked
            last_stats   = {
                "cars":    sum(1 for v in tracked.values() if v[6] == 2),
                "persons": sum(1 for v in tracked.values() if v[6] == 0),
                "bikes":   sum(1 for v in tracked.values() if v[6] in (1, 3)),
            }

        # ── Drawing, alerts, and hub publish ─────────────────────────
        zone_y        = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        warnings_dict = {}

        # Accumulators for hub publishing (reset each frame)
        frame_ped_flag       = 0      # highest pedestrian flag seen this frame
        frame_pos_flag       = 0      # position flag of highest-risk pedestrian
        frame_moto_flag      = 0      # motorcycle collision flag
        frame_lead_car_flag  = 0      # wrongly-stopped lead car collision flag

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            cname = TARGET_CLASSES[class_id]
            color = (0,255,0) if cname=="person" else ((0,0,255) if cname=="car"
                    else (255,100,0))
            
            # ── Compute position label early for all objects ─────────
            pos_label = get_position_label(cx, x1, y1, x2, y2, class_id)

            # ── Proximity ────────────────────────────────────────────
            ratio, prox_level, prox_label, prox_color = estimate_proximity(
                x1, y1, x2, y2)
            dist_m   = estimate_distance_meters(class_id, y1, y2)
            dist_str = f"~{dist_m}m" if dist_m is not None else f"{ratio*100:.1f}%"

            if prox_level == "DANGER":
                color = (0, 0, 255)
                add_warning(warnings_dict, obj_id, "TOO_CLOSE",
                            f"! TOO CLOSE: {cname} #{obj_id} {dist_str}", (0,0,255), pos_label)
            elif prox_level == "WARNING":
                if cname != "person":
                    color = (0, 165, 255)
                add_warning(warnings_dict, obj_id, "CLOSE",
                            f"~ CLOSE: {cname} #{obj_id} {dist_str}", (0,165,255), pos_label)

            # ── Intent (persons only) ─────────────────────────────────
            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id, (x1, y1, x2, y2))

            if class_id == 0:   # person
                in_zone = (y1+y2)//2 > zone_y
                # Compute pedestrian_flag
                if "CROSSING" in str(intent_label) or in_zone:
                    ped_flag_this = 2
                elif risk_level in ("MED", "HIGH"):
                    ped_flag_this = 1
                else:
                    ped_flag_this = 0

                if ped_flag_this > frame_ped_flag:
                    frame_ped_flag = ped_flag_this
                    frame_pos_flag = get_position_flag(cx)

                if risk_level == "HIGH":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! PERSON CROSSING (#{obj_id})", (0,0,255), pos_label)
                elif risk_level == "MED":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Person Approaching (#{obj_id})", (0,165,255), pos_label)

            # ── Motorcycle collision check ────────────────────────────
            if class_id == 3:   # motorcycle
                in_zone = (y1+y2)//2 > zone_y
                if in_zone and prox_level == "DANGER":
                    # A motorcycle in the crossing zone at DANGER proximity is the risk
                    # scenario. Base it on motion the bike can actually have (tracker
                    # speed); the old `risk_level == "HIGH"` check was dead code because
                    # analyze_intent() returns "LOW" for every non-person class.
                    pts = list(tracker.history[obj_id])
                    moving = False
                    if len(pts) >= 3:
                        seg = [math.hypot(pts[i][0]-pts[i-1][0], pts[i][1]-pts[i-1][1])
                               for i in range(1, len(pts))]
                        moving = (sum(seg[-3:]) / min(3, len(seg))) > 1.0
                    if moving:
                        frame_moto_flag = 1

            # ── V2I Traffic Light Logic ──────────────────────────────
            hist = list(tracker.history[obj_id])
            is_stationary = False
            if len(hist) >= 5:
                sp_dists = [math.hypot(hist[i][0]-hist[i-1][0],
                                       hist[i][1]-hist[i-1][1])
                            for i in range(1, len(hist))]
                avg_recent = (np.mean(sp_dists[-5:]) if len(sp_dists) >= 5
                              else np.mean(sp_dists) if sp_dists else 999)
                if avg_recent < 1.0:
                    is_stationary = True

            if class_id == 2 and is_stationary and (y1+y2)//2 > zone_y:
                if CAR_TRAFFIC_LIGHT == "RED":
                    add_warning(warnings_dict, obj_id, "CLOSE",
                                f"! Lead Car at RED Light. Safe Stop. (#{obj_id})", (0,0,255), pos_label)
                elif CAR_TRAFFIC_LIGHT == "GREEN":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Car Stopped on GREEN - Check Road! (#{obj_id})", (0,0,200), pos_label)
                    # Stopped wrong (light says GO) — grade the risk by how
                    # close it is: DANGER → 2, WARNING → 1, else stays 0.
                    if prox_level == "DANGER":
                        lead_car_this = 2
                    elif prox_level == "WARNING":
                        lead_car_this = 1
                    else:
                        lead_car_this = 0
                    if lead_car_this > frame_lead_car_flag:
                        frame_lead_car_flag = lead_car_this
                elif CAR_TRAFFIC_LIGHT in ("AMBER", "YELLOW"):
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Car Stopped on AMBER. Prepare. (#{obj_id})", (0,165,255), pos_label)
                    if prox_level == "DANGER":
                        lead_car_this = 2
                    elif prox_level == "WARNING":
                        lead_car_this = 1
                    else:
                        lead_car_this = 0
                    if lead_car_this > frame_lead_car_flag:
                        frame_lead_car_flag = lead_car_this

            if class_id == 0 and ("CROSSING" in str(intent_label)
                                  or (y1+y2)//2 > zone_y):
                if PED_TRAFFIC_LIGHT == "WALK":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Pedestrian Legally Crossing. Yield. (#{obj_id})",
                                (0,165,255), pos_label)
                elif PED_TRAFFIC_LIGHT == "DONT_WALK":
                    add_warning(warnings_dict, obj_id, "EMERGENCY",
                                f"!! JAYWALKING ON RED! (#{obj_id})", (0,0,255), pos_label)

            # ── Draw bounding box and overlays ───────────────────────
            thickness = 3 if prox_level == "DANGER" or risk_level == "HIGH" else 2
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)
            cv2.putText(frame, f"#{obj_id} {cname}",
                        (x1, max(y1-22, 12)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

            if prox_level != "SAFE":
                cv2.putText(frame, prox_label,
                            (x2+4, (y1+y2)//2),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, prox_color, 2)

            if intent_label:
                cv2.putText(frame, intent_label,
                            (x1, max(y1-6, 22)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)

            # Draw position label on frame if exists
            if pos_label is not None:
                cv2.putText(frame, pos_label,
                            (x1, min(y2+16, FRAME_H-4)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200,200,200), 1)

            # Proximity bar
            bx     = min(x2+2, FRAME_W-8)
            bar_h  = y2 - y1
            fill_h = int(bar_h * min(ratio / PROXIMITY_DANGER, 1.0))
            cv2.rectangle(frame, (bx, y1),        (bx+5, y2),        (80,80,80), -1)
            cv2.rectangle(frame, (bx, y2-fill_h), (bx+5, y2),        prox_color, -1)

            # Motion trail
            pts_hist = list(tracker.history[obj_id])
            for k in range(1, len(pts_hist)):
                alpha = k / len(pts_hist)
                tc    = tuple(int(c * alpha) for c in color)
                cv2.line(frame, pts_hist[k-1], pts_hist[k], tc, 1)

        # ── Publish to IPC hub (only when a flag actually changed) ──
        # These flags change a few times per minute, not ~30×/second. Publishing every
        # frame floods the hub and makes dashboard_bridge rewrite data.json to the SD
        # card at frame rate. Send deltas only.
        _v2p_now = (frame_ped_flag, frame_pos_flag, frame_lead_car_flag)
        if _v2p_now != _prev_v2p_frame:
            _prev_v2p_frame = _v2p_now
            _publish_v2p_frame(*_v2p_now)

        if frame_moto_flag != _prev_moto_flag:
            _publish_motorcycle_alert(frame_moto_flag)
            _prev_moto_flag = frame_moto_flag

        # ── Road zone line ───────────────────────────────────────────
        cv2.line(frame, (0, zone_y), (FRAME_W, zone_y), (0,200,200), 1)
        cv2.putText(frame, "-- Road Zone --", (5, zone_y-5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0,200,200), 1)

        # ── Radar overlay ────────────────────────────────────────────
        frame = draw_radar(frame, last_objects)

        # ── Driver alerts panel ──────────────────────────────────────
        warnings_list = [(txt, clr) for (_, txt, clr) in
                         sorted(warnings_dict.values(), key=lambda x: -x[0])]

        if warnings_list:
            panel_h = 30 + len(warnings_list) * 28
            overlay = frame.copy()
            cv2.rectangle(overlay, (0, FRAME_H-panel_h), (FRAME_W, FRAME_H),
                          (20,20,20), -1)
            frame = cv2.addWeighted(overlay, 0.6, frame, 0.4, 0)

            if any(c == (0,0,255) for (_, c) in warnings_list):
                flash = frame.copy()
                cv2.rectangle(flash, (0,0), (FRAME_W,FRAME_H), (0,0,180), -1)
                frame = cv2.addWeighted(flash, 0.12, frame, 0.88, 0)

            cv2.putText(frame, "DRIVER ALERTS:",
                        (10, FRAME_H-panel_h+20),
                        cv2.FONT_HERSHEY_DUPLEX, 0.55, (255,255,255), 1)

            for idx, (warn_text, warn_color) in enumerate(warnings_list):
                yp = FRAME_H - panel_h + 20 + (idx+1) * 28
                cv2.rectangle(frame, (8, yp-14), (18, yp-4), warn_color, -1)
                cv2.putText(frame, warn_text, (24, yp-3),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, warn_color, 2)

        # ── HUD ──────────────────────────────────────────────────────
        cv2.putText(frame, f"Cars: {last_stats['cars']}",       (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,0,255),   2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}", (10, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,255,0),   2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",     (10, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,100,0), 2)
        cv2.putText(frame, f"FPS: {fps}",                       (10, 105),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,255),2)

        cv2.putText(frame, f"Car Signal: {CAR_TRAFFIC_LIGHT}", (FRAME_W-200, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,0), 2)
        cv2.putText(frame, f"Ped Signal: {PED_TRAFFIC_LIGHT}", (FRAME_W-200, 52),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,0), 2)

        # Terminal output now includes LEFT/RIGHT via the warnings list
        print(f"\rCars:{last_stats['cars']} | "
              f"Persons:{last_stats['persons']} | "
              f"Bikes:{last_stats['bikes']} | "
              f"FPS:{fps} | "
              f"TL={CAR_TRAFFIC_LIGHT}", end="")

        cv2.imshow("V2P System - Raspberry Pi", frame)

        # Keyboard simulation (useful when hub is not available for testing)
        key = cv2.waitKey(1) & 0xFF
        if key == ord('r'):
            with _tl_lock:
                _CAR_TL_STR = "RED";    _PED_TL_STR = "DONT_WALK"
            print("\n[SIM] Car RED  | Ped DONT_WALK")
        elif key == ord('g'):
            with _tl_lock:
                _CAR_TL_STR = "GREEN";  _PED_TL_STR = "WALK"
            print("\n[SIM] Car GREEN | Ped WALK")
        elif key == ord('y'):
            with _tl_lock:
                _CAR_TL_STR = "AMBER";  _PED_TL_STR = "DONT_WALK"
            print("\n[SIM] Car AMBER | Ped DONT_WALK")
        elif key == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    _publish_v2p_frame(0, 0, 0)          # clear hub state on exit
    _publish_motorcycle_alert(0)
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\nDone. Total frames: {frame_count}")