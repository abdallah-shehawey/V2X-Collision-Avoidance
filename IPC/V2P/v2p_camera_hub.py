"""
v2p_camera_hub.py - V2P Camera Node (مربوط بالـ IPC Hub)
============================================================
نسخة محدثة من سكريبت الكاميرا (Picamera2 + YOLOv8 ONNX) بعد ربطه
بالـ IPC hub (hub.py):

    - يستقبل (subscribe) حالة الإشارة من topic "traffic_light"
    - يبعت  (publish)   بيانات المشاة على topic "pedestrian_status"
    - يبعت  (publish)   بيانات العربيات على topic "vehicle_data"

تشغيل:
    1) شغّل hub.py الأول.
    2) شغّل السكريبت ده على نفس الجهاز (Raspberry Pi).

فيه كمان:
    - شباك الكاميرا (cv2.imshow) شغال عادي زي الكود الأصلي.
    - طباعة حية في التيرمنال لكل فريم، عشان تتابع إن النظام شغال
      وبيبعت للـ hub صح (هتشوف ✅/❌ جنب "Hub:").

IPC Role
--------
    Node name  : "v2p_camera"
    Subscribes : "traffic_light"
    Publishes  : "pedestrian_status", "vehicle_data"
"""

import time
import threading

import cv2
import numpy as np
import onnxruntime as ort
from picamera2 import Picamera2

from ipc_node import IPCNode

# ════════════════════════════════════════════════════════════
# Configuration
# ════════════════════════════════════════════════════════════
NODE_NAME         = "v2p_camera"
MODEL_PATH        = "yolov8n.onnx"
CONF_THRESH        = 0.30
MODEL_INPUT_SIZE   = 640
CAM_WIDTH          = 640
CAM_HEIGHT         = 480
SKIP_FRAMES        = 2          # شغّل الموديل كل N فريم
PUB_INTERVAL       = 0.5        # أقل وقت بين كل publish للـ hub (ثواني)

TARGET_CLASSES = {
    0: "person",
    2: "car",
    3: "motorcycle",
}

# تقدير تقريبي للمسافة (front_cm) من عرض bounding box العربية
PX_TO_M = 0.05   # تقريب: بكسل ≈ متر عند مسافة ~10م (اضبطه على الكاميرا بتاعتك)

# ════════════════════════════════════════════════════════════
# Shared state — بتتحدث من thread الاستقبال (IPC) لما الإشارة تتغير
# ════════════════════════════════════════════════════════════
_traffic_light_state = {
    "state":         "RED",
    "remaining_sec": 0,
    "is_emergency":  False,
    "zone":          "unknown",
}
_tl_lock = threading.Lock()


def _on_traffic_light(topic: str, data: dict, sender: str) -> None:
    """يتحدث لما الـ hub يبعت فريم جديد على topic 'traffic_light'."""
    global _traffic_light_state
    with _tl_lock:
        _traffic_light_state = data
    print(
        f"\n[V2P] 🚦 traffic_light update : "
        f"state={data.get('state', '?')}  "
        f"remaining={data.get('remaining_sec', '?')}s"
        + ("  🚨 EMERGENCY" if data.get("is_emergency") else "")
    )


# ════════════════════════════════════════════════════════════
# Centroid Tracker — لحساب سرعة بكسلية تقريبية (متحرك / واقف)
# ════════════════════════════════════════════════════════════
class Tracker:
    """Tracker بسيط: بيوصل كل detection بأقرب object كان موجود قبل كده."""

    def __init__(self) -> None:
        self.objects: dict = {}
        self.id_counter: int = 0

    def track(self, detections: list) -> dict:
        now = time.time()
        new_tracked: dict = {}

        for (x1, y1, x2, y2, obj_type, conf) in detections:
            cx, cy = (x1 + x2) // 2, (y1 + y2) // 2

            match_id = None
            for oid, d in self.objects.items():
                if np.hypot(cx - d["c"][0], cy - d["c"][1]) < 60:
                    match_id = oid
                    break

            if match_id is not None:
                old_cx = self.objects[match_id]["c"][0]
                new_tracked[match_id] = {
                    "b": (x1, y1, x2, y2),
                    "c": (cx, cy),
                    "v": cx - old_cx,
                    "type": obj_type,
                    "conf": conf,
                    "t": now,
                }
            else:
                new_tracked[self.id_counter] = {
                    "b": (x1, y1, x2, y2),
                    "c": (cx, cy),
                    "v": 0,
                    "type": obj_type,
                    "conf": conf,
                    "t": now,
                }
                self.id_counter += 1

        # شيل أي object متشافش من ثانية
        self.objects = {k: v for k, v in new_tracked.items() if now - v["t"] < 1.0}
        return self.objects


