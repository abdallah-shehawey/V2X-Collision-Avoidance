import cv2
import numpy as np
import time
import math
import onnxruntime as ort
from picamera2 import Picamera2
from collections import deque, defaultdict

# LCD support — install with: pip install RPLCD
try:
    from RPLCD.i2c import CharLCD
    lcd = CharLCD(i2c_expander='PCF8574', address=0x27,
                  port=1, cols=16, rows=2, dotsize=8)
    lcd.clear()
    lcd.write_string("V2P System")
    lcd.crlf()
    lcd.write_string("Starting...")
    LCD_AVAILABLE = True
except Exception:
    LCD_AVAILABLE = False
    print("[LCD] Not found or RPLCD not installed. LCD output disabled.")

# LCD helper — writes top-priority warning to the 16x2 display
_lcd_last_text = ""

def lcd_show(line1, line2=""):
    global _lcd_last_text
    text = line1 + line2
    if not LCD_AVAILABLE or text == _lcd_last_text:
        return
    try:
        lcd.clear()
        lcd.write_string(line1[:16])
        if line2:
            lcd.crlf()
            lcd.write_string(line2[:16])
        _lcd_last_text = text
    except Exception:
        pass

print("=" * 60)
print("V2P SYSTEM - RASPBERRY PI (Proximity Fix + LEFT/RIGHT Zones)")
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
CROSSING_ZONE_RATIO  = 0.35
APPROACH_FRAMES      = 5

FRAME_AREA = FRAME_W * FRAME_H

# Hybrid proximity thresholds (0.0 to 1.0)
PROXIMITY_DANGER  = 0.60
PROXIMITY_WARNING = 0.35
PROXIMITY_SAFE    = 0.15

# Warning priority: higher number = higher priority (shown instead of lower ones)
WARN_PRIORITY = {
    "EMERGENCY":  5,
    "CROSSING":   4,
    "TOO_CLOSE":  3,
    "APPROACHING":2,
    "CLOSE":      1,
    "NONE":       0,
}

# ============================================================
# 2. Traffic Light State (V2I Simulation) - للاختبار بالكيبورد
# ============================================================
CAR_TRAFFIC_LIGHT = "GREEN"
PED_TRAFFIC_LIGHT = "DONT_WALK"

def update_traffic_light_from_server(car_state, ped_state):
    # Replace this body with MQTT / HTTP client code when integrating a real server
    global CAR_TRAFFIC_LIGHT, PED_TRAFFIC_LIGHT
    CAR_TRAFFIC_LIGHT = car_state
    PED_TRAFFIC_LIGHT = ped_state

# ============================================================
# 3. Centroid Tracker (Hybrid IOU + Distance cost)
# ============================================================
class CentroidTracker:
    def __init__(self, max_disappeared=20, max_distance=80):
        self.next_id      = 0
        self.objects      = {}        # id -> (cx, cy)
        self.objects_bbox = {}        # id -> (x1, y1, x2, y2)
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
        xA = max(boxA[0], boxB[0]);  yA = max(boxA[1], boxB[1])
        xB = min(boxA[2], boxB[2]);  yB = min(boxA[3], boxB[3])
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
# 4. Intent Analyzer (normalized speed)
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
# 5b. Distance Estimator (meters — pinhole camera approximation)
# ============================================================
FOCAL_PX = 600.0   # tune this value after testing

REAL_HEIGHT_M = {
    0: 1.70,   # person
    1: 1.10,   # bicycle
    2: 1.50,   # car
    3: 1.20,   # motorcycle
}

def estimate_distance_meters(class_id, y1, y2):
    px_height = y2 - y1
    if px_height < 10:
        return None
    real_h = REAL_HEIGHT_M.get(class_id, 1.70)
    dist   = (real_h * FOCAL_PX) / px_height
    return round(dist, 1)


# ============================================================
# 6. Warning Deduplication
# ============================================================
def add_warning(warnings_dict, obj_id, key, text, color):
    new_pri = WARN_PRIORITY.get(key, 0)
    if obj_id not in warnings_dict or new_pri > warnings_dict[obj_id][0]:
        warnings_dict[obj_id] = (new_pri, text, color)


# ============================================================
# 7. Radar & Position Label (المعدل حسب طلبك - LEFT/RIGHT zones)
# ============================================================
RADAR_W      = 160
RADAR_H      = 180
RADAR_MARGIN = 10

