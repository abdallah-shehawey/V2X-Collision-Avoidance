# =============================================================================
# V2P SYSTEM — Vehicle-to-Pedestrian Safety System
# =============================================================================
# PURPOSE:
#   Detects persons, cars, motorcycles, and bicycles in real-time using a
#   YOLOv8 ONNX model. Tracks each object across frames, estimates pedestrian
#   intent (standing / walking / approaching / crossing), and warns the driver
#   when any object enters a danger zone or gets too close to the camera.
#
# HARDWARE TARGET: Raspberry Pi 4 with Pi Camera Module
# MODEL FORMAT   : YOLOv8 exported to ONNX (80-class COCO)
# =============================================================================

import cv2                                  # Image processing, drawing, display
import numpy as np                          # Fast numerical operations on arrays
import time                                 # FPS measurement and camera warm-up
import onnxruntime as ort                   # Run the YOLOv8 ONNX model on CPU
from picamera2 import Picamera2             # Raspberry Pi camera interface
from collections import deque, defaultdict  # deque: fixed-size history list
                                            # defaultdict: auto-init dict values

print("=" * 60)
print("V2P SYSTEM - PEDESTRIAN INTENT DETECTION")
print("=" * 60)

# =============================================================================
# SECTION 1 — Configuration
# =============================================================================

MODEL_PATH       = "model2.onnx"  # Path to the YOLOv8 ONNX weights file
CONF_THRESH      = 0.30           # Min confidence to accept a detection (30%)
MODEL_INPUT_SIZE = 640            # YOLOv8 expects 640×640 pixel input images
FRAME_W, FRAME_H = 640, 480       # Camera capture resolution

# Only detect these 4 classes from the 80-class COCO dataset.
# Any other class (dog, chair, etc.) is ignored.
TARGET_CLASSES = {
    0: "person",
    1: "bicycle",
    2: "car",
    3: "motorcycle",
}

# --- Intent detection thresholds ---
# Speed is measured in pixels moved per frame (px/frame).
SPEED_THRESHOLD_FAST = 6.0   # Above this → person is jogging / rushing
SPEED_THRESHOLD_SLOW = 1.5   # Below this → person is standing still
CROSSING_ZONE_RATIO  = 0.35  # Bottom 35% of the frame is treated as road zone
APPROACH_FRAMES      = 5     # Number of recent frames used to compute speed/direction

# --- Proximity (closeness) thresholds ---
# Proximity is estimated from bounding box area relative to total frame area.
# A larger box means the object is closer to the camera (= closer to our car).
FRAME_AREA        = FRAME_W * FRAME_H  # 640 × 480 = 307,200 pixels²
PROXIMITY_DANGER  = 0.10  # Box covers >10% of frame → TOO CLOSE  (red alert)
PROXIMITY_WARNING = 0.05  # Box covers  >5% of frame → CLOSE      (orange warn)
PROXIMITY_SAFE    = 0.02  # Box covers  <2% of frame → safe distance

# =============================================================================
# SECTION 2 — CentroidTracker Class
# =============================================================================
# PURPOSE:
#   Assigns a stable integer ID to every detected object and keeps that ID
#   consistent across frames by matching new detections to existing tracked
#   objects using Euclidean distance between centroids.
#
# WHY NEEDED:
#   The YOLO model detects objects independently in every frame — it has no
#   memory. Without a tracker, we cannot know that "person in frame 5" is the
#   same person as "person in frame 4". The tracker solves this by linking
#   detections across frames using proximity.
#
# KEY DATA STRUCTURES:
#   self.objects     — dict {id: (cx, cy)}         last known centroid per object
#   self.disappeared — dict {id: int}              frames missed since last seen
#   self.history     — dict {id: deque(maxlen=30)} last 30 centroids per object
#                      *** THIS IS WHAT analyze_intent() READS ***
# =============================================================================

