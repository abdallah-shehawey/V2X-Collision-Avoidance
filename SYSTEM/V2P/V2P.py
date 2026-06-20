
"""
V2P.py - Vehicle-to-Pedestrian System (Raspberry Pi)
======================================================
Runs YOLOv8 on PiCamera2 frames to detect persons, cars, and motorcycles.

NEW (IPC integration)
----------------------
After every inference batch, publishes two topics to the local hub:

  "pedestrian_status"  — pedestrian count, moving count, crossing safety,
                         alert level, and the current traffic-light state
                         (received from hub topic "traffic_light_state").

  "vehicle_data"       — nearby vehicle count, estimated speed, braking
                         flag, bike count, and front ultrasonic distance.

Also SUBSCRIBES to:

  "traffic_light_state" — published by the V2I bridge (ESP32 WiFi → server
                          → hub) so V2P knows the current light state and
                          can warn pedestrians accordingly.

  "v2v_alert"          — published when V2V (UART/STM32) detects a hazard,
                         so V2P can raise its own alert level.

Run order: hub.py → V2P.py (+ Car_client.py + dashboard_bridge.py)
"""

import cv2
import numpy as np
import time
import threading
import onnxruntime as ort
from picamera2 import Picamera2

from ipc_node import IPCNode   # ← local hub client

print("=" * 60)
print("🚗 V2P SYSTEM  — YOLOv8 ONNX + IPC Hub")
print("=" * 60)

# ─────────────────────────────────────────────
# 1.  Settings
# ─────────────────────────────────────────────
MODEL_PATH       = "yolov8n.onnx"
CONF_THRESH      = 0.30
MODEL_INPUT_SIZE = 640

TARGET_CLASSES = {0: "person", 2: "car", 3: "motorcycle"}

# Pedestrian-proximity thresholds (fraction of frame height)
CROSSING_Y_MIN = 0.35   # top of the crossing zone
CROSSING_Y_MAX = 0.75   # bottom of the crossing zone

# ─────────────────────────────────────────────
# 2.  IPC Node — connect to local hub
# ─────────────────────────────────────────────
ipc = IPCNode("v2p_camera")

# Shared state updated by hub callbacks
_traffic_light_state = "RED"   # updated from "traffic_light_state" topic
_v2v_alert_active    = False   # updated from "v2v_alert" topic
_ipc_lock            = threading.Lock()


def _on_traffic_light(topic, data, sender):
    """Receive traffic-light state from V2I bridge via hub."""
    global _traffic_light_state
    with _ipc_lock:
        _traffic_light_state = data.get("state", "RED")
    print(f"\n[V2P] 🚦 traffic light → {_traffic_light_state}  (from {sender})")


def _on_v2v_alert(topic, data, sender):
    """Receive V2V hazard alert (e.g. EEBL, FCW) from UART bridge via hub."""
    global _v2v_alert_active
    with _ipc_lock:
        _v2v_alert_active = bool(data.get("active", False))
    print(f"\n[V2P] ⚡ V2V alert active={_v2v_alert_active}  (from {sender})")


print("\n🏠 Connecting V2P to Local IPC Hub …")
if ipc.connect():
    ipc.subscribe("traffic_light_state", _on_traffic_light)
    ipc.subscribe("v2v_alert",           _on_v2v_alert)
    ipc.start_listening()
    print("✅ IPC connected — subscribing to traffic_light_state & v2v_alert\n")
else:
    print("⚠️  Hub not found — V2P will run without IPC (no hub publishing)\n")

# ─────────────────────────────────────────────
# 3.  Camera
# ─────────────────────────────────────────────
print("📷 Opening camera …")
picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"format": "RGB888", "size": (640, 480)},
    controls={"FrameRate": 30}
)
picam2.configure(config)
picam2.start()
time.sleep(1)
print("✅ Camera ready!")

# ─────────────────────────────────────────────
# 4.  ONNX Model
# ─────────────────────────────────────────────
print(f"\n📦 Loading model: {MODEL_PATH}")
opts = ort.SessionOptions()
opts.intra_op_num_threads = 2
session    = ort.InferenceSession(MODEL_PATH, sess_options=opts,
                                  providers=["CPUExecutionProvider"])
input_name = session.get_inputs()[0].name
print("✅ Model loaded!")

# ─────────────────────────────────────────────
# 5.  Helpers
# ─────────────────────────────────────────────

def _estimate_alert_level(persons_count: int,
                           moving_count: int,
                           crossing_safe: bool,
                           light: str,
                           v2v: bool) -> str:
    """Return 'none', 'caution', or 'danger'."""
    if v2v or (persons_count > 0 and light == "GREEN" and not crossing_safe):
        return "danger"
    if persons_count > 0 and light != "RED":
        return "caution"
    if moving_count > 0:
        return "caution"
    return "none"


def _estimate_speed(boxes_prev, boxes_curr, dt: float) -> float:
    """Very rough pixel-motion → km/h estimate (placeholder)."""
    if not boxes_prev or not boxes_curr or dt <= 0:
        return 0.0
    # Use centroid of first matched box
    prev_cy = boxes_prev[0][1] + boxes_prev[0][3] / 2
    curr_cy = boxes_curr[0][1] + boxes_curr[0][3] / 2
    delta_px = abs(curr_cy - prev_cy)
    # 1 px ≈ 0.003 m at ~5 m distance; very rough
    speed_mps = (delta_px * 0.003) / dt
    return round(min(speed_mps * 3.6, 120.0), 1)


# ─────────────────────────────────────────────
# 6.  Main loop
# ─────────────────────────────────────────────
frame_count  = 0
skip_frames  = 2          # process every 2nd frame
fps          = 0
fps_counter  = 0
fps_time     = time.time()

