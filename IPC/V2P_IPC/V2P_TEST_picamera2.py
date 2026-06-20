"""
V2P_TEST.py  –  V2P Camera Node
=================================
Runs a YOLO-ONNX model on the live camera feed to detect pedestrians
and vehicles. The node subscribes to the traffic-light state so it can
factor V2I data into its safety analysis, then publishes two topics:

    "pedestrian_status"   – crowd / crossing data consumed by ADAS
    "vehicle_data"        – car count + estimated front distance consumed
                            by ADAS and V2V

IPC Role
--------
    Node name  : "v2p_camera"
    Subscribes : "traffic_light"          (from Traffic_Light_Simulator.py)
    Publishes  : "pedestrian_status"
                 "vehicle_data"

Published schema  ("pedestrian_status")
----------------------------------------
{
    "timestamp"       : float,
    "pedestrian_count": int,
    "moving_count"    : int,
    "crossing_safe"   : bool,
    "alert_level"     : "none" | "caution" | "danger",
    "traffic_light"   : str               # last known light state
}

Published schema  ("vehicle_data")
------------------------------------
{
    "timestamp"    : float,
    "speed_kmh"    : float,     # pixel-velocity proxy (0 if unknown)
    "brake_pressed": bool,
    "car_count"    : int,
    "moving_cars"  : int,
    "ultrasonic"   : { "front_cm": float }  # nearest car in px→cm estimate
}

Kernel-level IPC path
---------------------
    connect()   sys_connect()  → Unix Domain Socket to hub
    sendall()   sys_send()     → publish frames
    recv()      sys_recv()     → background thread receives traffic_light
"""

import cv2
import numpy as np
import time
import onnxruntime as ort
import os
import sys
from picamera2 import Picamera2

# ── Suppress OpenCV GUI (headless Raspberry Pi) ───────────────────────────────
os.environ["QT_QPA_PLATFORM"] = "offscreen"

# ── IPC ──────────────────────────────────────────────────────────────────────
from ipc_node import IPCNode

# ── Configuration ─────────────────────────────────────────────────────────────
NODE_NAME    = "v2p_camera"
MODEL_PATH   = "model.onnx"
CAM_WIDTH    = 640
CAM_HEIGHT   = 480
SKIP_FRAMES  = 3          # run inference every Nth frame

CONF_THRESH  = 0.30
PERSON_CLASS = 0
CAR_CLASS    = 2

# Approximate scale: 1 pixel ≈ 0.05 m at ~10 m distance (tune per camera)
PX_TO_M      = 0.05
FOCAL_CM     = 650 * 100  # focal_length in px × 100 → cm denominator

# ── Shared traffic-light state (updated by IPC callback) ─────────────────────
_traffic_light_state: dict = {
    "state":         "RED",
    "remaining_sec": 0,
    "is_emergency":  False,
    "zone":          "unknown",
}


# ═══════════════════════════════════════════════════════════════════════════════
# 1.  Object Tracker
# ═══════════════════════════════════════════════════════════════════════════════

class Tracker:
    """Simple centroid tracker with 1-second object lifetime."""

    def __init__(self) -> None:
        self.objects: dict = {}
        self.id_counter: int = 0

    def track(self, detections: list) -> dict:
        now = time.time()
        new_tracked: dict = {}

        for (x1, y1, x2, y2, obj_type, conf) in detections:
            cx, cy = (x1 + x2) // 2, (y1 + y2) // 2

            match_id = None
            for oid, data in self.objects.items():
                if np.hypot(cx - data["c"][0], cy - data["c"][1]) < 60:
                    match_id = oid
                    break

            if match_id is not None:
                old_cx = self.objects[match_id]["c"][0]
                new_tracked[match_id] = {
                    "b":    (x1, y1, x2, y2),
                    "c":    (cx, cy),
                    "v":    cx - old_cx,   # horizontal pixel velocity
                    "type": obj_type,
                    "conf": conf,
                    "t":    now,
                }
            else:
                new_tracked[self.id_counter] = {
                    "b":    (x1, y1, x2, y2),
                    "c":    (cx, cy),
                    "v":    0,
                    "type": obj_type,
                    "conf": conf,
                    "t":    now,
                }
                self.id_counter += 1

        # Expire objects not seen in the last second
        self.objects = {
            k: v for k, v in new_tracked.items()
            if now - v["t"] < 1.0
        }
        return self.objects