def get_position_label(cx, x1, y1, x2, y2, class_id):
    """
    تقسيم الشاشة إلى 5 أقسام متساوية (كل قسم 20% من العرض = 128 بيكسل).
    مهم فقط: LEFT (128-256) و RIGHT (384-512).
    الأطراف والمنتصف => None (مش هتظهر على الشاشة)
    """
    # حدود المناطق المهمة
    left_start  = 128
    left_end    = 256
    right_start = 384
    right_end   = 512

    # تحديد المنطقة
    if left_start <= cx < left_end:
        h_label = "LEFT"
    elif right_start <= cx < right_end:
        h_label = "RIGHT"
    else:
        # في الأطراف أو المنتصف → مش مهم، متظهرش position
        return None

    # حساب المسافة بالمتر
    dist_m = estimate_distance_meters(class_id, y1, y2)
    if dist_m is not None:
        d_label = f"~{dist_m}m"
    else:
        area_ratio     = (x2-x1)*(y2-y1) / FRAME_AREA
        vertical_ratio = min(1.0, y2 / (FRAME_H * 0.9))
        hybrid         = min(1.0, (vertical_ratio * 0.7) + (area_ratio * 2.5))
        d_label = "VERY CLOSE" if hybrid >= PROXIMITY_DANGER else ("CLOSE" if hybrid >= PROXIMITY_WARNING else "FAR")

    return f"{h_label}  {d_label}"


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
        dc = (0, 0, 255) if cname == "car" else ((0, 255, 0) if cname == "person" else (255, 100, 0))

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
# 8. Camera & Model Initialization (Raspberry Pi)
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
# 9. Runtime State
# ============================================================
frame_count  = 0
skip_frames  = 2
fps          = 0
fps_counter  = 0
fps_time     = time.time()
last_stats   = {"cars": 0, "persons": 0, "bikes": 0}
last_objects = {}

print("\n" + "=" * 60)
print("CONTROLS (لاختبار إشارات المرور):")
print("  [R] -> Car RED    | Ped DONT_WALK")
print("  [G] -> Car GREEN  | Ped WALK")
print("  [Y] -> Car AMBER  | Ped DONT_WALK")
print("  [Q] -> Quit")
print("=" * 60 + "\n")

