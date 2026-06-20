
"""
V2P_laptop.py — V2P System (Laptop / PC Test Mode)
====================================================
Same as V2P.py on the Raspberry Pi but uses a VIDEO FILE or WEBCAM
instead of Picamera2. Publishes to the local IPC hub exactly like the
real node would, so the full pipeline (hub → dashboard_bridge → data.json
→ browser) can be tested on a laptop before touching hardware.

Controls
--------
  Q      — quit
  SPACE  — pause / resume
  R      — restart video from beginning

Run order (all in separate terminals):
  1. python3 hub.py
  2. python3 dashboard_bridge.py
  3. python3 server.py
  4. python3 V2P_laptop.py          ← this file
  (5. python3 Car_client-1.py       ← optional, for V2N data)
"""

import cv2
import numpy as np
import time
import threading

import onnxruntime as ort
from ipc_node import IPCNode          # local hub client

print("=" * 60)
print("🚗  V2P SYSTEM  —  Laptop / PC Test Mode")
print("=" * 60)

# ──────────────────────────────────────────────────────────────
# 1.  Configuration — change these to match your setup
# ──────────────────────────────────────────────────────────────
MODEL_PATH       = "yolov8n.onnx"
CONF_THRESH      = 0.30
MODEL_INPUT_SIZE = 640

# Video source: path to .mp4/.avi file, OR 0/1 for webcam
VIDEO_PATH = "test_video.mp4"   # ← change me

TARGET_CLASSES = {0: "person", 2: "car", 3: "motorcycle"}

# How often to publish to the hub (seconds)
PUB_INTERVAL = 0.5

# Pedestrian crossing zone (fraction of frame height)
CROSSING_Y_MIN = 0.35
CROSSING_Y_MAX = 0.75

# ──────────────────────────────────────────────────────────────
# 2.  IPC Hub connection
# ──────────────────────────────────────────────────────────────
ipc = IPCNode("v2p_camera")

_traffic_light_state = "RED"
_v2v_alert_active    = False
_ipc_lock            = threading.Lock()


def _on_traffic_light(topic, data, sender):
    global _traffic_light_state
    with _ipc_lock:
        _traffic_light_state = data.get("state", "RED")
    print(f"\n[V2P] 🚦 traffic_light → {_traffic_light_state}  (from {sender})")


def _on_v2v_alert(topic, data, sender):
    global _v2v_alert_active
    with _ipc_lock:
        _v2v_alert_active = bool(data.get("active", False))
    print(f"\n[V2P] ⚡ v2v_alert active={_v2v_alert_active}  (from {sender})")


print("\n🏠 Connecting to local IPC hub …")
hub_ok = ipc.connect()
if hub_ok:
    ipc.subscribe("traffic_light_state", _on_traffic_light)
    ipc.subscribe("v2v_alert",           _on_v2v_alert)
    ipc.start_listening()
    print("✅ Hub connected — subscribed to traffic_light_state & v2v_alert")
else:
    print("⚠️  Hub not found — running standalone (no hub publishing)")

# ──────────────────────────────────────────────────────────────
# 3.  Video source
# ──────────────────────────────────────────────────────────────
print(f"\n📷 Opening: {VIDEO_PATH}")
cap = cv2.VideoCapture(VIDEO_PATH)

if not cap.isOpened():
    print(f"❌ Cannot open: {VIDEO_PATH}")
    print("   Check the path or set VIDEO_PATH = 0 for webcam")
    raise SystemExit(1)

fps_video    = cap.get(cv2.CAP_PROP_FPS) or 30
VID_W        = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
VID_H        = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
is_file      = total_frames > 0

print(f"✅ {VID_W}×{VID_H}  {fps_video:.1f} FPS  "
      f"{'%d frames' % total_frames if is_file else 'live camera'}")

# ──────────────────────────────────────────────────────────────
# 4.  ONNX model
# ──────────────────────────────────────────────────────────────
print(f"\n📦 Loading: {MODEL_PATH}")
try:
    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 4      # more threads available on laptop
    session    = ort.InferenceSession(MODEL_PATH, sess_options=opts,
                                      providers=["CPUExecutionProvider"])
    input_name = session.get_inputs()[0].name
    print("✅ Model loaded!")
except Exception as exc:
    print(f"❌ Model load failed: {exc}")
    cap.release()
    raise SystemExit(1)

# ──────────────────────────────────────────────────────────────
# 5.  Helpers
# ──────────────────────────────────────────────────────────────
def alert_level(n_ped, n_moving, crossing_safe, light, v2v):
    if v2v or (n_ped > 0 and light == "GREEN" and not crossing_safe):
        return "danger"
    if n_ped > 0 and light != "RED":
        return "caution"
    if n_moving > 0:
        return "caution"
    return "none"