# ════════════════════════════════════════════════════════════
# Safety Analyzer — يبني الـ payloads اللي هتروح للـ hub
# ════════════════════════════════════════════════════════════
class SafetyAnalyzer:
    def analyze(self, tracked_objects: dict, tl_state: dict):
        pedestrians, cars, bikes = [], [], []

        for oid, d in tracked_objects.items():
            v        = d["v"]
            obj_type = d["type"]
            moving   = abs(v) > 2

            if obj_type == "person":
                pedestrians.append({"id": oid, "moving": moving, "speed": abs(v)})
            elif obj_type == "car":
                x1, y1, x2, y2 = d["b"]
                box_w = max(x2 - x1, 1)
                front_cm_est = round((180.0 * 650) / box_w, 1)
                cars.append({
                    "id": oid, "moving": moving, "speed": abs(v),
                    "front_cm": front_cm_est,
                })
            elif obj_type == "motorcycle":
                bikes.append({"id": oid, "moving": moving, "speed": abs(v)})

        moving_cars   = [c for c in cars if c["moving"]]
        moving_people = [p for p in pedestrians if p["moving"]]
        light         = tl_state.get("state", "UNKNOWN")
        is_emerg      = tl_state.get("is_emergency", False)

        alerts = []
        if is_emerg:
            alerts.append("EMERGENCY")
        if light == "RED":
            alerts.append("STOP")
        if moving_cars and moving_people:
            alerts.append("WARNING")
        if len(pedestrians) > 3:
            alerts.append("CAUTION_PEDS")
        if len(cars) > 5:
            alerts.append("CAUTION_TRAFFIC")

        has_danger  = "WARNING" in alerts or "EMERGENCY" in alerts
        has_caution = any(a.startswith("CAUTION") for a in alerts)

        if has_danger:
            alert_level, crossing_safe = "danger", False
        elif has_caution:
            alert_level, crossing_safe = "caution", True
        else:
            alert_level, crossing_safe = "none", True

        pedestrian_payload = {
            "timestamp":        time.time(),
            "pedestrian_count": len(pedestrians),
            "moving_count":     len(moving_people),
            "crossing_safe":    crossing_safe,
            "alert_level":      alert_level,
            "traffic_light":    light,
        }

        nearest_front_cm = min((c["front_cm"] for c in cars), default=999.0)
        avg_speed_px = (
            sum(c["speed"] for c in moving_cars) / len(moving_cars)
            if moving_cars else 0
        )
        speed_kmh_est = round(avg_speed_px * PX_TO_M * 3.6, 1)

        vehicle_payload = {
            "timestamp":     time.time(),
            "speed_kmh":     speed_kmh_est,
            "brake_pressed": light == "RED",
            "car_count":     len(cars),
            "moving_cars":   len(moving_cars),
            "bike_count":    len(bikes),
            "ultrasonic":    {"front_cm": nearest_front_cm},
        }

        return alerts, pedestrian_payload, vehicle_payload