# ============================================================
# 10. Main Loop (Raspberry Pi)
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

        # ---- Inference ----
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
                cs      = p[4:]
                cid     = int(np.argmax(cs))
                score   = float(cs[cid])
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

        # ---- Drawing & Alerts ----
        zone_y      = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        warnings_dict = {}

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            cname = TARGET_CLASSES[class_id]
            color = (0,255,0) if cname=="person" else ((0,0,255) if cname=="car" else (255,100,0))

            # -- Proximity --
            ratio, prox_level, prox_label, prox_color = estimate_proximity(x1, y1, x2, y2)
            prox_pct = f"{ratio*100:.1f}%"
            dist_m   = estimate_distance_meters(class_id, y1, y2)
            dist_str = f"~{dist_m}m" if dist_m is not None else prox_pct

            if prox_level == "DANGER":
                color = (0, 0, 255)
                add_warning(warnings_dict, obj_id, "TOO_CLOSE",
                            f"! TOO CLOSE: {cname} #{obj_id} {dist_str}", (0, 0, 255))
            elif prox_level == "WARNING":
                if cname != "person":
                    color = (0, 165, 255)
                add_warning(warnings_dict, obj_id, "CLOSE",
                            f"~ CLOSE: {cname} #{obj_id} {dist_str}", (0, 165, 255))

            # -- Intent (persons only) --
            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id, (x1, y1, x2, y2)
            )
            if class_id == 0:
                if risk_level == "HIGH":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! PERSON CROSSING (#{obj_id})", (0, 0, 255))
                elif risk_level == "MED":
                    color = intent_color
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Person Approaching (#{obj_id})", (0, 165, 255))

            # -- Traffic Light Logic (V2I) --
            hist = list(tracker.history[obj_id])
            is_stationary = False
            if len(hist) >= 5:
                sp_dists = [math.hypot(hist[i][0]-hist[i-1][0],
                                       hist[i][1]-hist[i-1][1])
                            for i in range(1, len(hist))]
                if (np.mean(sp_dists[-5:]) if len(sp_dists)>=5 else np.mean(sp_dists) if sp_dists else 999) < 1.0:
                    is_stationary = True

            if class_id == 2 and is_stationary and (y1+y2)//2 > zone_y:
                if CAR_TRAFFIC_LIGHT == "RED":
                    add_warning(warnings_dict, obj_id, "CLOSE",
                                f"! Lead Car at RED Light. Safe Stop. (#{obj_id})", (0, 0, 255))
                elif CAR_TRAFFIC_LIGHT == "GREEN":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Car Stopped on GREEN - Check Road! (#{obj_id})", (0, 0, 200))
                elif CAR_TRAFFIC_LIGHT == "AMBER":
                    add_warning(warnings_dict, obj_id, "APPROACHING",
                                f"~ Car Stopped on AMBER. Prepare. (#{obj_id})", (0, 165, 255))

            if class_id == 0 and ("CROSSING" in str(intent_label) or (y1+y2)//2 > zone_y):
                if PED_TRAFFIC_LIGHT == "WALK":
                    add_warning(warnings_dict, obj_id, "CROSSING",
                                f"! Pedestrian Legally Crossing. Yield. (#{obj_id})", (0, 165, 255))
                elif PED_TRAFFIC_LIGHT == "DONT_WALK":
                    add_warning(warnings_dict, obj_id, "EMERGENCY",
                                f"!! JAYWALKING ON RED! (#{obj_id})", (0, 0, 255))

            # -- Draw box --
            thickness = 3 if prox_level == "DANGER" or risk_level == "HIGH" else 2
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)
            cv2.putText(frame, f"#{obj_id} {cname}",
                        (x1, max(y1-22, 12)), cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

            if prox_level != "SAFE":
                cv2.putText(frame, prox_label,
                            (x2+4, (y1+y2)//2), cv2.FONT_HERSHEY_SIMPLEX, 0.45, prox_color, 2)

            if intent_label:
                cv2.putText(frame, intent_label,
                            (x1, max(y1-6, 22)), cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)

            # ============================================================
            # ★ Position Label (LEFT/RIGHT فقط) - التعديل الجديد ★
            # ============================================================
            pos_label = get_position_label(cx, x1, y1, x2, y2, class_id)
            if pos_label is not None:
                cv2.putText(frame, pos_label,
                            (x1, min(y2+16, FRAME_H-4)), cv2.FONT_HERSHEY_SIMPLEX, 0.38, (200, 200, 200), 1)

            # Proximity bar
            bx     = min(x2+2, FRAME_W-8)
            bar_h  = y2 - y1
            fill_h = int(bar_h * min(ratio / PROXIMITY_DANGER, 1.0))
            cv2.rectangle(frame, (bx, y1),          (bx+5, y2),          (80, 80, 80),  -1)
            cv2.rectangle(frame, (bx, y2-fill_h),   (bx+5, y2),          prox_color,    -1)

            # Motion trail
            pts_hist = list(tracker.history[obj_id])
            for k in range(1, len(pts_hist)):
                alpha = k / len(pts_hist)
                tc    = tuple(int(c * alpha) for c in color)
                cv2.line(frame, pts_hist[k-1], pts_hist[k], tc, 1)

        # ---- Road zone line ----
        cv2.line(frame, (0, zone_y), (FRAME_W, zone_y), (0, 200, 200), 1)
        cv2.putText(frame, "-- Road Zone --", (5, zone_y-5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 200, 200), 1)

        # ---- Radar ----
        frame = draw_radar(frame, last_objects)

        # ---- Driver Alerts Panel (one warning per object) ----
        warnings_list = [(txt, clr) for (_, txt, clr) in
                         sorted(warnings_dict.values(), key=lambda x: -x[0])]

        # ---- LCD Output (highest priority warning only) ----
        if warnings_list:
            top_text = warnings_list[0][0]
            clean = top_text.lstrip("!~ ").strip()
            if len(clean) <= 16:
                lcd_show(clean)
            else:
                lcd_show(clean[:16], clean[16:32])
        else:
            lcd_show("V2P System", "  All Clear  ")

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

        # ---- HUD ----
        cv2.putText(frame, f"Cars: {last_stats['cars']}",       (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,   0, 255), 2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}", (10, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255,   0), 2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",     (10, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100,  0), 2)
        cv2.putText(frame, f"FPS: {fps}",                       (10, 105),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        cv2.putText(frame, f"Car Signal: {CAR_TRAFFIC_LIGHT}", (FRAME_W-200, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)
        cv2.putText(frame, f"Ped Signal: {PED_TRAFFIC_LIGHT}", (FRAME_W-200, 52),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)

        print(f"\rCars:{last_stats['cars']} | "
              f"Persons:{last_stats['persons']} | "
              f"Bikes:{last_stats['bikes']} | "
              f"FPS:{fps}", end="")

        cv2.imshow("V2P System - Raspberry Pi", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('r'):
            update_traffic_light_from_server("RED", "DONT_WALK")
            print("\n[SIM] Car RED  | Ped DONT_WALK")
        elif key == ord('g'):
            update_traffic_light_from_server("GREEN", "WALK")
            print("\n[SIM] Car GREEN | Ped WALK")
        elif key == ord('y'):
            update_traffic_light_from_server("AMBER", "DONT_WALK")
            print("\n[SIM] Car AMBER | Ped DONT_WALK")
        elif key == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    picam2.stop()
    if LCD_AVAILABLE:
        lcd.clear()
        lcd.write_string("System Off")
    cv2.destroyAllWindows()
    print(f"\nDone. Total frames: {frame_count}")