class CentroidTracker:
    def __init__(self, max_disappeared=20, max_distance=80):
        # max_disappeared: remove an object after this many consecutive missed frames
        # max_distance   : if nearest match is farther than this (px), register as new
        self.next_id     = 0
        self.objects     = {}
        self.disappeared = {}
        self.max_dis     = max_disappeared
        self.max_dist    = max_distance
        self.history     = defaultdict(lambda: deque(maxlen=30))

    def register(self, cx, cy):
        # Called when a detection has no matching existing object.
        # Assigns the next available ID and starts its position history.
        self.objects[self.next_id]     = (cx, cy)
        self.disappeared[self.next_id] = 0
        self.history[self.next_id].append((cx, cy))
        self.next_id += 1

    def deregister(self, obj_id):
        # Called when an object has been missing for too long.
        # Removes it from active tracking (history is kept for debugging).
        del self.objects[obj_id]
        del self.disappeared[obj_id]

    def update(self, rects):
        # rects: list of (x1, y1, x2, y2, class_id) from the current frame.
        # Returns: dict {obj_id: (cx, cy, x1, y1, x2, y2, class_id)}

        # --- Case 1: No detections this frame ---
        # Increment disappeared counters; remove objects that have been gone too long.
        if len(rects) == 0:
            for obj_id in list(self.disappeared):
                self.disappeared[obj_id] += 1
                if self.disappeared[obj_id] > self.max_dis:
                    self.deregister(obj_id)
            return {}

        # Compute centroids for all incoming detections.
        # Using the center point (cx, cy) instead of the full box simplifies matching.
        input_centroids = []
        for (x1, y1, x2, y2, _) in rects:
            cx = int((x1 + x2) / 2)
            cy = int((y1 + y2) / 2)
            input_centroids.append((cx, cy))

        # --- Case 2: No objects tracked yet → register everything as new ---
        if len(self.objects) == 0:
            for i, (cx, cy) in enumerate(input_centroids):
                self.register(cx, cy)
        else:
            obj_ids   = list(self.objects.keys())
            obj_cents = list(self.objects.values())

            # Build a distance matrix D where D[r][c] is the Euclidean distance
            # between existing object r and new detection c.
            D = np.zeros((len(obj_cents), len(input_centroids)))
            for r, (ox, oy) in enumerate(obj_cents):
                for c, (ix, iy) in enumerate(input_centroids):
                    D[r, c] = np.sqrt((ox - ix)**2 + (oy - iy)**2)

            # Greedy matching: process pairs starting from the smallest distance.
            # This ensures the best (closest) matches are assigned first.
            rows = D.min(axis=1).argsort()
            cols = D.argmin(axis=1)[rows]

            used_rows, used_cols = set(), set()
            for (row, col) in zip(rows, cols):
                if row in used_rows or col in used_cols:
                    continue  # already matched
                if D[row, col] > self.max_dist:
                    continue  # too far apart → treat as a new object
                obj_id = obj_ids[row]
                cx, cy = input_centroids[col]
                self.objects[obj_id]     = (cx, cy)   # update position
                self.disappeared[obj_id] = 0           # reset missed-frame counter
                self.history[obj_id].append((cx, cy))  # log position for intent
                used_rows.add(row)
                used_cols.add(col)

            # Unmatched new detections → new objects
            for col in range(len(input_centroids)):
                if col not in used_cols:
                    self.register(*input_centroids[col])

            # Unmatched existing objects → increment disappeared counter
            for row in range(len(obj_cents)):
                if row not in used_rows:
                    obj_id = obj_ids[row]
                    self.disappeared[obj_id] += 1
                    if self.disappeared[obj_id] > self.max_dis:
                        self.deregister(obj_id)

        # Build and return the result dict, mapping each ID to its full box info.
        result = {}
        for i, (x1, y1, x2, y2, class_id) in enumerate(rects):
            cx = int((x1 + x2) / 2)
            cy = int((y1 + y2) / 2)
            for obj_id, (ox, oy) in self.objects.items():
                if ox == cx and oy == cy:
                    result[obj_id] = (cx, cy, x1, y1, x2, y2, class_id)
                    break
        return result


# =============================================================================
# SECTION 3 — Intent Analyzer
# =============================================================================
# PURPOSE:
#   Reads the position history stored in tracker.history[obj_id] and decides
#   what the person is most likely doing: standing, walking, approaching the
#   road, or actively crossing.
#
# ONLY runs for class_id == 0 (person). All other classes return None / LOW.
#
# HOW SPEED IS CALCULATED:
#   We take the last APPROACH_FRAMES (5) positions and compute the Euclidean
#   distance between consecutive points. The mean of those distances is the
#   average speed in pixels per frame.
#
# HOW DIRECTION IS CALCULATED:
#   dx = horizontal displacement over the last 5 frames (positive = moving right)
#   dy = vertical displacement over the last 5 frames
#        IMPORTANT: in image coordinates, y increases downward.
#        So positive dy means the person is moving DOWN = toward the road.
#
# DECISION TREE:
#   in_road_zone AND speed > FAST  →  "CROSSING FAST"  HIGH  red
#   in_road_zone AND lateral motion →  "CROSSING"       HIGH  red
#   moving toward road AND speed>SLOW → "Approaching"   MED   orange
#   speed < SLOW                   →  "Standing"        LOW   green
#   (default)                      →  "Walking"         LOW   green
# =============================================================================