ALERT_COLOR_BGR = {"none": (0,220,80), "caution": (0,165,255), "danger": (0,50,255)}

# ──────────────────────────────────────────────────────────────
# 6.  Main loop
# ──────────────────────────────────────────────────────────────
frame_count   = 0
skip_frames   = 2          # run inference every N frames (speed vs accuracy)
fps_disp      = 0
fps_counter   = 0
fps_clock     = time.time()
_last_pub     = 0.0

# State carried across frames
last_cars_count    = 0
last_persons_count = 0
last_bikes_count   = 0
last_alert         = "none"
last_speed_kmh     = 0.0
prev_car_boxes     = []
prev_frame_time    = time.time()

paused = False

print("\n🎥 Running — Q to quit  |  SPACE to pause  |  R to restart\n")
print("-" * 60)

try:
    while True:

        # ── Read frame ──────────────────────────────────────────
        if not paused:
            ret, frame = cap.read()
            if not ret:
                if is_file:
                    print("\n📹 End of video — press R to restart or Q to quit")
                    paused = True
                    # keep last frame on screen; wait for keypress
                    key = cv2.waitKey(100) & 0xFF
                    if key == ord('q'):
                        break
                    elif key == ord('r'):
                        cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                        paused = False
                    continue
                else:
                    break   # webcam disconnected

            frame_count += 1
            fps_counter += 1
            now = time.time()
            if now - fps_clock >= 1.0:
                fps_disp    = fps_counter
                fps_counter = 0
                fps_clock   = now

        display = frame.copy()
        h, w    = display.shape[:2]

        # ── Inference (every skip_frames frames) ────────────────
        if not paused and frame_count % skip_frames == 0:

            blob = cv2.resize(frame, (MODEL_INPUT_SIZE, MODEL_INPUT_SIZE))
            blob = blob.astype(np.float32) / 255.0
            blob = blob.transpose(2, 0, 1)[np.newaxis, ...]

            outputs = session.run(None, {input_name: blob})
            preds   = np.squeeze(outputs[0]).T

            cars_count    = 0
            persons_count = 0
            bikes_count   = 0
            moving_count  = 0
            crossing_safe = True
            car_boxes_now = []

            boxes     = []
            scores    = []
            class_ids = []

            for p in preds:
                cs      = p[4:]
                cid     = int(np.argmax(cs))
                score   = float(cs[cid])
                if score > CONF_THRESH and cid in TARGET_CLASSES:
                    cx, cy, wb, hb = p[0:4]
                    x1    = int((cx - wb/2) * (w / MODEL_INPUT_SIZE))
                    y1    = int((cy - hb/2) * (h / MODEL_INPUT_SIZE))
                    wbox  = int(wb * (w / MODEL_INPUT_SIZE))
                    hbox  = int(hb * (h / MODEL_INPUT_SIZE))
                    boxes.append([x1, y1, wbox, hbox])
                    scores.append(score)
                    class_ids.append(cid)

            if boxes:
                idxs = cv2.dnn.NMSBoxes(boxes, scores, CONF_THRESH, 0.4)
                if len(idxs) > 0:
                    for i in idxs.flatten():
                        x1, y1, wbox, hbox = boxes[i]
                        x2, y2 = x1+wbox, y1+hbox
                        x1, y1 = max(0,x1), max(0,y1)
                        x2, y2 = min(w,x2), min(h,y2)
                        cid   = class_ids[i]
                        name  = TARGET_CLASSES[cid]

                        if name == "car":
                            cars_count += 1
                            car_boxes_now.append([x1,y1,wbox,hbox])
                            color = (0, 0, 255)
                        elif name == "person":
                            persons_count += 1
                            norm_cy = (y1 + hbox/2) / h
                            if CROSSING_Y_MIN <= norm_cy <= CROSSING_Y_MAX:
                                moving_count  += 1
                                crossing_safe  = False
                            color = (0, 255, 0)
                        else:
                            bikes_count += 1
                            color = (255, 0, 0)

                        cv2.rectangle(display, (x1,y1), (x2,y2), color, 2)
                        cv2.putText(display, f"{name} {scores[i]:.2f}",
                                    (x1, y1-10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

            # Speed estimate (centroid motion)
            dt = time.time() - prev_frame_time
            prev_frame_time = time.time()
            speed_kmh = 0.0
            if prev_car_boxes and car_boxes_now and dt > 0:
                pcy = prev_car_boxes[0][1] + prev_car_boxes[0][3]/2
                ccy = car_boxes_now[0][1]  + car_boxes_now[0][3]/2
                speed_kmh = round(min(abs(pcy-ccy)*0.003/dt*3.6, 120.0), 1)
            prev_car_boxes = car_boxes_now

            with _ipc_lock:
                light = _traffic_light_state
                v2v   = _v2v_alert_active

            alert = alert_level(persons_count, moving_count, crossing_safe, light, v2v)

            # Save for use between inference frames
            last_cars_count    = cars_count
            last_persons_count = persons_count
            last_bikes_count   = bikes_count
            last_alert         = alert
            last_speed_kmh     = speed_kmh

            # ── Publish to hub ───────────────────────────────────
            now = time.time()
            if hub_ok and now - _last_pub >= PUB_INTERVAL:
                _last_pub = now
                ipc.publish("pedestrian_status", {
                    "timestamp":        now,
                    "pedestrian_count": persons_count,
                    "moving_count":     moving_count,
                    "crossing_safe":    crossing_safe,
                    "alert_level":      alert,
                    "traffic_light":    light,
                })
                ipc.publish("vehicle_data", {
                    "timestamp":     now,
                    "speed_kmh":     speed_kmh,
                    "brake_pressed": False,
                    "car_count":     cars_count,
                    "moving_cars":   cars_count,
                    "bike_count":    bikes_count,
                    "ultrasonic":    {"front_cm": 999.0},
                })

        # ── Overlay panel (always draw, even on skipped frames) ──
        with _ipc_lock:
            light_now = _traffic_light_state

        LIGHT_CLR = {"RED":(0,50,255),"GREEN":(0,220,80),"YELLOW":(0,180,255)}
        ALERT_CLR = ALERT_COLOR_BGR.get(last_alert, (255,255,255))

        # semi-transparent sidebar
        overlay = display.copy()
        cv2.rectangle(overlay, (0, 0), (220, 200), (20,20,20), -1)
        cv2.addWeighted(overlay, 0.6, display, 0.4, 0, display)

        def txt(msg, y, color=(220,220,220), scale=0.55, thickness=1):
            cv2.putText(display, msg, (8, y), cv2.FONT_HERSHEY_SIMPLEX,
                        scale, color, thickness, cv2.LINE_AA)

        txt(f"Cars    : {last_cars_count}",    28,  (60,60,255),  0.58, 2)
        txt(f"Persons : {last_persons_count}", 56,  (60,220,60),  0.58, 2)
        txt(f"Bikes   : {last_bikes_count}",   84,  (255,90,50),  0.58, 2)
        txt(f"Speed   : {last_speed_kmh} km/h", 112)
        txt(f"FPS     : {fps_disp}",            140)
        txt(f"Light   : {light_now}",           168, LIGHT_CLR.get(light_now,(200,200,200)))
        txt(f"Alert   : {last_alert.upper()}",  196, ALERT_CLR, 0.62, 2)

        # Video progress bar (file mode only)
        if is_file and total_frames > 0:
            pct = frame_count / total_frames
            bar_w = int(w * pct)
            cv2.rectangle(display, (0, h-6), (w, h),   (40,40,40), -1)
            cv2.rectangle(display, (0, h-6), (bar_w,h),(0,200,100),-1)
            cv2.putText(display,
                        f"{frame_count}/{total_frames}  ({pct*100:.1f}%)",
                        (w-200, h-10), cv2.FONT_HERSHEY_SIMPLEX,
                        0.45, (200,200,200), 1, cv2.LINE_AA)

        if paused:
            cv2.putText(display, "PAUSED", (w//2-60, h//2),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.4, (0,80,255), 3, cv2.LINE_AA)

        # ── Terminal line ────────────────────────────────────────
        hub_tag = "📡 HUB" if hub_ok else "🔌 standalone"
        prog    = f"{frame_count}/{total_frames}" if is_file else str(frame_count)
        print(f"\r{hub_tag} | 🚗{last_cars_count} 👤{last_persons_count} "
              f"🏍️{last_bikes_count} | "
              f"Light:{light_now:<6} Alert:{last_alert:<7} | "
              f"FPS:{fps_disp} Frame:{prog}   ", end="", flush=True)

        cv2.imshow("V2P — Laptop Test Mode", display)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            print("\n\n🛑 Stopped by user")
            break
        elif key == ord(' '):
            paused = not paused
            print(f"\n{'⏸️  Paused' if paused else '▶️  Resumed'}")
        elif key == ord('r') and is_file:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            frame_count = 0
            paused = False
            print("\n🔄 Restarted")

except KeyboardInterrupt:
    print("\n\n🛑 Stopped by user")

finally:
    cap.release()
    cv2.destroyAllWindows()
    print(f"\n✅ Done.  Processed {frame_count} frames.")

