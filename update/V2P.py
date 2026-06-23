"""
V2P.py - Vehicle-to-Pedestrian System with Full IPC Integration
================================================================
Runs a YOLO ONNX model on PiCamera2 frames to detect persons, bicycles,
cars, and motorcycles.

Features:
- Hybrid centroid tracker (IOU + Distance) for stable IDs.
- Normalized speed detection (fixes the "far away running person" bug).
- Hybrid proximity estimation (Y-axis + Area).
- Pedestrian & motorcycle intent analysis (generalized for any class).
- Traffic light state received live from IPC hub (topic: traffic_light_state).
- V2V alerts received from IPC hub (topic: v2v_alert).
- Motorcycles are completely silent on screen unless:
      crossing_zone AND proximity==DANGER AND risk_level==HIGH
  In that case motorcycle_collision_flag = 1 and a warning is shown.
- Publishes to IPC hub every second:
      pedestrian_status   — ped count, moving count, alert level
      vehicle_data        — car count, bike count, front proximity
      v2p_frame           — pedestrian_flag (0/1/2), position_flag (0-3)
      motorcycle_alert    — motorcycle_collision_flag (0 or 1)
"""

import cv2
import numpy as np
import time
import math
import threading
import onnxruntime as ort
from picamera2 import Picamera2
from collections import deque, defaultdict

from ipc_node import IPCNode

print("=" * 60)
print("V2P SYSTEM - Full IPC Integration")
print("=" * 60)

# ============================================================
# 1. Settings & Configuration
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

# Normalized speed thresholds (speed relative to object size)
SPEED_THRESHOLD_FAST = 0.45
SPEED_THRESHOLD_SLOW = 0.08
CROSSING_ZONE_RATIO  = 0.35   # bottom 35% of frame = crossing zone
APPROACH_FRAMES      = 5

FRAME_AREA = FRAME_W * FRAME_H

# Hybrid proximity thresholds (0.0 to 1.0)
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

# IPC publish interval (seconds)
IPC_PUBLISH_INTERVAL = 1.0

# ============================================================
# 2. IPC Hub Setup
# ============================================================
ipc = IPCNode("v2p_camera")

# Traffic light state — updated by IPC callback from hub
_ipc_lock          = threading.Lock()
CAR_TRAFFIC_LIGHT  = "RED"      # "RED" | "GREEN" | "YELLOW"
PED_TRAFFIC_LIGHT  = "DONT_WALK"
TRANSITION_FLAG    = -1          # 0=G→Y, 1=Y→R, 2=R→Y, 3=Y→G, -1=mid-phase

# V2V alerts — received from hub, forwarded as-is to warning panel
_v2v_alert_text    = ""          # latest alert string from V2V system
_v2v_alert_time    = 0.0         # timestamp of last V2V alert (for expiry)
V2V_ALERT_TTL      = 5.0         # seconds before V2V alert disappears

def _on_traffic_light_state(topic, data, sender):
    """Receive traffic light state from trafic_light.py via hub."""
    global CAR_TRAFFIC_LIGHT, PED_TRAFFIC_LIGHT, TRANSITION_FLAG
    state = data.get("state", "RED")
    tf    = data.get("transition_flag", -1)
    # Derive pedestrian signal from car signal
    ped = "WALK" if state == "GREEN" else "DONT_WALK"
    with _ipc_lock:
        CAR_TRAFFIC_LIGHT = state
        PED_TRAFFIC_LIGHT = ped
        TRANSITION_FLAG   = tf

def _on_v2v_alert(topic, data, sender):
    """Receive V2V danger alerts (EEBL, FCW, BSW, etc.) from hub."""
    global _v2v_alert_text, _v2v_alert_time
    msg = data.get("message", data.get("warning", ""))
    if msg:
        with _ipc_lock:
            _v2v_alert_text = msg
            _v2v_alert_time = time.time()

def connect_ipc():
    """Connect to hub and subscribe. Non-blocking — runs at startup."""
    if not ipc.connect():
        print("[V2P] WARNING: IPC hub not reachable. Running camera-only.")
        return
    ipc.subscribe("traffic_light_state", _on_traffic_light_state)
    ipc.subscribe("v2v_alert",           _on_v2v_alert)
    ipc.start_listening()
    print("[V2P] IPC connected. Subscribed to traffic_light_state + v2v_alert.")