def analyze_intent(obj_id, history, class_id):
    # Only analyze persons; return neutral values for all other classes.
    if class_id != 0:
        return None, "LOW", (0, 255, 0)

    pts = list(history[obj_id])  # list of (cx, cy) positions, oldest → newest

    # Need at least 3 data points to compute a meaningful motion vector.
    if len(pts) < 3:
        return "Observing", "LOW", (0, 255, 0)

    # --- Step 1: Compute average speed (px/frame) ---
    recent    = pts[-APPROACH_FRAMES:] if len(pts) >= APPROACH_FRAMES else pts
    dists     = [np.sqrt((recent[i][0]-recent[i-1][0])**2 +
                         (recent[i][1]-recent[i-1][1])**2)
                 for i in range(1, len(recent))]
    avg_speed = np.mean(dists) if dists else 0.0

    # --- Step 2: Compute net displacement vector ---
    dx = pts[-1][0] - pts[max(0, len(pts)-APPROACH_FRAMES)][0]  # left/right
    dy = pts[-1][1] - pts[max(0, len(pts)-APPROACH_FRAMES)][1]  # up/down

    # Positive dy = person moved downward in the image = toward the road.
    moving_toward_road = dy > 3

    # --- Step 3: Check current position ---
    # Road zone occupies the bottom CROSSING_ZONE_RATIO fraction of the frame.
    # Example: FRAME_H=480, ratio=0.35 → road zone starts at y = 480*0.65 = 312
    cy_now       = pts[-1][1]
    in_road_zone = cy_now > FRAME_H * (1 - CROSSING_ZONE_RATIO)

    # Lateral motion: horizontal movement dominates vertical → crossing sideways
    moving_laterally = abs(dx) > abs(dy) * 0.8

    # --- Step 4: Decision tree ---
    if in_road_zone and avg_speed > SPEED_THRESHOLD_FAST:
        return "CROSSING FAST", "HIGH", (0, 0, 255)
    if in_road_zone and moving_laterally:
        return "CROSSING",      "HIGH", (0, 0, 255)
    if moving_toward_road and avg_speed > SPEED_THRESHOLD_SLOW:
        return "Approaching",   "MED",  (0, 165, 255)
    if avg_speed < SPEED_THRESHOLD_SLOW:
        return "Standing",      "LOW",  (0, 255, 0)
    return "Walking",           "LOW",  (0, 200, 100)


# =============================================================================
# SECTION 4 — Proximity Estimator
# =============================================================================
# PURPOSE:
#   Estimates how close an object is to the camera (our vehicle) based on
#   the size of its bounding box relative to the total frame area.
#
# PRINCIPLE:
#   Larger box area → object is closer to the camera.
#   We express proximity as:  ratio = box_area / frame_area
#
#   ratio >= PROXIMITY_DANGER  (0.10) → object covers >10% of frame → TOO CLOSE
#   ratio >= PROXIMITY_WARNING (0.05) → object covers  >5% of frame → CLOSE
#   ratio <  PROXIMITY_SAFE    (0.02) → far away, no warning needed
#
# LIMITATION:
#   This is an approximation. True metric distance (meters) requires camera
#   calibration or a depth sensor. Box-area proximity works well for relative
#   danger assessment without extra hardware.
# =============================================================================

def estimate_proximity(x1, y1, x2, y2):
    box_area = (x2 - x1) * (y2 - y1)
    ratio    = box_area / FRAME_AREA  # value between 0.0 and 1.0

    if ratio >= PROXIMITY_DANGER:
        return ratio, "DANGER",  "TOO CLOSE!", (0, 0, 255)
    elif ratio >= PROXIMITY_WARNING:
        return ratio, "WARNING", "CLOSE",      (0, 165, 255)
    else:
        return ratio, "SAFE",    "",            (0, 220, 0)