# ════════════════════════════════════════════════════════════
# Main
# ════════════════════════════════════════════════════════════
def main() -> None:
    print("=" * 60)
    print("🚗 V2P SYSTEM — Camera + YOLOv8 ONNX + IPC HUB")
    print("=" * 60)

    # ── الاتصال بالـ hub ────────────────────────────────────────
    node = IPCNode(NODE_NAME)
    connected = node.connect()
    if connected:
        node.subscribe("traffic_light", _on_traffic_light)
        node.start_listening()
        print("[V2P] ✅ متصل بالـ hub — هيبعت على 'pedestrian_status' و 'vehicle_data'\n")
    else:
        print("[V2P] ⚠️  مش متصل بالـ hub (شغّل hub.py الأول) — هيشتغل محلي بدون publish\n")

    # ── الكاميرا ──────────────────────────────────────────────
    print("📷 Opening camera...")
    picam2 = Picamera2()
    cam_config = picam2.create_preview_configuration(
        main={"format": "RGB888", "size": (CAM_WIDTH, CAM_HEIGHT)},
        controls={"FrameRate": 30},
    )
    picam2.configure(cam_config)
    picam2.start()
    time.sleep(1)
    print("✅ Camera ready!")

    # ── الموديل ──────────────────────────────────────────────
    print(f"\n📦 Loading model: {MODEL_PATH}")
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 2
    session = ort.InferenceSession(
        MODEL_PATH, sess_options=opts, providers=["CPUExecutionProvider"]
    )
    input_name = session.get_inputs()[0].name
    print("✅ Model loaded!\n")

    tracker  = Tracker()
    analyzer = SafetyAnalyzer()

    frame_count = 0
    fps, fps_counter, fps_time = 0, 0, time.time()
    last_pub = 0.0

    print("🎥 System ready! اضغط 'q' في شباك الكاميرا للخروج\n")

    try:
        while True:
            rgb_frame = picam2.capture_array()
            frame_count += 1

            fps_counter += 1
            if time.time() - fps_time >= 1.0:
                fps, fps_counter, fps_time = fps_counter, 0, time.time()

            frame = cv2.cvtColor(rgb_frame, cv2.COLOR_RGB2BGR)

            if frame_count % SKIP_FRAMES == 0:
                blob = cv2.resize(rgb_frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))
                blob = blob.astype(np.float32) / 255.0
                blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

                outputs = session.run(None, {input_name: blob})
                preds = np.squeeze(outputs[0]).T

                boxes, scores, class_ids = [], [], []
                for p in preds:
                    class_scores = p[4:]
                    class_id = int(np.argmax(class_scores))
                    score = float(class_scores[class_id])
                    if score > CONF_THRESH and class_id in TARGET_CLASSES:
                        cx, cy, wb, hb = p[0:4]
                        x1 = int((cx - wb / 2) * (CAM_WIDTH / MODEL_INPUT_SIZE))
                        y1 = int((cy - hb / 2) * (CAM_HEIGHT / MODEL_INPUT_SIZE))
                        w_box = int(wb * (CAM_WIDTH / MODEL_INPUT_SIZE))
                        h_box = int(hb * (CAM_HEIGHT / MODEL_INPUT_SIZE))
                        boxes.append([x1, y1, w_box, h_box])
                        scores.append(score)
                        class_ids.append(class_id)

                indices = cv2.dnn.NMSBoxes(boxes, scores, CONF_THRESH, 0.4)

                detections = []
                if len(indices) > 0:
                    color_map = {
                        "car": (0, 0, 255),
                        "person": (0, 255, 0),
                        "motorcycle": (255, 0, 0),
                    }
                    for i in indices.flatten():
                        x1, y1, w_box, h_box = boxes[i]
                        x2, y2 = x1 + w_box, y1 + h_box
                        x1, y1 = max(0, x1), max(0, y1)
                        x2, y2 = min(CAM_WIDTH, x2), min(CAM_HEIGHT, y2)
                        class_name = TARGET_CLASSES[class_ids[i]]
                        detections.append((x1, y1, x2, y2, class_name, scores[i]))

                        color = color_map[class_name]
                        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                        cv2.putText(
                            frame, f"{class_name} {scores[i]:.2f}", (x1, y1 - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2,
                        )

                with _tl_lock:
                    tl_snapshot = dict(_traffic_light_state)

                tracked = tracker.track(detections)
                alerts, ped_payload, veh_payload = analyzer.analyze(tracked, tl_snapshot)

                # ── overlay على شباك الكاميرا ───────────────────
                cv2.putText(frame, f"Cars: {veh_payload['car_count']}", (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
                cv2.putText(frame, f"Persons: {ped_payload['pedestrian_count']}", (10, 55),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
                cv2.putText(frame, f"FPS: {fps}", (10, 80),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
                cv2.putText(frame, f"Alert: {ped_payload['alert_level']}", (10, 105),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 165, 255), 2)

                # ── طباعة حية في التيرمنال ──────────────────────
                print(
                    f"\r🚗 Cars: {veh_payload['car_count']:<2} | "
                    f"👤 Persons: {ped_payload['pedestrian_count']:<2} | "
                    f"⚠️  {ped_payload['alert_level']:<8} | "
                    f"FPS: {fps:<3} | "
                    f"Hub: {'✅' if connected else '❌'}   ",
                    end="",
                )

                # ── publish للـ hub (rate-limited) ──────────────
                now = time.time()
                if connected and now - last_pub >= PUB_INTERVAL:
                    last_pub = now
                    node.publish("pedestrian_status", ped_payload)
                    node.publish("vehicle_data", veh_payload)

            cv2.imshow("V2P System - Connected to Hub", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    except KeyboardInterrupt:
        print("\n\n🛑 Stopped by user")

    finally:
        picam2.stop()
        cv2.destroyAllWindows()
        print(f"\n✅ Done. Total frames: {frame_count}")


if __name__ == "__main__":
    main()