# ═══════════════════════════════════════════════════════════════════════════════
# 2.  Safety Analyser
# ═══════════════════════════════════════════════════════════════════════════════

class SafetyAnalyzer:
    """
    Fuses camera detections with the live traffic-light state to produce
    alerts and structured payloads for the IPC hub.
    """

    def analyze(
        self,
        tracked_objects: dict,
        tl_state: dict,
    ) -> tuple[list, list, list, dict, dict]:
        """
        Returns
        -------
        alerts             : list of alert dicts for console display
        pedestrians        : list of pedestrian summaries
        cars               : list of car summaries
        pedestrian_payload : dict to publish on "pedestrian_status"
        vehicle_payload    : dict to publish on "vehicle_data"
        """
        pedestrians = []
        cars        = []

        for oid, data in tracked_objects.items():
            v        = data["v"]
            obj_type = data["type"]
            moving   = abs(v) > 2

            if obj_type == "person":
                pedestrians.append({
                    "id":     oid,
                    "moving": moving,
                    "speed":  abs(v),
                })
            elif obj_type == "car":
                # Estimate nearest car distance from bounding-box width
                x1, y1, x2, y2 = data["b"]
                box_w = max(x2 - x1, 1)
                # rough: front_cm = real_width(180cm) * focal / box_px
                front_cm_est = round((180.0 * 650) / box_w, 1)
                cars.append({
                    "id":       oid,
                    "moving":   moving,
                    "speed":    abs(v),
                    "front_cm": front_cm_est,
                })

        moving_cars   = [c for c in cars   if c["moving"]]
        moving_people = [p for p in pedestrians if p["moving"]]
        light         = tl_state.get("state", "UNKNOWN")
        is_emerg      = tl_state.get("is_emergency", False)
        remaining     = tl_state.get("remaining_sec", 0)

        # ── Alert generation ──────────────────────────────────────────
        alerts = []

        if is_emerg:
            alerts.append({
                "level":   "EMERGENCY",
                "message": "🚨 EMERGENCY VEHICLE — clear the road immediately!",
            })

        if light == "RED":
            alerts.append({
                "level":   "STOP",
                "message": f"🔴 RED LIGHT — stop! ({remaining}s remaining)",
            })
        elif light == "YELLOW":
            alerts.append({
                "level":   "CAUTION",
                "message": f"🟡 YELLOW LIGHT — prepare to stop ({remaining}s)",
            })
        elif light == "GREEN":
            alerts.append({
                "level":   "INFO",
                "message": f"🟢 GREEN LIGHT — you may proceed ({remaining}s)",
            })

        if moving_cars and moving_people:
            alerts.append({
                "level":   "WARNING",
                "message": (
                    f"⚠️ {len(moving_cars)} car(s) + "
                    f"{len(moving_people)} pedestrian(s) moving simultaneously!"
                ),
            })

        if len(pedestrians) > 3:
            alerts.append({
                "level":   "CAUTION",
                "message": f"⚠️ High pedestrian density: {len(pedestrians)} people",
            })

        if len(cars) > 5:
            alerts.append({
                "level":   "CAUTION",
                "message": f"⚠️ Heavy traffic: {len(cars)} vehicles detected",
            })

        # ── Determine crossing safety & alert level ───────────────────
        has_danger  = any(a["level"] in ("WARNING", "EMERGENCY") for a in alerts)
        has_caution = any(a["level"] == "CAUTION" for a in alerts)

        if has_danger:
            alert_level   = "danger"
            crossing_safe = False
        elif has_caution:
            alert_level   = "caution"
            crossing_safe = True
        else:
            alert_level   = "none"
            crossing_safe = True

        # ── IPC payloads ──────────────────────────────────────────────
        pedestrian_payload = {
            "timestamp":        time.time(),
            "pedestrian_count": len(pedestrians),
            "moving_count":     len(moving_people),
            "crossing_safe":    crossing_safe,
            "alert_level":      alert_level,
            "traffic_light":    light,
        }

        # Nearest car's estimated front_cm (999 = no car)
        nearest_front_cm = min(
            (c["front_cm"] for c in cars), default=999.0
        )
        # Pixel-velocity to rough km/h (very approximate; replace with real
        # speed sensor data on the actual vehicle).
        avg_car_speed_px = (
            sum(c["speed"] for c in moving_cars) / len(moving_cars)
            if moving_cars else 0
        )
        speed_kmh_est = round(avg_car_speed_px * PX_TO_M * 3.6, 1)

        vehicle_payload = {
            "timestamp":    time.time(),
            "speed_kmh":    speed_kmh_est,
            "brake_pressed": light == "RED",   # proxy: assume brake at red
            "car_count":    len(cars),
            "moving_cars":  len(moving_cars),
            "ultrasonic":   {"front_cm": nearest_front_cm},
        }

        return alerts, pedestrians, cars, pedestrian_payload, vehicle_payload