# =============================================================================
# SECTION 5 — Camera Initialization
# =============================================================================
# Opens the Raspberry Pi camera using the Picamera2 library.
# Format RGB888: 3 channels (R, G, B), 8 bits each.
# We sleep 1 second after start() to let the sensor exposure stabilize
# before we begin reading frames.
# =============================================================================

print("\nOpening camera...")
picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"format": "RGB888", "size": (FRAME_W, FRAME_H)},
    controls={"FrameRate": 30}
)
picam2.configure(config)
picam2.start()
time.sleep(1)   # allow camera sensor to stabilize
print("Camera ready!")


# =============================================================================
# SECTION 6 — Model Loading
# =============================================================================
# Loads the YOLOv8 ONNX model into an ONNX Runtime inference session.
#
# intra_op_num_threads = 2:
#   Limits parallelism to 2 threads, appropriate for Raspberry Pi 4.
#   On a PC you can increase this to 4 or 8.
#
# providers = ["CPUExecutionProvider"]:
#   Run inference on CPU. Raspberry Pi has no ONNX-compatible GPU.
#   On a PC with NVIDIA GPU, add "CUDAExecutionProvider" first in the list.
#
# input_name:
#   The model's input tensor has a name (e.g. "images"). We read it
#   dynamically so the code works with any YOLOv8 ONNX export.
# =============================================================================

print(f"\nLoading model: {MODEL_PATH}")
opts = ort.SessionOptions()
opts.intra_op_num_threads = 2
session = ort.InferenceSession(
    MODEL_PATH, sess_options=opts,
    providers=["CPUExecutionProvider"]
)
input_name = session.get_inputs()[0].name
print("Model loaded!")


# =============================================================================
# SECTION 7 — Tracker & State Variables
# =============================================================================

tracker = CentroidTracker(max_disappeared=25, max_distance=100)

frame_count  = 0
skip_frames  = 2     # Run inference every 2nd frame to save CPU on Raspberry Pi
fps          = 0
fps_counter  = 0
fps_time     = time.time()

# These hold the last computed results and are reused on skipped frames,
# so the display stays updated even when inference is not running.
last_stats   = {"cars": 0, "persons": 0, "bikes": 0}
last_objects = {}

print("\nSystem ready! Press 'q' to stop\n")


# =============================================================================
# SECTION 8 — Main Loop
# =============================================================================