connect_ipc()

# ============================================================
# 3. Centroid Tracker (Hybrid IOU + Distance cost)
# ============================================================
class CentroidTracker:
    def __init__(self, max_disappeared=20, max_distance=80):
        self.next_id      = 0
        self.objects      = {}
        self.objects_bbox = {}
        self.disappeared  = {}
        self.max_dis      = max_disappeared
        self.max_dist     = max_distance
        self.history      = defaultdict(lambda: deque(maxlen=30))

    def register(self, cx, cy, bbox):
        self.objects[self.next_id]      = (cx, cy)
        self.objects_bbox[self.next_id] = bbox
        self.disappeared[self.next_id]  = 0
        self.history[self.next_id].append((cx, cy))
        self.next_id += 1

    def deregister(self, obj_id):
        self.objects.pop(obj_id, None)
        self.objects_bbox.pop(obj_id, None)
        self.disappeared.pop(obj_id, None)

    def compute_iou(self, boxA, boxB):
        xA = max(boxA[0], boxB[0]); yA = max(boxA[1], boxB[1])
        xB = min(boxA[2], boxB[2]); yB = min(boxA[3], boxB[3])
        inter = max(0, xB - xA) * max(0, yB - yA)
        areaA = (boxA[2]-boxA[0]) * (boxA[3]-boxA[1])
        areaB = (boxB[2]-boxB[0]) * (boxB[3]-boxB[1])
        return inter / float(areaA + areaB - inter + 1e-6)

    def update(self, rects):
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
# 4. Intent Analyzer — generalized for ANY class
#    (persons + motorcycles both use this now)
# ============================================================
def analyze_intent(obj_id, history, class_id, bbox):
    """
    Analyze movement intent for any tracked object.

    Returns (label, risk_level, color)
      label      : display string
      risk_level : "HIGH" | "MED" | "LOW"
      color      : BGR tuple

    For non-person, non-motorcycle classes (bicycles, cars) we return
    LOW risk so they don't trigger false alerts.
    """
    # Classes we analyze deeply: person (0) and motorcycle (3)
    if class_id not in (0, 3):
        return "Observing", "LOW", (0, 255, 0)

    pts = list(history[obj_id])
    if len(pts) < 3:
        return "Observing", "LOW", (0, 255, 0)

    recent = pts[-APPROACH_FRAMES:] if len(pts) >= APPROACH_FRAMES else pts
    dists  = [math.hypot(recent[i][0]-recent[i-1][0],
                         recent[i][1]-recent[i-1][1])
              for i in range(1, len(recent))]
    avg_pixel_speed  = float(np.mean(dists)) if dists else 0.0
    avg_size         = max(5, ((bbox[2]-bbox[0]) + (bbox[3]-bbox[1])) / 2.0)
    normalized_speed = avg_pixel_speed / avg_size

    dx = pts[-1][0] - pts[max(0, len(pts)-APPROACH_FRAMES)][0]
    dy = pts[-1][1] - pts[max(0, len(pts)-APPROACH_FRAMES)][1]

    moving_toward_road = dy > 3
    in_road_zone       = pts[-1][1] > FRAME_H * (1 - CROSSING_ZONE_RATIO)
    moving_laterally   = abs(dx) > abs(dy) * 0.8

    # Label prefix by class
    prefix = "MOTO" if class_id == 3 else ""

    if in_road_zone and normalized_speed > SPEED_THRESHOLD_FAST:
        label = f"{prefix} CROSSING FAST" if prefix else "CROSSING FAST"
        return label, "HIGH", (0, 0, 255)
    if in_road_zone and moving_laterally:
        label = f"{prefix} CROSSING" if prefix else "CROSSING"
        return label, "HIGH", (0, 0, 255)
    if moving_toward_road and normalized_speed > SPEED_THRESHOLD_SLOW:
        return "Approaching", "MED", (0, 165, 255)
    if normalized_speed < SPEED_THRESHOLD_SLOW:
        return "Standing", "LOW", (0, 255, 0)
    return "Walking", "LOW", (0, 200, 100)