# ═══════════════════════════════════════════════════════════════════════════════
# 3.  Console Display
# ═══════════════════════════════════════════════════════════════════════════════

class Display:
    def __init__(self) -> None:
        self._last = 0.0

    def show(
        self,
        alerts:      list,
        pedestrians: list,
        cars:        list,
        tl_state:    dict,
    ) -> None:
        if time.time() - self._last < 0.5:
            return
        self._last = time.time()

        os.system("cls" if os.name == "nt" else "clear")

        light    = tl_state.get("state", "?")
        rem      = tl_state.get("remaining_sec", 0)
        emerg    = tl_state.get("is_emergency", False)
        light_icon = {"RED": "🔴", "GREEN": "🟢", "YELLOW": "🟡"}.get(light, "⚫")

        print("=" * 58)
        print("🟢 V2P CAMERA NODE  —  connected to IPC hub")
        print("=" * 58)
        print(
            f"🚶 Pedestrians : {len(pedestrians):>3}  |  "
            f"🚗 Cars : {len(cars):>3}"
        )
        print(
            f"{light_icon} Traffic Light : {light:<7}  "
            f"({rem}s remaining)"
            + ("  🚨 EMERGENCY" if emerg else "")
        )
        print("-" * 58)

        if alerts:
            print("\n📢  ALERTS:")
            level_icons = {
                "EMERGENCY": "🚨",
                "STOP":      "🛑",
                "WARNING":   "🟡",
                "CAUTION":   "🟠",
                "INFO":      "🔵",
            }
            for alert in alerts:
                icon = level_icons.get(alert["level"], "⬜")
                print(f"   {icon}  {alert['message']}")
        else:
            print("\n   ✅  All safe — no alerts")

        print("\n   📡  Publishing  →  pedestrian_status  |  vehicle_data")
        print("=" * 58)
        print("   Press Ctrl+C to stop")


# ═══════════════════════════════════════════════════════════════════════════════
# 4.  IPC Traffic-Light Callback
# ═══════════════════════════════════════════════════════════════════════════════

def _on_traffic_light(topic: str, data: dict, sender: str) -> None:
    """Update shared traffic-light state whenever hub delivers a new frame."""
    global _traffic_light_state
    _traffic_light_state = data
    state   = data.get("state", "?")
    rem     = data.get("remaining_sec", "?")
    emerg   = data.get("is_emergency", False)
    print(
        f"[V2P] traffic_light update  : "
        f"state={state}  remaining={rem}s"
        + ("  🚨 EMERGENCY" if emerg else "")
    )


# ═══════════════════════════════════════════════════════════════════════════════
# 5.  Main
# ═══════════════════════════════════════════════════════════════════════════════