try:
    while True:

        # --- 8.1 Capture frame ---
        # picam2.capture_array() returns a NumPy array in RGB format (H, W, 3).
        rgb_frame = picam2.capture_array()
        frame_count += 1

        # --- 8.2 FPS counter ---
        # Count how many frames are processed per second.
        # Every 1.0 seconds, save the count as the current FPS and reset.
        fps_counter += 1
        if time.time() - fps_time >= 1.0:
            fps         = fps_counter
            fps_counter = 0
            fps_time    = time.time()

        # Convert RGB → BGR because OpenCV drawing functions expect BGR order.
        frame = cv2.cvtColor(rgb_frame, cv2.COLOR_RGB2BGR)

        # --- 8.3 Inference block (runs every skip_frames frames) ---
        if frame_count % skip_frames == 0:

            # -- Preprocessing --
            # Step 1: Resize to 640×640 (model's required input size).
            blob = cv2.resize(rgb_frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))
            # Step 2: Convert pixel values from [0, 255] int to [0.0, 1.0] float.
            blob = blob.astype(np.float32) / 255.0
            # Step 3: Reorder axes from (H, W, C) to (1, C, H, W).
            #   The model expects: batch=1, channels=3, height=640, width=640.
            blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

            # -- Run model --
            # Output shape: (1, 84, 8400)
            #   8400 = number of candidate boxes the model evaluates
            #   84   = 4 box coordinates + 80 class confidence scores
            outputs = session.run(None, {input_name: blob})

            # Reshape to (8400, 84): one row per candidate box.
            preds = np.squeeze(outputs[0]).T

            # -- Decode detections --
            # Scale factors convert box coordinates from 640×640 space back to
            # the actual frame resolution (e.g. 640×480).
            raw_boxes, raw_scores, raw_classes = [], [], []
            scale_x = FRAME_W / MODEL_INPUT_SIZE
            scale_y = FRAME_H / MODEL_INPUT_SIZE

            for p in preds:
                class_scores = p[4:]                       # 80 class confidences
                class_id     = int(np.argmax(class_scores))# highest-confidence class
                score        = float(class_scores[class_id])

                if score > CONF_THRESH and class_id in TARGET_CLASSES:
                    # YOLOv8 outputs boxes as (center_x, center_y, width, height).
                    # Convert to top-left / bottom-right corners.
                    cx, cy, wb, hb = p[0:4]
                    x1 = int((cx - wb / 2) * scale_x)
                    y1 = int((cy - hb / 2) * scale_y)
                    x2 = int((cx + wb / 2) * scale_x)
                    y2 = int((cy + hb / 2) * scale_y)
                    raw_boxes.append([x1, y1, x2 - x1, y2 - y1])
                    raw_scores.append(score)
                    raw_classes.append(class_id)

            # -- Non-Maximum Suppression (NMS) --
            # The model often produces multiple overlapping boxes for the same object.
            # NMS keeps only the highest-confidence box when two boxes overlap > 40%.
            rects_for_tracker = []
            if raw_boxes:
                indices = cv2.dnn.NMSBoxes(raw_boxes, raw_scores, CONF_THRESH, 0.4)
                if len(indices) > 0:
                    for i in indices.flatten():
                        x1, y1, wb, hb = raw_boxes[i]
                        x2, y2 = x1 + wb, y1 + hb
                        # Clamp coordinates to stay within frame bounds.
                        x1 = max(0, x1);       y1 = max(0, y1)
                        x2 = min(FRAME_W, x2); y2 = min(FRAME_H, y2)
                        rects_for_tracker.append((x1, y1, x2, y2, raw_classes[i]))

            # -- Update tracker --
            # The tracker matches these detections to existing tracked objects,
            # updates their positions in self.objects, and appends to self.history.
            tracked      = tracker.update(rects_for_tracker)
            last_objects = tracked

            # Count objects by class for the HUD display.
            last_stats = {
                "cars":    sum(1 for v in tracked.values() if v[6] == 2),
                "persons": sum(1 for v in tracked.values() if v[6] == 0),
                "bikes":   sum(1 for v in tracked.values() if v[6] in (1, 3)),
            }

        # --- 8.4 Drawing & Alert Generation ---
        # warnings_list collects all active alerts for this frame.
        # Each entry is a (text, BGR_color) tuple shown in the driver panel.
        warnings_list = []

        for obj_id, (cx, cy, x1, y1, x2, y2, class_id) in last_objects.items():
            class_name = TARGET_CLASSES[class_id]

            # Default box color per class type.
            if class_name == "car":
                color = (0, 0, 255)    # red
            elif class_name == "person":
                color = (0, 255, 0)    # green
            else:
                color = (255, 100, 0)  # blue-orange for bikes

            # -- Proximity check --
            # Estimate how close this object is based on its box size.
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

            # -- Road zone check for vehicles and bicycles --
            # If a vehicle's center is inside the road zone, add an alert.
            if class_id in (1, 2, 3):
                cy_obj = (y1 + y2) // 2
                if cy_obj > FRAME_H * (1 - CROSSING_ZONE_RATIO):
                    zone_msgs = {
                        2: (f"! CAR IN ROAD ZONE (#{obj_id})",   (0,   0, 255)),
                        3: (f"! MOTORCYCLE IN ROAD (#{obj_id})", (0, 100, 255)),
                        1: (f"! BICYCLE IN ROAD (#{obj_id})",    (0, 140, 255)),
                    }
                    warnings_list.append(zone_msgs[class_id])
                    color = (0, 0, 255)

            # -- Pedestrian intent analysis --
            # analyze_intent reads tracker.history[obj_id] (the last 30 positions)
            # and returns what this person appears to be doing.
            intent_label, risk_level, intent_color = analyze_intent(
                obj_id, tracker.history, class_id
            )

            if class_id == 0:   # persons only
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

            # -- Draw bounding box --
            # Use a thicker border for dangerous objects.
            thickness = 3 if prox_level == "DANGER" or risk_level == "HIGH" else 2
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, thickness)

            # Object ID and class name label above the box.
            cv2.putText(frame, f"#{obj_id} {class_name}",
                        (x1, max(y1 - 22, 12)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, 2)

            # Proximity label to the right of the box (only when not safe).
            if prox_level != "SAFE":
                cv2.putText(frame, prox_label,
                            (x2 + 4, (y1 + y2) // 2),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, prox_color, 2)

            # Intent label below the ID label (persons only).
            if intent_label:
                cv2.putText(frame, intent_label,
                            (x1, max(y1 - 6, 22)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, intent_color, 2)

            # -- Proximity bar --
            # A small vertical bar on the right edge of the box fills up as the
            # object gets closer. Full bar = at the DANGER threshold.
            bar_x  = min(x2 + 2, FRAME_W - 8)
            bar_h  = y2 - y1
            fill_h = int(bar_h * min(ratio / PROXIMITY_DANGER, 1.0))
            cv2.rectangle(frame, (bar_x, y1),          (bar_x+5, y2),          (80, 80, 80),  -1)
            cv2.rectangle(frame, (bar_x, y2 - fill_h), (bar_x+5, y2),          prox_color,    -1)

            # -- Motion trail --
            # Draws a fading line through the object's last 30 positions.
            # Older positions are drawn dimmer (alpha closer to 0).
            hist = list(tracker.history[obj_id])
            for k in range(1, len(hist)):
                alpha       = k / len(hist)                        # 0.0 (old) → 1.0 (new)
                trail_color = tuple(int(c * alpha) for c in color)
                cv2.line(frame, hist[k-1], hist[k], trail_color, 1)

        # -- Road zone divider line --
        zone_y = int(FRAME_H * (1 - CROSSING_ZONE_RATIO))
        cv2.line(frame, (0, zone_y), (FRAME_W, zone_y), (0, 200, 200), 1)
        cv2.putText(frame, "-- Road Zone --", (5, zone_y - 5),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 200, 200), 1)

        # --- 8.5 Driver Alert Panel ---
        # If any warnings are active, render a semi-transparent panel at the
        # bottom of the frame listing all current alerts.
        if warnings_list:
            panel_h = 30 + len(warnings_list) * 28

            # Dark semi-transparent background for readability.
            overlay = frame.copy()
            cv2.rectangle(overlay,
                          (0, FRAME_H - panel_h), (FRAME_W, FRAME_H),
                          (20, 20, 20), -1)
            frame = cv2.addWeighted(overlay, 0.6, frame, 0.4, 0)

            # Red screen flash for any HIGH-risk (red) alert.
            if any(w[1] == (0, 0, 255) for w in warnings_list):
                flash = frame.copy()
                cv2.rectangle(flash, (0, 0), (FRAME_W, FRAME_H), (0, 0, 180), -1)
                frame = cv2.addWeighted(flash, 0.12, frame, 0.88, 0)

            cv2.putText(frame, "DRIVER ALERTS:",
                        (10, FRAME_H - panel_h + 20),
                        cv2.FONT_HERSHEY_DUPLEX, 0.55, (255, 255, 255), 1)

            # Each alert gets a colored bullet square followed by its message.
            for idx, (warn_text, warn_color) in enumerate(warnings_list):
                y_pos = FRAME_H - panel_h + 20 + (idx + 1) * 28
                cv2.rectangle(frame, (8, y_pos-14), (18, y_pos-4), warn_color, -1)
                cv2.putText(frame, warn_text, (24, y_pos - 3),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.55, warn_color, 2)

        # --- 8.6 HUD counters (top-left) ---
        cv2.putText(frame, f"Cars: {last_stats['cars']}",       (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0,   0, 255), 2)
        cv2.putText(frame, f"Persons: {last_stats['persons']}", (10, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255,   0), 2)
        cv2.putText(frame, f"Bikes: {last_stats['bikes']}",     (10, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 100, 0), 2)
        cv2.putText(frame, f"FPS: {fps}",                       (10, 105),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (220, 220, 220), 2)

        # Terminal progress line (overwrites itself with \r).
        print(
            f"\rCars:{last_stats['cars']} | "
            f"Persons:{last_stats['persons']} | "
            f"Bikes:{last_stats['bikes']} | "
            f"FPS:{fps}",
            end=""
        )

        # Display frame and listen for 'q' key to quit.
        cv2.imshow("V2P System", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\nStopped by user.")

finally:
    # Always release resources, regardless of how the loop ended.
    picam2.stop()            # shut down camera cleanly
    cv2.destroyAllWindows()  # close all OpenCV windows
    print(f"\nDone. Total frames processed: {frame_count}")