# ============================================================
# 5. Proximity Estimator (hybrid: vertical position + area)
# ============================================================
def estimate_proximity(x1, y1, x2, y2):
    area_ratio     = (x2-x1)*(y2-y1) / FRAME_AREA
    vertical_ratio = min(1.0, y2 / (FRAME_H * 0.9))
    hybrid         = min(1.0, (vertical_ratio * 0.7) + (area_ratio * 2.5))

    if hybrid >= PROXIMITY_DANGER:
        return hybrid, "DANGER",  "TOO CLOSE!", (0, 0, 255)
    if hybrid >= PROXIMITY_WARNING:
        return hybrid, "WARNING", "CLOSE",      (0, 165, 255)
    return hybrid, "SAFE", "", (0, 220, 0)


# ============================================================
# 6. Warning Deduplication
# ============================================================
def add_warning(warnings_dict, obj_id, key, text, color):
    new_pri = WARN_PRIORITY.get(key, 0)
    if obj_id not in warnings_dict or new_pri > warnings_dict[obj_id][0]:
        warnings_dict[obj_id] = (new_pri, text, color)


# ============================================================
# 7. IPC Frame Helpers
# ============================================================
def compute_pedestrian_flag(person_list):
    """
    0 = no pedestrian risk
    1 = pedestrian nearby (approaching / close)
    2 = pedestrian actively crossing (HIGH risk)

    person_list: list of risk_level strings for all detected persons
    """
    if any(r == "HIGH" for r in person_list):
        return 2
    if any(r == "MED" for r in person_list):
        return 1
    return 0

def compute_position_flag(cx, frame_w):
    """
    0 = LEFT   (cx < 25% of frame)
    1 = CENTER-LEFT  (25-50%)
    2 = CENTER-RIGHT (50-75%)
    3 = RIGHT  (cx > 75%)
    Returns None if cx is None.
    """
    if cx is None:
        return None
    ratio = cx / frame_w
    if ratio < 0.25:  return 0
    if ratio < 0.50:  return 1
    if ratio < 0.75:  return 2
    return 3


# ============================================================
# 8. Radar & Position Label
# ============================================================
RADAR_W      = 160
RADAR_H      = 180
RADAR_MARGIN = 10

def get_position_label(cx, x1, y1, x2, y2):
    frame_cx = FRAME_W // 2
    if cx < frame_cx - FRAME_W // 4:
        h_label = "LEFT"
    elif cx > frame_cx + FRAME_W // 4:
        h_label = "RIGHT"
    else:
        h_label = "FRONT"
    area_ratio     = (x2-x1)*(y2-y1) / FRAME_AREA
    vertical_ratio = min(1.0, y2 / (FRAME_H * 0.9))
    hybrid         = min(1.0, (vertical_ratio * 0.7) + (area_ratio * 2.5))
    d_label = "VERY CLOSE" if hybrid >= PROXIMITY_DANGER else (
              "CLOSE" if hybrid >= PROXIMITY_WARNING else "FAR")
    return f"{h_label} ({d_label})"


def draw_radar(frame, objects):
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
        dc = (0, 0, 255) if cname == "car" else (
             (0, 255, 0) if cname == "person" else (255, 100, 0))
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
# 9. Camera & Model Initialization
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

print(f"\nLoading model: {MODEL_PATH}")
opts = ort.SessionOptions()
opts.intra_op_num_threads = 2
session = ort.InferenceSession(
    MODEL_PATH, sess_options=opts,
    providers=["CPUExecutionProvider"]
)
input_name = session.get_inputs()[0].name
print("Model loaded!")

tracker = CentroidTracker(max_disappeared=25, max_distance=100)

# ============================================================
# 10. Runtime State
# ============================================================
frame_count      = 0
skip_frames      = 2
fps              = 0
fps_counter      = 0
fps_time         = time.time()
last_stats       = {"cars": 0, "persons": 0, "bikes": 0}
last_objects     = {}
last_publish_t   = 0.0

print("\n" + "=" * 60)
print("CONTROLS (override traffic light for testing):")
print("  [R] -> Car RED    | Ped DONT_WALK")
print("  [G] -> Car GREEN  | Ped WALK")
print("  [Y] -> Car AMBER  | Ped DONT_WALK")
print("  [Q] -> Quit")
print("(Normally traffic light comes from IPC hub)")
print("=" * 60 + "\n")

