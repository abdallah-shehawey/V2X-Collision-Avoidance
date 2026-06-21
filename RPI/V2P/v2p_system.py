import cv2
import numpy as np
import time
import onnxruntime as ort
from picamera2 import Picamera2
from collections import deque, defaultdict

print("=" * 60)
print("V2P SYSTEM - PEDESTRIAN INTENT DETECTION")
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

SPEED_THRESHOLD_FAST = 6.0
SPEED_THRESHOLD_SLOW = 1.5
CROSSING_ZONE_RATIO  = 0.35
APPROACH_FRAMES      = 5

FRAME_AREA        = FRAME_W * FRAME_H
PROXIMITY_DANGER  = 0.10
PROXIMITY_WARNING = 0.05
PROXIMITY_SAFE    = 0.02

# ============================================================
# 2. Centroid Tracker
# ============================================================
class CentroidTracker:
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
                    D[r, c] = np.sqrt((ox - ix)**2 + (oy - iy)**2)

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
# 3. Intent Analyzer
# ============================================================
def analyze_intent(obj_id, history, class_id):
    if class_id != 0:
        return None, "LOW", (0, 255, 0)

    pts = list(history[obj_id])
    if len(pts) < 3:
        return "Observing", "LOW", (0, 255, 0)

    recent    = pts[-APPROACH_FRAMES:] if len(pts) >= APPROACH_FRAMES else pts
    dists     = [np.sqrt((recent[i][0]-recent[i-1][0])**2 +
                         (recent[i][1]-recent[i-1][1])**2)
                 for i in range(1, len(recent))]
    avg_speed = np.mean(dists) if dists else 0.0

    dx = pts[-1][0] - pts[max(0, len(pts)-APPROACH_FRAMES)][0]
    dy = pts[-1][1] - pts[max(0, len(pts)-APPROACH_FRAMES)][1]

    moving_toward_road = dy > 3
    cy_now             = pts[-1][1]
    in_road_zone       = cy_now > FRAME_H * (1 - CROSSING_ZONE_RATIO)
    moving_laterally   = abs(dx) > abs(dy) * 0.8

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
# 4. Proximity Estimator
# ============================================================
def estimate_proximity(x1, y1, x2, y2):
    box_area = (x2 - x1) * (y2 - y1)
    ratio    = box_area / FRAME_AREA

    if ratio >= PROXIMITY_DANGER:
        return ratio, "DANGER",  "TOO CLOSE!", (0, 0, 255)
    elif ratio >= PROXIMITY_WARNING:
        return ratio, "WARNING", "CLOSE",      (0, 165, 255)
    else:
        return ratio, "SAFE",    "",            (0, 220, 0)


# ============================================================
# 5. Camera Initialization
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
# 6. Model Loading
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
# 7. Tracker Init
# ============================================================
tracker = CentroidTracker(max_disappeared=25, max_distance=100)

# ============================================================
# 8. Runtime Loop
# ============================================================
frame_count  = 0
skip_frames  = 2
fps          = 0
fps_counter  = 0
fps_time     = time.time()
last_stats   = {"cars": 0, "persons": 0, "bikes": 0}
last_objects = {}

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

            if class_id in (1, 2, 3):
                cy_obj = (y1 + y2) // 2
                if cy_obj > FRAME_H * (1 - CROSSING_ZONE_RATIO):
                    zone_msgs = {
                        2: (f"! CAR IN ROAD ZONE (#{obj_id})",        (0, 0, 255)),
                        3: (f"! MOTORCYCLE IN ROAD (#{obj_id})",      (0, 100, 255)),
                        1: (f"! BICYCLE IN ROAD (#{obj_id})",         (0, 140, 255)),
                    }
                    warnings_list.append(zone_msgs[class_id])
                    color = (0, 0, 255)

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

        cv2.imshow("V2P System", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\nDone. Total frames: {frame_count}")