def main() -> None:
    print("\n" + "=" * 58)
    print("🚀 Starting V2P Camera Node")
    print("=" * 58 + "\n")

    # ── Connect to IPC hub ────────────────────────────────────────────
    node = IPCNode(NODE_NAME)
    if not node.connect():
        print("[V2P] ❌  Could not connect to hub. Is hub.py running?")
        sys.exit(1)

    # Subscribe to traffic-light topic (V2I data from ESP32 via V2N)
    node.subscribe("traffic_light", _on_traffic_light)
    node.start_listening()   # background thread handles inbound frames

    print(f"[V2P] Node '{NODE_NAME}' ready")
    print("[V2P] Subscribed to  : 'traffic_light'")
    print("[V2P] Publishes to   : 'pedestrian_status', 'vehicle_data'\n")

    # ── Open camera ───────────────────────────────────────────────────
    print("[V2P] Opening camera …")
    cap = Picamera2()
    config = cap.create_preview_configuration(
        main={"size": (CAM_WIDTH, CAM_HEIGHT), "format": "RGB888"}
    )
    cap.configure(config)
    cap.start()
    time.sleep(1.0)   # انتظر الكاميرا تتهيأ
    print("[V2P] ✅  PiCamera2 opened successfully")

    # ── Load ONNX model ───────────────────────────────────────────────
    print("[V2P] Loading YOLO model …")
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 2
    session    = ort.InferenceSession(
        MODEL_PATH, sess_options=opts,
        providers=["CPUExecutionProvider"]
    )
    input_name = session.get_inputs()[0].name
    print(f"[V2P] ✅  Model loaded: {MODEL_PATH}\n")

    # ── Initialise components ─────────────────────────────────────────
    tracker  = Tracker()
    analyzer = SafetyAnalyzer()
    display  = Display()

    frame_count  = 0
    pub_interval = 0.5          # publish to hub at most every 0.5 s
    last_pub     = 0.0

    print("✅  System ready — watching road …\n")

    try:
        while True:
            # Picamera2 بترجع numpy array مباشرة بصيغة RGB
            frame_rgb = cap.capture_array()
            # نحوّل من RGB لـ BGR عشان OpenCV والـ ONNX model متعودين عليها
            frame = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2BGR)

            frame_count += 1

            # ── Inference every SKIP_FRAMES ───────────────────────────
            if frame_count % SKIP_FRAMES != 0:
                time.sleep(0.01)
                continue

            # ── Pre-process ───────────────────────────────────────────
            blob = cv2.resize(frame, (416, 416))
            blob = blob.astype(np.float32) / 255.0
            blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

            outputs = session.run(None, {input_name: blob})
            preds   = np.squeeze(outputs[0]).T

            # ── Post-process predictions ──────────────────────────────
            detections = []
            for p in preds:
                if len(p) < 5 or p[4] <= CONF_THRESH:
                    continue
                cx, cy, w, h = p[0:4]
                class_id = int(p[5]) if len(p) > 5 else -1

                x1 = int((cx - w / 2) * (CAM_WIDTH  / 416))
                y1 = int((cy - h / 2) * (CAM_HEIGHT / 416))
                x2 = int((cx + w / 2) * (CAM_WIDTH  / 416))
                y2 = int((cy + h / 2) * (CAM_HEIGHT / 416))
                x1, y1 = max(0, x1), max(0, y1)
                x2, y2 = min(CAM_WIDTH, x2), min(CAM_HEIGHT, y2)

                if class_id == PERSON_CLASS and (x2 - x1) > 20 and (y2 - y1) > 40:
                    detections.append((x1, y1, x2, y2, "person", p[4]))
                elif class_id == CAR_CLASS and (x2 - x1) > 40 and (y2 - y1) > 30:
                    detections.append((x1, y1, x2, y2, "car", p[4]))

            # ── Track & analyse ───────────────────────────────────────
            tracked = tracker.track(detections)
            (
                alerts,
                pedestrians,
                cars,
                ped_payload,
                veh_payload,
            ) = analyzer.analyze(tracked, _traffic_light_state)

            # ── Display ───────────────────────────────────────────────
            display.show(alerts, pedestrians, cars, _traffic_light_state)

            # ── Publish to hub (rate-limited) ─────────────────────────
            now = time.time()
            if now - last_pub >= pub_interval:
                last_pub = now

                node.publish("pedestrian_status", ped_payload)
                print(
                    f"[V2P] publish → 'pedestrian_status' | "
                    f"peds={ped_payload['pedestrian_count']} "
                    f"alert={ped_payload['alert_level']} "
                    f"light={ped_payload['traffic_light']}"
                )

                node.publish("vehicle_data", veh_payload)
                print(
                    f"[V2P] publish → 'vehicle_data'       | "
                    f"cars={veh_payload['car_count']} "
                    f"front={veh_payload['ultrasonic']['front_cm']}cm"
                )

            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n\n" + "=" * 58)
        print("🛑  V2P Camera Node stopped.")
        print("=" * 58)

    finally:
        cap.stop()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
