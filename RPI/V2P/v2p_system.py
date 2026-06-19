import cv2
import numpy as np
import time
import onnxruntime as ort
from picamera2 import Picamera2
from collections import deque, defaultdict

print("=" * 60)
print("🚗 V2P SYSTEM - WITH PEDESTRIAN INTENT DETECTION")
print("=" * 60)

# ============================================================
# 1. Settings & Configuration
# ============================================================
MODEL_PATH = "yolov8n.onnx"
CONF_THRESH = 0.30
MODEL_INPUT_SIZE = 640
FRAME_W, FRAME_H = 640, 480

# COCO class IDs we care about
TARGET_CLASSES = {
    0: "person",
    2: "car",
    3: "motorcycle",
    1: "bicycle",
}

# Intent thresholds
SPEED_THRESHOLD_FAST   = 6.0   # px/frame — jogging / rushing
SPEED_THRESHOLD_SLOW   = 1.5   # px/frame — nearly standing still
CROSSING_ZONE_RATIO    = 0.35  # bottom 35% of frame = road zone
APPROACH_FRAMES        = 5     # how many frames to check trajectory

# ============================================================
# 2. Simple centroid tracker
# ============================================================
class CentroidTracker:
    """
    Lightweight tracker that assigns stable IDs to detections
    by matching closest centroids frame-to-frame.
    """
    def __init__(self, max_disappeared=20, max_distance=80):
        self.next_id       = 0
        self.objects       = {}          # id -> centroid
        self.disappeared   = {}          # id -> frames missing
        self.max_dis       = max_disappeared
        self.max_dist      = max_distance
        self.history       = defaultdict(lambda: deque(maxlen=30))  # id -> centroid history

    def register(self, cx, cy):
        self.objects[self.next_id]     = (cx, cy)
        self.disappeared[self.next_id] = 0
        self.history[self.next_id].append((cx, cy))
        self.next_id += 1

    def deregister(self, obj_id):
        del self.objects[obj_id]
        del self.disappeared[obj_id]
        # keep history a little while for debug

    def update(self, rects):
        """
        rects: list of (x1, y1, x2, y2, class_id)
        returns: dict {obj_id: (cx, cy, x1, y1, x2, y2, class_id)}
        """
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

            # Compute distance matrix
            D = np.zeros((len(obj_cents), len(input_centroids)))
            for r, (ox, oy) in enumerate(obj_cents):
                for c, (ix, iy) in enumerate(input_centroids):
                    D[r, c] = np.sqrt((ox - ix)**2 + (oy - iy)**2)

            # Greedy matching: smallest distances first
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

            # Register unmatched new detections
            for col in range(len(input_centroids)):
                if col not in used_cols:
                    self.register(*input_centroids[col])

            # Mark missing objects
            for row in range(len(obj_cents)):
                if row not in used_rows:
                    obj_id = obj_ids[row]
                    self.disappeared[obj_id] += 1
                    if self.disappeared[obj_id] > self.max_dis:
                        self.deregister(obj_id)

        # Build result map
        result = {}
        for i, (x1, y1, x2, y2, class_id) in enumerate(rects):
            cx = int((x1 + x2) / 2)
            cy = int((y1 + y2) / 2)
            # Find which object ID was assigned to this centroid
            for obj_id, (ox, oy) in self.objects.items():
                if ox == cx and oy == cy:
                    result[obj_id] = (cx, cy, x1, y1, x2, y2, class_id)
                    break
        return result


# ============================================================
# 3. Intent Analyzer
# ============================================================
def analyze_intent(obj_id, history, class_id):
    """
    Analyzes motion history to estimate pedestrian intent.
    Returns (intent_label, risk_level, color)
        intent_label : short description string
        risk_level   : "HIGH" | "MED" | "LOW"
        color        : BGR tuple
    """
    if class_id != 0:          # only analyze persons
        return None, "LOW", (0, 255, 0)

    pts = list(history[obj_id])
    if len(pts) < 3:
        return "Observing", "LOW", (0, 255, 0)

    # --- Speed (pixels per frame, smoothed over last N frames) ---
    recent = pts[-APPROACH_FRAMES:] if len(pts) >= APPROACH_FRAMES else pts
    dists  = [np.sqrt((recent[i][0]-recent[i-1][0])**2 +
                      (recent[i][1]-recent[i-1][1])**2)
              for i in range(1, len(recent))]
    avg_speed = np.mean(dists) if dists else 0.0

    # --- Direction vector (net displacement over recent frames) ---
    dx = pts[-1][0] - pts[max(0, len(pts)-APPROACH_FRAMES)][0]
    dy = pts[-1][1] - pts[max(0, len(pts)-APPROACH_FRAMES)][1]

    # Positive dy = moving DOWN = toward road (camera is above)
    moving_toward_road = dy > 3

    # --- Position: is person already in road zone? ---
    cy_now = pts[-1][1]
    in_road_zone = cy_now > FRAME_H * (1 - CROSSING_ZONE_RATIO)

    # --- Lateral motion (left-right crossing) ---
    moving_laterally = abs(dx) > abs(dy) * 0.8

    # ---- Decision tree ----
    if in_road_zone and avg_speed > SPEED_THRESHOLD_FAST:
        return "⚠️ CROSSING FAST", "HIGH", (0, 0, 255)

    if in_road_zone and moving_laterally:
        return "⚠️ CROSSING", "HIGH", (0, 0, 255)

    if moving_toward_road and avg_speed > SPEED_THRESHOLD_SLOW:
        return "🟡 Approaching", "MED", (0, 165, 255)

    if avg_speed < SPEED_THRESHOLD_SLOW:
        return "🟢 Standing", "LOW", (0, 255, 0)

    return "🟢 Walking", "LOW", (0, 200, 100)


