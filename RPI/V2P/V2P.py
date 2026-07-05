# -*- coding: utf-8 -*-
"""
V2P.py — Vehicle-to-Pedestrian Safety System (Raspberry Pi)
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
sys.path.insert(0, os.path.join(_HERE, "..", "hub"))
from ipc_node import IPCNode

print("=" * 60)
print("V2P SYSTEM - RASPBERRY PI (FIXED)")
print("=" * 60)

# ============================================================
# 1. Configuration
# ============================================================
MODEL_PATH       = os.path.join(_HERE, "model2.onnx")
CONF_THRESH      = 0.40
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
FRAME_AREA           = FRAME_W * FRAME_H

PROXIMITY_DANGER  = 0.60
PROXIMITY_WARNING = 0.35
PROXIMITY_SAFE    = 0.15

WARN_PRIORITY = {
    "EMERGENCY":   5,
    "CROSSING":    4,
    "TOO_CLOSE":   3,
    "APPROACHING": 2,
    "CLOSE":       1,
    "NONE":        0,
}

FOCAL_PX = 600.0
REAL_HEIGHT_M = {0: 1.70, 1: 1.10, 2: 1.50, 3: 1.20}

RADAR_W      = 160
RADAR_H      = 180
RADAR_MARGIN = 10

# ============================================================
# 2. IPC Hub
# ============================================================
_ipc        = IPCNode("v2p_camera")
_tl_lock    = threading.Lock()
_CAR_TL_STR = "RED"
_PED_TL_STR = "DONT_WALK"

_TL_FLAG_TO_STR = {0: "RED", 1: "GREEN", 2: "RED"}
_TL_STR_TO_PED  = {"GREEN": "WALK", "RED": "DONT_WALK"}


def _on_v2n_frame(topic, data, sender):
    global _CAR_TL_STR, _PED_TL_STR
    flag    = int(data.get("traffic_flag", 2))
    car_str = _TL_FLAG_TO_STR.get(flag, "RED")
    ped_str = _TL_STR_TO_PED.get(car_str, "DONT_WALK")
    with _tl_lock:
        _CAR_TL_STR = car_str
        _PED_TL_STR = ped_str
    print(f"[V2P] traffic update <- {sender}: car={car_str} ped={ped_str}")


def _connect_hub():
    if _ipc.connect():
        _ipc.subscribe("v2n_frame", _on_v2n_frame)
        _ipc.start_listening()
        print("[V2P] IPC hub connected.")
    else:
        print("[V2P] WARNING: IPC hub unreachable — traffic state stays RED.")


def _publish_v2p_frame(ped_flag, pos_flag, lead_car_flag=0):
    _ipc.publish("v2p_frame", {
        "pedestrian_flag":        ped_flag,
        "position_flag":          pos_flag,
        "lead_car_collision_flag": lead_car_flag,
    })


def _publish_motorcycle_alert(flag):
    _ipc.publish("motorcycle_alert", {"motorcycle_collision_flag": flag})


# ============================================================
# 3. Centroid Tracker
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
# 4. Intent Analyzer
# ============================================================
def analyze_intent(obj_id, history, class_id, bbox):
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
# 5. Proximity & Distance
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


def estimate_distance_meters(class_id, y1, y2):
    px_h = y2 - y1
    if px_h < 10:
        return None
    return round((REAL_HEIGHT_M.get(class_id, 1.70) * FOCAL_PX) / px_h, 1)


# ============================================================
# 6. Warning Helper
# ============================================================
def add_warning(warnings_dict, obj_id, key, text, color, pos_label=None):
    new_pri = WARN_PRIORITY.get(key, 0)
    if pos_label:
        text = f"{text} [{pos_label}]"
    if obj_id not in warnings_dict or new_pri > warnings_dict[obj_id][0]:
        warnings_dict[obj_id] = (new_pri, text, color)


# ============================================================
# 7. Position Label & Radar
# ============================================================
def get_position_flag(cx):
    if 128 <= cx < 256:
        return 2   # LEFT
    if 384 <= cx < 512:
        return 1   # RIGHT
    return 0


def get_position_label(cx, x1, y1, x2, y2, class_id):
    flag = get_position_flag(cx)
    if flag == 0:
        return None
    h_label = "LEFT" if flag == 2 else "RIGHT"
    dist_m  = estimate_distance_meters(class_id, y1, y2)
    d_label = (f"~{dist_m}m" if dist_m is not None
               else ("VERY CLOSE" if estimate_proximity(x1,y1,x2,y2)[1] == "DANGER"
                     else "CLOSE"))
    return f"{h_label}  {d_label}"


def draw_radar(frame, objects):
    rx = FRAME_W - RADAR_W - RADAR_MARGIN
    ry = RADAR_MARGIN
    overlay = frame.copy()
    cv2.rectangle(overlay, (rx, ry), (rx+RADAR_W, ry+RADAR_H), (20,20,20), -1)
    frame[:] = cv2.addWeighted(overlay, 0.75, frame, 0.25, 0)
    cv2.rectangle(frame, (rx, ry), (rx+RADAR_W, ry+RADAR_H), (100,100,100), 1)
    cv2.putText(frame, "RADAR", (rx+RADAR_W//2-22, ry+14),
                cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200,200,200), 1)
    radar_cx = rx + RADAR_W // 2
    for zf in (0.35, 0.65):
        zy = int(ry + RADAR_H * zf)
        cv2.line(frame, (rx, zy), (rx+RADAR_W, zy), (50,50,50), 1)
    cv2.line(frame, (radar_cx, ry+18), (radar_cx, ry+RADAR_H-20), (50,50,50), 1)
    car_y = ry + RADAR_H - 15
    cv2.rectangle(frame, (radar_cx-8, car_y-10), (radar_cx+8, car_y+5), (0,200,255), -1)
    cv2.putText(frame, "YOU", (radar_cx-10, car_y+16),
                cv2.FONT_HERSHEY_SIMPLEX, 0.3, (0,200,255), 1)
    for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in objects.items():
        cname = TARGET_CLASSES[class_id]
        dc    = (0,0,255) if cname=="car" else ((0,255,0) if cname=="person" else (255,100,0))
        dot_rx = max(rx+5, min(rx+RADAR_W-5, int(rx+(cx/FRAME_W)*RADAR_W)))
        ar  = (x2-x1)*(y2-y1) / FRAME_AREA
        vr  = min(1.0, y2/(FRAME_H*0.9))
        hyb = min(1.0, (vr*0.7)+(ar*2.5))
        nd  = min(hyb/PROXIMITY_DANGER, 1.0)
        dot_ry = max(ry+20, min(ry+RADAR_H-25, int((ry+20)+(1.0-nd)*(RADAR_H-50))))
        cv2.circle(frame, (dot_rx, dot_ry), 5, dc, -1)
        cv2.putText(frame, f"{cname[0].upper()}{obj_id}",
                    (dot_rx+6, dot_ry+4), cv2.FONT_HERSHEY_SIMPLEX, 0.3, dc, 1)
    return frame


# ============================================================
# 8. Camera & Model Init (FIXED)
# ============================================================
print("\nOpening camera...")
try:
    picam2 = Picamera2()
    config = picam2.create_preview_configuration(
        main={"format": "RGB888", "size": (FRAME_W, FRAME_H)},
        controls={
            "FrameRate": 30,
            "AwbEnable": True,
            "AwbMode": 0,
        }
    )
    picam2.configure(config)
    picam2.start()
    time.sleep(1)
    print("Camera ready!")
except RuntimeError as e:
    print(f"\nERROR: Cannot open camera — {e}")
    print("Fix: run  sudo pkill -f picamera2  then retry.")
    sys.exit(1)

print(f"\nLoading model: {MODEL_PATH}")
opts = ort.SessionOptions()
opts.intra_op_num_threads = 4
session    = ort.InferenceSession(MODEL_PATH, sess_options=opts,
                                   providers=["CPUExecutionProvider"])
input_name = session.get_inputs()[0].name
print("Model loaded!")

_connect_hub()
tracker = CentroidTracker(max_disappeared=25, max_distance=100)

# ============================================================
# 9. Runtime State (FIXED: skip_frames = 2 for better FPS)
# ============================================================
frame_count    = 0
skip_frames    = 3          # ★★★ زيادة الـ FPS ★★★
fps            = 0
fps_counter    = 0
fps_time       = time.time()
last_stats     = {"cars": 0, "persons": 0, "bikes": 0}
last_objects   = {}
_prev_moto_flag = 0

print("\n" + "=" * 60)
print("CONTROLS:  [R]=RED  [G]=GREEN  [Y]=AMBER  [Q]=Quit")
print("=" * 60 + "\n")

# ============================================================
# 10. Main Loop (FIXED: NO color conversion)
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

        # ★★★ التغيير الأساسي: من غير تحويل ألوان ★★★
        frame = rgb_frame

        with _tl_lock:
            CAR_TRAFFIC_LIGHT = _CAR_TL_STR
            PED_TRAFFIC_LIGHT = _PED_TL_STR

        # ── Inference ─────────────────────────────────────────────
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
                    x1 = int((cx-wb/2)*scale_x); y1 = int((cy-hb/2)*scale_y)
                    x2 = int((cx+wb/2)*scale_x); y2 = int((cy+hb/2)*scale_y)
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
                        x1 = max(0,x1);       y1 = max(0,y1)
                        x2 = min(FRAME_W,x2); y2 = min(FRAME_H,y2)
                        rects.append((x1, y1, x2, y2, raw_classes[i]))

            tracked      = tracker.update(rects)
            last_objects = tracked
            last_stats   = {
                "cars":    sum(1 for v in tracked.values() if v[6]==2),
                "persons": sum(1 for v in tracked.values() if v[6]==0),
                "bikes":   sum(1 for v in tracked.values() if v[6] in (1,3)),
            }

        # ── Drawing & Alerts ───────────────────────────────────────
        zone_y         = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        warnings_dict  = {}
        frame_ped_flag = 0
        frame_pos_flag = 0
        frame_moto_flag= 0
        frame_lead_flag= 0

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            cname = TARGET_CLASSES[class_id]
            color = (0,255,0) if cname=="person" else ((0,0,255) if cname=="car" else (255,100,0))

            pos_label                        = get_position_label(cx, x1, y1, x2, y2, class_id)
            ratio, prox_level, prox_label, prox_color = estimate_proximity(x1, y1, x2, y2)
            dist_m                           = estimate_distance_meters(class_id, y1, y2)
            dist_str = f"~{dist_m}m" if dist_m is not None else f"{ratio*100:.1f}%"

            if prox_level == "DANGER":
                color = (0,0,255)
                add_warning(warnings_dict, obj_id, "TOO_CLOSE",
                            f"! TOO CLOSE: {cname} #{obj_id} {dist_str}", (0,0,255), pos_label)
            elif prox_level == "WARNING":
                if cname != "person": color = (0,165,255)
                add_warning(warnings_dict, obj_id, "CLOSE",
                            f"~ CLOSE: {cname} #{obj_id} {dist_str}", (0,165,255), pos_label)

            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id, (x1,y1,x2,y2))

            if class_id == 0:
                in_zone      = (y1+y2)//2 > zone_y
                ped_flag_val = 2 if ("CROSSING" in str(intent_label) or in_zone) else (1 if risk_level in ("MED","HIGH") else 0)
                if ped_flag_val > frame_ped_flag:
                    frame_ped_flag = ped_flag_val
                    frame_pos_flag = get_position_flag(cx)
                if risk_level == "HIGH":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! PERSON CROSSING (#{obj_id})", (0,0,255), pos_label)
                elif risk_level == "MED":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Person Approaching (#{obj_id})", (0,165,255), pos_label)

            if class_id == 3 and (y1+y2)//2 > zone_y and prox_level=="DANGER" and risk_level=="HIGH":
                frame_moto_flag = 1

            hist = list(tracker.history[obj_id])
            is_stationary = False
            if len(hist) >= 5:
                sp = [math.hypot(hist[i][0]-hist[i-1][0], hist[i][1]-hist[i-1][1])
                      for i in range(1, len(hist))]
                if (np.mean(sp[-5:]) if len(sp)>=5 else (np.mean(sp) if sp else 999)) < 1.0:
                    is_stationary = True

            if class_id==2 and is_stationary and (y1+y2)//2 > zone_y:
                if CAR_TRAFFIC_LIGHT == "RED":
                    add_warning(warnings_dict, obj_id, "CLOSE",
                                f"! Lead Car at RED. Safe Stop. (#{obj_id})", (0,0,255), pos_label)
                elif CAR_TRAFFIC_LIGHT == "GREEN":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Car Stopped on GREEN! (#{obj_id})", (0,0,200), pos_label)
                    frame_lead_flag = max(frame_lead_flag, 2 if prox_level=="DANGER" else 1)
                elif CAR_TRAFFIC_LIGHT in ("AMBER","YELLOW"):
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Car Stopped on AMBER. Prepare. (#{obj_id})", (0,165,255), pos_label)
                    frame_lead_flag = max(frame_lead_flag, 2 if prox_level=="DANGER" else 1)

            if class_id==0 and ("CROSSING" in str(intent_label) or (y1+y2)//2 > zone_y):
                if PED_TRAFFIC_LIGHT == "WALK":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Pedestrian Legally Crossing. Yield. (#{obj_id})", (0,165,255), pos_label)
                elif PED_TRAFFIC_LIGHT == "DONT_WALK":
                    add_warning(warnings_dict, obj_id, "EMERGENCY",
                                f"!! JAYWALKING ON RED! (#{obj_id})", (0,0,255), pos_label)

            thickness = 3 if prox_level=="DANGER" or risk_level=="HIGH" else 2
            cv2.rectangle(frame, (x1,y1), (x2,y2), color, thickness)
            cv2.putText(frame, f"#{obj_id} {cname}",
                        (x1, max(y1-22,12)), cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)
            if prox_level != "SAFE":
                cv2.putText(frame, prox_label,
                            (x2+4,(y1+y2)//2), cv2.FONT_HERSHEY_SIMPLEX, 0.45, prox_color, 2)
            if intent_label:
                cv2.putText(frame, intent_label,
                            (x1, max(y1-6,22)), cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)
            if pos_label:
                cv2.putText(frame, pos_label,
                            (x1, min(y2+16,FRAME_H-4)), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200,200,200), 1)

            bx     = min(x2+2, FRAME_W-8)
            fill_h = int((y2-y1) * min(ratio/PROXIMITY_DANGER, 1.0))
            cv2.rectangle(frame, (bx,y1),         (bx+5,y2),         (80,80,80),   -1)
            cv2.rectangle(frame, (bx,y2-fill_h),  (bx+5,y2),         prox_color,   -1)

            pts_h = list(tracker.history[obj_id])
            for k in range(1, len(pts_h)):
                a  = k / len(pts_h)
                tc = tuple(int(c*a) for c in color)
                cv2.line(frame, pts_h[k-1], pts_h[k], tc, 1)

        _publish_v2p_frame(frame_ped_flag, frame_pos_flag, frame_lead_flag)
        if frame_moto_flag != _prev_moto_flag:
            _publish_motorcycle_alert(frame_moto_flag)
            _prev_moto_flag = frame_moto_flag

        cv2.line(frame, (0,zone_y), (FRAME_W,zone_y), (0,200,200), 1)
        cv2.putText(frame, "-- Road Zone --", (5,zone_y-5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0,200,200), 1)

        frame = draw_radar(frame, last_objects)

        warnings_list = [(t,c) for (_,t,c) in sorted(warnings_dict.values(), key=lambda x:-x[0])]
        if warnings_list:
            ph = 30 + len(warnings_list)*28
            ov = frame.copy()
            cv2.rectangle(ov, (0,FRAME_H-ph), (FRAME_W,FRAME_H), (20,20,20), -1)
            frame = cv2.addWeighted(ov, 0.6, frame, 0.4, 0)
            if any(c==(0,0,255) for (_,c) in warnings_list):
                fl = frame.copy()
                cv2.rectangle(fl, (0,0), (FRAME_W,FRAME_H), (0,0,180), -1)
                frame = cv2.addWeighted(fl, 0.12, frame, 0.88, 0)
            cv2.putText(frame, "DRIVER ALERTS:", (10,FRAME_H-ph+20),
                        cv2.FONT_HERSHEY_DUPLEX, 0.55, (255,255,255), 1)
            for idx,(wt,wc) in enumerate(warnings_list):
                yp = FRAME_H-ph+20+(idx+1)*28
                cv2.rectangle(frame, (8,yp-14),(18,yp-4), wc, -1)
                cv2.putText(frame, wt, (24,yp-3), cv2.FONT_HERSHEY_SIMPLEX, 0.55, wc, 2)

        cv2.putText(frame, f"Cars: {last_stats['cars']}",       (10,30),  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,0,255),   2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}", (10,55),  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,255,0),   2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",     (10,80),  cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,100,0), 2)
        cv2.putText(frame, f"FPS: {fps}",                       (10,105), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255,255,255),2)
        cv2.putText(frame, f"Car: {CAR_TRAFFIC_LIGHT}", (FRAME_W-190,30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,0), 2)
        cv2.putText(frame, f"Ped: {PED_TRAFFIC_LIGHT}", (FRAME_W-190,52), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,0), 2)

        print(f"\rCars:{last_stats['cars']} | Persons:{last_stats['persons']} | "
              f"Bikes:{last_stats['bikes']} | FPS:{fps} | TL={CAR_TRAFFIC_LIGHT}", end="")

        cv2.imshow("V2P System", frame)
        key = cv2.waitKey(1) & 0xFF
        if key == ord('r'):
            with _tl_lock: _CAR_TL_STR="RED";   _PED_TL_STR="DONT_WALK"
            print("\n[SIM] RED")
        elif key == ord('g'):
            with _tl_lock: _CAR_TL_STR="GREEN"; _PED_TL_STR="WALK"
            print("\n[SIM] GREEN")
        elif key == ord('y'):
            with _tl_lock: _CAR_TL_STR="AMBER"; _PED_TL_STR="DONT_WALK"
            print("\n[SIM] AMBER")
        elif key == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    _publish_v2p_frame(0, 0, 0)
    _publish_motorcycle_alert(0)
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\nDone. Total frames: {frame_count}")