# ============================================================
# 11. Main Loop
# ============================================================
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

        # ── Snapshot IPC-shared state (thread-safe) ──────────────────
        with _ipc_lock:
            car_light  = CAR_TRAFFIC_LIGHT
            ped_light  = PED_TRAFFIC_LIGHT
            v2v_text   = _v2v_alert_text if (time.time() - _v2v_alert_time) < V2V_ALERT_TTL else ""

        # ── Inference ────────────────────────────────────────────────
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
                        x1 = max(0, x1);       y1 = max(0, y1)
                        x2 = min(FRAME_W, x2); y2 = min(FRAME_H, y2)
                        rects.append((x1, y1, x2, y2, raw_classes[i]))

            tracked      = tracker.update(rects)
            last_objects = tracked
            last_stats   = {
                "cars":    sum(1 for v in tracked.values() if v[6] == 2),
                "persons": sum(1 for v in tracked.values() if v[6] == 0),
                "bikes":   sum(1 for v in tracked.values() if v[6] in (1, 3)),
            }

        # ── Drawing & Alerts ─────────────────────────────────────────
        zone_y        = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        warnings_dict = {}

        # Accumulators for IPC publish
        person_risks            = []   # list of risk_level per person
        closest_person_cx       = None
        motorcycle_collision_flag = 0
        front_proximity_ratio   = 0.0  # highest proximity ratio of any car/bike

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            cname = TARGET_CLASSES[class_id]

            # ── Proximity ─────────────────────────────────────────────
            ratio, prox_level, prox_label, prox_color = estimate_proximity(x1, y1, x2, y2)

            # Track front proximity for vehicle_data publish
            if class_id in (2, 3) and ratio > front_proximity_ratio:
                front_proximity_ratio = ratio

            # ── Intent (generalized for person=0 and motorcycle=3) ────
            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id, (x1, y1, x2, y2)
            )

            # ── MOTORCYCLE: strict silence unless collision risk ───────
            if class_id == 3:
                in_crossing_zone = (y1 + y2) // 2 > zone_y
                # collision_flag = 1 only if ALL three conditions met:
                #   1. object center is in crossing zone (bottom 35%)
                #   2. proximity is DANGER
                #   3. intent analysis returns HIGH risk
                if in_crossing_zone and prox_level == "DANGER" and risk_level == "HIGH":
                    motorcycle_collision_flag = 1
                    add_warning(warnings_dict, obj_id, "EMERGENCY",
                                f"!! MOTORCYCLE COLLISION RISK (#{obj_id})", (0, 0, 255))
                    # Draw box only when actual collision risk
                    cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 0, 255), 3)
                    cv2.putText(frame, f"MOTO DANGER #{obj_id}",
                                (x1, max(y1-22, 12)),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 0, 255), 2)
                # Motorcycle is otherwise COMPLETELY silent — no box, no label
                continue   # skip all remaining drawing for this object

            # ── PERSON ────────────────────────────────────────────────
            if class_id == 0:
                person_risks.append(risk_level)
                if risk_level == "HIGH":
                    # Track closest HIGH-risk person for position_flag
                    if closest_person_cx is None or abs(cx - FRAME_W//2) < abs(closest_person_cx - FRAME_W//2):
                        closest_person_cx = cx

            # ── Standard warnings (persons + cars + bicycles) ─────────
            color = (0,255,0) if cname=="person" else (
                    (0,0,255) if cname=="car" else (255,100,0))

            if prox_level == "DANGER":
                color = (0, 0, 255)
                add_warning(warnings_dict, obj_id, "TOO_CLOSE",
                            f"! TOO CLOSE: {cname} #{obj_id} ({ratio*100:.0f}%)", (0, 0, 255))
            elif prox_level == "WARNING":
                if cname != "person":
                    color = (0, 165, 255)
                add_warning(warnings_dict, obj_id, "CLOSE",
                            f"~ CLOSE: {cname} #{obj_id} ({ratio*100:.0f}%)", (0, 165, 255))

            if class_id == 0:
                if risk_level == "HIGH":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! PERSON CROSSING (#{obj_id})", (0, 0, 255))
                elif risk_level == "MED":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Person Approaching (#{obj_id})", (0, 165, 255))

            # ── Traffic light interaction ──────────────────────────────
            hist = list(tracker.history[obj_id])
            is_stationary = False
            if len(hist) >= 5:
                sp_dists = [math.hypot(hist[i][0]-hist[i-1][0],
                                       hist[i][1]-hist[i-1][1])
                            for i in range(1, len(hist))]
                avg_sp = (np.mean(sp_dists[-5:]) if len(sp_dists) >= 5
                          else np.mean(sp_dists) if sp_dists else 999)
                is_stationary = avg_sp < 1.0

            if class_id == 2 and is_stationary and (y1+y2)//2 > zone_y:
                if car_light == "RED":
                    add_warning(warnings_dict, obj_id, "CLOSE",
                                f"! Lead Car at RED. Safe Stop. (#{obj_id})", (0, 0, 255))
                elif car_light == "GREEN":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Car Stopped on GREEN - Check Road! (#{obj_id})", (0, 0, 200))
                elif car_light in ("YELLOW", "AMBER"):
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Car Stopped on AMBER. Prepare. (#{obj_id})", (0, 165, 255))

            if class_id == 0 and ("CROSSING" in str(intent_label) or (y1+y2)//2 > zone_y):
                if ped_light == "WALK":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Pedestrian Legally Crossing. Yield. (#{obj_id})", (0, 165, 255))
                elif ped_light == "DONT_WALK":
                    add_warning(warnings_dict, obj_id, "EMERGENCY",
                                f"!! JAYWALKING ON RED! (#{obj_id})", (0, 0, 255))

            # ── Draw ──────────────────────────────────────────────────
            thickness = 3 if prox_level == "DANGER" or risk_level == "HIGH" else 2
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)
            cv2.putText(frame, f"#{obj_id} {cname}",
                        (x1, max(y1-22, 12)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

            if prox_level != "SAFE":
                cv2.putText(frame, prox_label,
                            (x2+4, (y1+y2)//2),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, prox_color, 2)

            if intent_label and class_id == 0:
                cv2.putText(frame, intent_label,
                            (x1, max(y1-6, 22)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)

            pos_label = get_position_label(cx, x1, y1, x2, y2)
            cv2.putText(frame, pos_label,
                        (x1, min(y2+16, FRAME_H-4)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200, 200, 200), 1)

            # Proximity bar
            bx     = min(x2+2, FRAME_W-8)
            bar_h  = y2 - y1
            fill_h = int(bar_h * min(ratio / PROXIMITY_DANGER, 1.0))
            cv2.rectangle(frame, (bx, y1),        (bx+5, y2),        (80, 80, 80),  -1)
            cv2.rectangle(frame, (bx, y2-fill_h), (bx+5, y2),        prox_color,    -1)

            # Motion trail
            pts_hist = list(tracker.history[obj_id])
            for k in range(1, len(pts_hist)):
                alpha = k / len(pts_hist)
                tc    = tuple(int(c * alpha) for c in color)
                cv2.line(frame, pts_hist[k-1], pts_hist[k], tc, 1)

        # ── Road zone line ────────────────────────────────────────────
        cv2.line(frame, (0, zone_y), (FRAME_W, zone_y), (0, 200, 200), 1)
        cv2.putText(frame, "-- Road Zone --", (5, zone_y-5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 200, 200), 1)

        # ── Radar ─────────────────────────────────────────────────────
        frame = draw_radar(frame, last_objects)

        # ── V2V Alert overlay (top of screen) ─────────────────────────
        if v2v_text:
            cv2.putText(frame, f"V2V: {v2v_text}",
                        (10, FRAME_H - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 2)

        # ── Driver Alerts Panel ───────────────────────────────────────
        warnings_list = [(txt, clr) for (_, txt, clr) in
                         sorted(warnings_dict.values(), key=lambda x: -x[0])]

        if warnings_list:
            panel_h = 30 + len(warnings_list) * 28
            overlay = frame.copy()
            cv2.rectangle(overlay, (0, FRAME_H-panel_h), (FRAME_W, FRAME_H), (20, 20, 20), -1)
            frame = cv2.addWeighted(overlay, 0.6, frame, 0.4, 0)

            if any(c == (0, 0, 255) for (_, c) in warnings_list):
                flash = frame.copy()
                cv2.rectangle(flash, (0, 0), (FRAME_W, FRAME_H), (0, 0, 180), -1)
                frame = cv2.addWeighted(flash, 0.12, frame, 0.88, 0)

            cv2.putText(frame, "DRIVER ALERTS:",
                        (10, FRAME_H-panel_h+20),
                        cv2.FONT_HERSHEY_DUPLEX, 0.55, (255, 255, 255), 1)

            for idx, (warn_text, warn_color) in enumerate(warnings_list):
                yp = FRAME_H - panel_h + 20 + (idx+1) * 28
                cv2.rectangle(frame, (8, yp-14), (18, yp-4), warn_color, -1)
                cv2.putText(frame, warn_text, (24, yp-3),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, warn_color, 2)

        # ── HUD ──────────────────────────────────────────────────────
        cv2.putText(frame, f"Cars: {last_stats['cars']}",
                    (10, 30),  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,   0, 255), 2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}",
                    (10, 55),  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255,   0), 2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",
                    (10, 80),  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 0), 2)
        cv2.putText(frame, f"FPS: {fps}",
                    (10, 105), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        # Traffic light indicator (top-right) — state only, no timer
        tl_color = {"GREEN": (0,200,0), "RED": (0,0,200), "YELLOW": (0,180,180)}.get(car_light, (150,150,150))
        cv2.putText(frame, f"Light: {car_light}",
                    (FRAME_W-160, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, tl_color, 2)

        # ── IPC Publish (every IPC_PUBLISH_INTERVAL seconds) ─────────
        now = time.time()
        if now - last_publish_t >= IPC_PUBLISH_INTERVAL:
            last_publish_t = now

            # pedestrian_flag: 0=none, 1=approaching, 2=crossing
            ped_flag    = compute_pedestrian_flag(person_risks)
            pos_flag    = compute_position_flag(closest_person_cx, FRAME_W)
            moving_peds = sum(1 for r in person_risks if r in ("MED", "HIGH"))
            crossing_ok = (ped_flag == 0)

            # pedestrian_status — for V2P awareness
            ipc.publish("pedestrian_status", {
                "timestamp":        now,
                "pedestrian_count": last_stats["persons"],
                "moving_count":     moving_peds,
                "crossing_safe":    crossing_ok,
                "alert_level":      "danger" if ped_flag == 2 else (
                                    "caution" if ped_flag == 1 else "none"),
                "traffic_light":    car_light,
            })

            # vehicle_data — camera-based sensing
            front_cm = round((1.0 - front_proximity_ratio) * 300, 1)  # rough cm estimate
            ipc.publish("vehicle_data", {
                "timestamp":    now,
                "speed_kmh":    0.0,          # V2P camera can't measure car speed
                "brake_pressed": False,
                "car_count":    last_stats["cars"],
                "moving_cars":  last_stats["cars"],
                "bike_count":   last_stats["bikes"],
                "ultrasonic":   {"front_cm": front_cm},
            })

            # v2p_frame — compact flags for dashboard_bridge
            ipc.publish("v2p_frame", {
                "timestamp":       now,
                "pedestrian_flag": ped_flag,   # 0/1/2
                "position_flag":   pos_flag,   # 0/1/2/3 or None
            })

            # motorcycle_alert — new topic
            ipc.publish("motorcycle_alert", {
                "timestamp":                now,
                "motorcycle_collision_flag": motorcycle_collision_flag,  # 0 or 1
            })

        # ── Console ──────────────────────────────────────────────────
        print(f"\rCars:{last_stats['cars']} | "
              f"Persons:{last_stats['persons']} | "
              f"Bikes:{last_stats['bikes']} | "
              f"FPS:{fps} | "
              f"Light:{car_light}", end="")

        cv2.imshow("V2P System", frame)

        # ── Keyboard (manual override for testing) ────────────────────
        key = cv2.waitKey(1) & 0xFF
        if key == ord('r'):
            with _ipc_lock:
                CAR_TRAFFIC_LIGHT = "RED"
                PED_TRAFFIC_LIGHT = "DONT_WALK"
            print("\n[MANUAL] Car RED | Ped DONT_WALK")
        elif key == ord('g'):
            with _ipc_lock:
                CAR_TRAFFIC_LIGHT = "GREEN"
                PED_TRAFFIC_LIGHT = "WALK"
            print("\n[MANUAL] Car GREEN | Ped WALK")
        elif key == ord('y'):
            with _ipc_lock:
                CAR_TRAFFIC_LIGHT = "YELLOW"
                PED_TRAFFIC_LIGHT = "DONT_WALK"
            print("\n[MANUAL] Car YELLOW | Ped DONT_WALK")
        elif key == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\nDone. Total frames: {frame_count}")