prev_car_boxes: list = []
prev_time: float     = time.time()

# Publish interval: don't flood the hub on every processed frame
_last_pub_time = 0.0
PUB_INTERVAL   = 0.5     # seconds between hub publishes

print("\n🎥 System ready! Press 'q' to stop\n")

try:
    while True:
        rgb_frame   = picam2.capture_array()
        frame_count += 1

        # FPS counter
        fps_counter += 1
        now = time.time()
        if now - fps_time >= 1.0:
            fps         = fps_counter
            fps_counter = 0
            fps_time    = now

        frame = cv2.cvtColor(rgb_frame, cv2.COLOR_RGB2BGR)

        if frame_count % skip_frames != 0:
            cv2.imshow("V2P System", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
            continue

        # ── Inference ──────────────────────────────────────────────
        blob = cv2.resize(rgb_frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))
        blob = blob.astype(np.float32) / 255.0
        blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

        outputs = session.run(None, {input_name: blob})
        preds   = np.squeeze(outputs[0]).T   # (8400, 84)

        cars_count    = 0
        persons_count = 0
        bikes_count   = 0
        moving_count  = 0
        crossing_safe = True

        boxes      = []
        scores     = []
        class_ids  = []
        car_boxes  = []

        frame_h, frame_w = frame.shape[:2]

        for p in preds:
            class_scores = p[4:]
            class_id     = int(np.argmax(class_scores))
            score        = float(class_scores[class_id])

            if score > CONF_THRESH and class_id in TARGET_CLASSES:
                cx, cy, wb, hb = p[0:4]
                x1     = int((cx - wb / 2) * (frame_w / MODEL_INPUT_SIZE))
                y1     = int((cy - hb / 2) * (frame_h / MODEL_INPUT_SIZE))
                w_box  = int(wb * (frame_w / MODEL_INPUT_SIZE))
                h_box  = int(hb * (frame_h / MODEL_INPUT_SIZE))
                boxes.append([x1, y1, w_box, h_box])
                scores.append(score)
                class_ids.append(class_id)

        indices = cv2.dnn.NMSBoxes(boxes, scores, CONF_THRESH, 0.4)

        if len(indices) > 0:
            for i in indices.flatten():
                x1, y1, w_box, h_box = boxes[i]
                x2, y2 = x1 + w_box, y1 + h_box
                x1, y1 = max(0, x1), max(0, y1)
                x2, y2 = min(frame_w, x2), min(frame_h, y2)

                cid  = class_ids[i]
                name = TARGET_CLASSES[cid]

                if name == "car":
                    cars_count += 1
                    car_boxes.append([x1, y1, w_box, h_box])
                    color = (0, 0, 255)
                elif name == "person":
                    persons_count += 1
                    # Is the person inside the crossing zone?
                    norm_cy = (y1 + h_box / 2) / frame_h
                    if CROSSING_Y_MIN <= norm_cy <= CROSSING_Y_MAX:
                        moving_count  += 1
                        crossing_safe  = False
                    color = (0, 255, 0)
                else:
                    bikes_count += 1
                    color = (255, 0, 0)

                cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                cv2.putText(frame, f"{name} {scores[i]:.2f}",
                            (x1, y1 - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

        # ── Speed estimate ─────────────────────────────────────────
        dt         = time.time() - prev_time
        speed_kmh  = _estimate_speed(prev_car_boxes, car_boxes, dt)
        prev_car_boxes = car_boxes
        prev_time      = time.time()

        # ── Overlay stats ──────────────────────────────────────────
        with _ipc_lock:
            light  = _traffic_light_state
            v2v_ok = _v2v_alert_active

        alert = _estimate_alert_level(persons_count, moving_count,
                                      crossing_safe, light, v2v_ok)

        cv2.putText(frame, f"Cars: {cars_count}",    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
        cv2.putText(frame, f"Persons: {persons_count}", (10, 55),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        cv2.putText(frame, f"Bikes: {bikes_count}",  (10, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 0), 2)
        cv2.putText(frame, f"FPS: {fps}",            (10, 105),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        cv2.putText(frame, f"Light: {light}  Alert: {alert}", (10, 130),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 220, 220), 2)

        print(f"\r🚗 Cars:{cars_count} | 👤 Persons:{persons_count} | "
              f"🏍️ Bikes:{bikes_count} | Alert:{alert} | FPS:{fps}", end="")

        # ── Publish to hub ─────────────────────────────────────────
        now = time.time()
        if now - _last_pub_time >= PUB_INTERVAL:
            _last_pub_time = now

            # 1) pedestrian_status
            ipc.publish("pedestrian_status", {
                "timestamp":        now,
                "pedestrian_count": persons_count,
                "moving_count":     moving_count,
                "crossing_safe":    crossing_safe,
                "alert_level":      alert,
                "traffic_light":    light,
            })

            # 2) vehicle_data  (camera-side vehicle sensing)
            ipc.publish("vehicle_data", {
                "timestamp":    now,
                "speed_kmh":    speed_kmh,
                "brake_pressed": False,   # no brake sensor here; STM32 sends via V2V
                "car_count":    cars_count,
                "moving_cars":  cars_count,   # all detected cars assumed moving
                "bike_count":   bikes_count,
                "ultrasonic": {
                    "front_cm": 999.0     # camera can't give cm; STM32 fills this
                },
            })

        # ── Display ────────────────────────────────────────────────
        cv2.imshow("V2P System", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n\n🛑 Stopped by user")

finally:
    picam2.stop()
    cv2.destroyAllWindows()
    print(f"\n✅ Done. Total frames: {frame_count}")
