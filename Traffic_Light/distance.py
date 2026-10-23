# -*- coding: utf-8 -*-
"""
distance.py — V2X Camera Node (Raspberry Pi ready)
===================================================
Reads a video file (or webcam), detects cars with YOLOv8, reads the plate
with EasyOCR, estimates distance from the plate width, and publishes
{plate_id, distance_m, is_ambulance, timestamp} to the Intelligent Gateway
over MQTT.

Usage:
    python3 distance.py                 # uses VIDEO_FILE next to this script
    python3 distance.py my_video.mp4    # explicit file (relative or absolute)
    python3 distance.py 0               # webcam index 0

Changes vs the old version:
  * Video path is relative to this script (no Colab path, no input()).
  * AMBULANCE_ID unified with the gateway ("REX").
  * No video writing / no CSV (less load on the Pi).
  * While paused for an ambulance, a heartbeat is re-published every second,
    otherwise the gateway's 5-second stale cleanup would clear the emergency.
"""

import cv2
from ultralytics import YOLO
import easyocr
import re
import os
import sys
import json
import time
import ssl
import paho.mqtt.client as mqtt

# ============================================================
# CONFIG
# ============================================================
HERE       = os.path.dirname(os.path.abspath(__file__))
VIDEO_FILE = "idv.mp4"            # put the video next to this script

BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"

AMBULANCE_ID = "REX"              # MUST match AMBULANCE_ID in Intelligent_Gateway.py (upper-case)

FOCAL_LENGTH      = 700           # px, calibrate for your camera
KNOWN_PLATE_WIDTH = 0.45          # m
MIN_CAR_WIDTH     = 80            # px, ignore far/small cars
CAR_REAL_WIDTH_M  = 1.8           # m, used when the plate can't be read
CAR_FOCAL_PX      = 650           # px, fallback focal length
MAX_FALLBACK_DIST = 100           # m
PROCESS_EVERY_N   = 10            # run detection every N frames
SPEED_ASSUMED     = 5.0           # m/s, used for the ambulance pause time
HEARTBEAT_SEC     = 1.0

# ============================================================
# VIDEO SOURCE
# ============================================================
if len(sys.argv) > 1:
    src = sys.argv[1]
    source = int(src) if src.isdigit() else (src if os.path.isabs(src) else os.path.join(HERE, src))
else:
    source = os.path.join(HERE, VIDEO_FILE)

if isinstance(source, str) and not os.path.exists(source):
    print(f"Video not found: {source}")
    sys.exit(1)

cap = cv2.VideoCapture(source)
if not cap.isOpened():
    print(f"Cannot open video source: {source}")
    sys.exit(1)

fps = int(cap.get(cv2.CAP_PROP_FPS)) or 30

# ============================================================
# MQTT
# ============================================================
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set(USERNAME, PASSWORD)
mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

try:
    print("Connecting to HiveMQ Cloud...")
    mqtt_client.connect(BROKER, PORT, 60)
    mqtt_client.loop_start()
    time.sleep(1)
    print("MQTT connected.")
except Exception as e:
    print(f"MQTT connection failed: {e}")
    sys.exit(1)


def publish(plate_id, distance, is_ambulance):
    payload = {
        "plate_id":     plate_id,
        "distance_m":   round(float(distance), 2),
        "is_ambulance": bool(is_ambulance),
        "timestamp":    int(time.time()),
    }
    mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))

# ============================================================
# MODELS
# ============================================================
print("Loading YOLO + EasyOCR (first run downloads models)...")
car_detector = YOLO("yolov8n.pt")
reader = easyocr.Reader(["en"], gpu=False, verbose=False)


def calculate_distance(plate_width_px):
    if plate_width_px and plate_width_px > 0:
        return (KNOWN_PLATE_WIDTH * FOCAL_LENGTH) / plate_width_px
    return None

# ============================================================
# MAIN LOOP
# ============================================================
detected_plates = {}
frame_count = 0

print(f"\nSource: {source}")
print(f"Ambulance ID: {AMBULANCE_ID}")
print("Press Ctrl+C to stop.\n")

try:
    while True:
        ret, frame = cap.read()
        if not ret:
            if isinstance(source, int):
                print("Webcam read failed.")
                break
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)   # loop the video file
            continue

        frame_count += 1
        if frame_count % PROCESS_EVERY_N != 0:
            continue

        results = car_detector(frame, verbose=False)
        print(f"[frame {frame_count}] boxes detected: {len(results[0].boxes)}")

        for box in results[0].boxes:
            if int(box.cls[0]) != 2 or float(box.conf[0]) <= 0.5:
                continue

            x1, y1, x2, y2 = map(int, box.xyxy[0])
            if (x2 - x1) <= MIN_CAR_WIDTH:
                continue

            # plate area = lower 30% of the car box
            plate_y1 = int(y2 - (y2 - y1) * 0.3)
            plate_region = frame[plate_y1:y2, x1:x2]
            if plate_region.size == 0:
                continue

            gray = cv2.cvtColor(plate_region, cv2.COLOR_BGR2GRAY)
            gray = cv2.equalizeHist(gray)
            _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

            published = False
            for (bbox, text, conf) in reader.readtext(thresh, detail=1, paragraph=False):
                cleaned = re.sub(r"[^A-Z0-9]", "", text.upper().replace(" ", ""))
                if len(cleaned) < 4:
                    continue

                x_coords = [p[0] for p in bbox]
                plate_width_px = max(x_coords) - min(x_coords)
                if plate_width_px < 10:
                    continue

                distance = calculate_distance(plate_width_px)
                if distance is None:
                    continue

                is_ambulance = (cleaned == AMBULANCE_ID)

                if cleaned not in detected_plates:
                    icon = "AMBULANCE" if is_ambulance else "CAR"
                    print(f"[{icon}] {cleaned} | {distance:.2f} m (plate {plate_width_px:.0f}px)")
                detected_plates[cleaned] = {"frame": frame_count, "distance": distance}

                publish(cleaned, distance, is_ambulance)
                published = True

                # ── Ambulance: hold (time-based) and keep the heartbeat alive ──
                if is_ambulance:
                    pause_s = max(2.0, min(20.0, distance / SPEED_ASSUMED))
                    print(f"Ambulance at {distance:.1f} m -> hold {pause_s:.1f}s")
                    t_end = time.time() + pause_s
                    while time.time() < t_end:
                        publish(cleaned, distance, True)
                        time.sleep(HEARTBEAT_SEC)
                    print("Resumed.")
                break   # one plate per car

            # ── Fallback: plate unreadable -> distance from CAR width ──
            if not published:
                car_dist = (CAR_REAL_WIDTH_M * CAR_FOCAL_PX) / (x2 - x1)
                if car_dist <= MAX_FALLBACK_DIST:
                    publish("???", car_dist, False)
                    print(f"[CAR] plate unreadable | {car_dist:.1f} m (from car width)")

except KeyboardInterrupt:
    print("\nStopped by user.")

finally:
    cap.release()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    print("\nSummary:")
    for plate, info in detected_plates.items():
        print(f"  {plate}  frame {info['frame']}  {info['distance']:.2f} m")