# ============================================================
# 4. Camera Initialization
# ============================================================
print("\n📷 Opening camera...")
picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"format": "RGB888", "size": (FRAME_W, FRAME_H)},
    controls={"FrameRate": 30}
)
picam2.configure(config)
picam2.start()
time.sleep(1)
print("✅ Camera ready!")

# ============================================================
# 5. Model Loading
# ============================================================
print(f"\n📦 Loading model: {MODEL_PATH}")
opts = ort.SessionOptions()
opts.intra_op_num_threads = 2
session = ort.InferenceSession(
    MODEL_PATH, sess_options=opts,
    providers=["CPUExecutionProvider"]
)
input_name = session.get_inputs()[0].name
print("✅ Model loaded!")

# ============================================================
# 6. Tracker init
# ============================================================
tracker = CentroidTracker(max_disappeared=25, max_distance=100)

# ============================================================
# 7. Runtime Loop
# ============================================================
frame_count  = 0
skip_frames  = 2
fps = fps_counter = 0
fps_time     = time.time()

# Stats preserved across skipped frames
last_stats   = {"cars": 0, "persons": 0, "bikes": 0}
last_objects = {}

print("\n🎥 System ready! Press 'q' to stop\n")

try:
    while True:
        rgb_frame = picam2.capture_array()
        frame_count += 1

        # FPS counter
        fps_counter += 1
        if time.time() - fps_time >= 1.0:
            fps         = fps_counter
            fps_counter = 0
            fps_time    = time.time()

        frame = cv2.cvtColor(rgb_frame, cv2.COLOR_RGB2BGR)

        if frame_count % skip_frames == 0:
            # ---- Preprocess ----
            blob = cv2.resize(rgb_frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))
            blob = blob.astype(np.float32) / 255.0
            blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

            # ---- Inference ----
            outputs = session.run(None, {input_name: blob})
            preds   = np.squeeze(outputs[0]).T   # (8400, 84)

            # ---- Decode detections ----
            raw_boxes   = []
            raw_scores  = []
            raw_classes = []

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

            # ---- NMS ----
            rects_for_tracker = []
            if raw_boxes:
                indices = cv2.dnn.NMSBoxes(raw_boxes, raw_scores, CONF_THRESH, 0.4)
                if len(indices) > 0:
                    for i in indices.flatten():
                        x1, y1, wb, hb = raw_boxes[i]
                        x2, y2 = x1 + wb, y1 + hb
                        x1 = max(0, x1); y1 = max(0, y1)
                        x2 = min(FRAME_W, x2); y2 = min(FRAME_H, y2)
                        rects_for_tracker.append(
                            (x1, y1, x2, y2, raw_classes[i])
                        )

            # ---- Update tracker ----
            tracked = tracker.update(rects_for_tracker)
            last_objects = tracked

            # ---- Count stats ----
            cars_count    = sum(1 for v in tracked.values() if v[6] == 2)
            persons_count = sum(1 for v in tracked.values() if v[6] == 0)
            bikes_count   = sum(1 for v in tracked.values() if v[6] in (1, 3))
            last_stats    = {"cars": cars_count, "persons": persons_count, "bikes": bikes_count}

        # ---- Draw tracked objects ----
        high_risk_alert = False

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            class_name = TARGET_CLASSES[class_id]

            # Base color by class
            if class_name == "car":
                color = (0, 0, 255)
            elif class_name == "person":
                color = (0, 255, 0)
            else:
                color = (255, 100, 0)

            # Intent analysis (persons only)
            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id
            )

            if risk_level == "HIGH":
                color          = intent_color
                high_risk_alert = True
            elif risk_level == "MED":
                color = intent_color

            # Bounding box
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)

            # ID + class label
            label = f"#{obj_id} {class_name}"
            cv2.putText(frame, label, (x1, y1 - 22),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

            # Intent label (persons only)
            if intent_label:
                cv2.putText(frame, intent_label, (x1, y1 - 6),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)

            # Draw motion trail
            hist = list(tracker.history[obj_id])
            for k in range(1, len(hist)):
                alpha = k / len(hist)
                trail_color = tuple(int(c * alpha) for c in color)
                cv2.line(frame, hist[k-1], hist[k], trail_color, 1)

        # ---- Crossing-zone line ----
        zone_y = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        cv2.line(frame, (0, zone_y), (FRAME_W, zone_y), (0, 200, 200), 1)
        cv2.putText(frame, "-- Road Zone --", (5, zone_y - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 200, 200), 1)

        # ---- HUD overlay ----
        if high_risk_alert:
            overlay = frame.copy()
            cv2.rectangle(overlay, (0, 0), (FRAME_W, FRAME_H), (0, 0, 200), -1)
            frame = cv2.addWeighted(overlay, 0.12, frame, 0.88, 0)
            cv2.putText(frame, "⚠️  PEDESTRIAN CROSSING ALERT",
                        (60, FRAME_H - 15),
                        cv2.FONT_HERSHEY_DUPLEX, 0.7, (0, 0, 255), 2)

        cv2.putText(frame, f"Cars: {last_stats['cars']}",    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}", (10, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",  (10, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 0), 2)
        cv2.putText(frame, f"FPS: {fps}",                    (10, 105),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        print(
            f"\r🚗 Cars:{last_stats['cars']} | "
            f"👤 Persons:{last_stats['persons']} | "
            f"🏍️ Bikes:{last_stats['bikes']} | "
            f"FPS:{fps}",
            end=""
        )

        cv2.imshow("V2P System - Intent Detection", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\n🛑 Stopped by user")

finally:
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\n✅ Done. Total frames: {frame_count}")
