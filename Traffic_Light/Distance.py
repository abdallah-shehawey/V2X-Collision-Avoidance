# -*- coding: utf-8 -*-
"""
Author: Eng. Gamila
Date: June 2026
Description: Edge node processing framework utilizing YOLOv8 and EasyOCR to extract license plates, 
             calculate optical distance triangulation, and handle emergency preemption camera loops.
"""

import cv2
from ultralytics import YOLO
import easyocr
import re
import csv
import os
import json
import time
import ssl
import paho.mqtt.client as mqtt

print("="*60)
print("🚗 LICENSE PLATE READER - DISTANCE FIXED + PAUSE ON AMBULANCE")
print("="*60)

# ============================================================
# 🌐 MQTT SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
# Shared broker + ambulance identity (single source of truth, env-overridable).
from v2x_config import BROKER, PORT, USERNAME, PASSWORD, AMBULANCE_ID
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"


def _on_connect(client, userdata, flags, reason_code, properties=None):
    # Authentication is confirmed here (in CONNACK), not by connect() returning.
    # Without this, a wrong password still prints "connected" and publishes into the void.
    if reason_code == 0:
        print("✅ MQTT connected & authenticated.")
    else:
        print(f"❌ MQTT auth/connect failed: {reason_code}")


mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set(USERNAME, PASSWORD)
mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
mqtt_client.on_connect = _on_connect

try:
    print("🌐 Connecting to HiveMQ Cloud Server...")
    mqtt_client.connect(BROKER, PORT, 60)
    mqtt_client.loop_start()
    print("🌐 MQTT connecting … (result reported by callback)")
except Exception as e:
    print(f"❌ MQTT Connection Failed: {e}")

# ============================================================
# 🎥 VIDEO & MODEL CONFIGURATION
# ============================================================
video_name = "idv.mp4"
if not os.path.exists(video_name):
    video_name = input("Enter your video file name: ").strip()
    if not os.path.exists(video_name):
        exit()

cap = cv2.VideoCapture(video_name)
fps = int(cap.get(cv2.CAP_PROP_FPS)) if cap.get(cv2.CAP_PROP_FPS) > 0 else 30
width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

output_path = 'processed_output.mp4'
out = cv2.VideoWriter(output_path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (width, height))
# The loop replays the video forever for continuous MQTT publishing, but we only
# record ONE pass to disk — otherwise processed_output.mp4 grows until the SD card fills.
recording = True

car_detector = YOLO('yolov8n.pt')
reader = easyocr.Reader(['en'], gpu=False, verbose=False)

frame_count = 0
detected_plates = {}
MIN_CAR_WIDTH = 150

# ============================================================
# 🎯 DISTANCE CALIBRATION CONSTANTS (Based on Plate Width)
# ============================================================
FOCAL_LENGTH = 700          # Camera focal length constant (approx. equivalent to a 4mm lens)
KNOWN_PLATE_WIDTH = 0.45    # Standard physical license plate width in meters (45 cm)
MIN_OCR_CONF = 0.35         # Minimum EasyOCR confidence to accept a read (matches dis&AI_camera.py)

def calculate_distance(plate_width_px, focal_length=FOCAL_LENGTH, known_width=KNOWN_PLATE_WIDTH):
    """
    Computes distance using pinhole camera model triangulation based on plate dimensions.
    """
    if plate_width_px > 0:
        return (known_width * focal_length) / plate_width_px
    return float('inf')

# ============================================================
# 🕒 CAMERA STANDBY CONTROLS FOR EMERGENCY VEHICLES
# ============================================================
pause_mode = False
pause_frames_remaining = 0
SPEED_ASSUMED = 5.0  # Assumed vehicle approach velocity (meters/second)

print(f"\n🎥 Processing video: {video_name}")
print(f"🚑 Ambulance ID: {AMBULANCE_ID} (System will temporarily enter standby mode when detected)")
print("📏 Distance is calculated dynamically using bounding plate width dimensions")
print("⌨️  Press Ctrl+C to stop execution safely\n")

running = True
try:
    while running:
        ret, frame = cap.read()
        if not ret:
            print("\n🔄 Video ended. Restarting loop...")
            if recording:
                recording = False
                out.release()   # first full pass recorded — stop growing the file
                print(f"💾 Recording finished: {output_path}")
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ret, frame = cap.read()
            if not ret:
                break

        frame_count += 1

        # ============================================================
        # 🚫 STANDBY MODE (Bypass frame processing during active preemption)
        # ============================================================
        if pause_mode:
            pause_frames_remaining -= 1
            if recording:
                out.write(frame)
            if pause_frames_remaining <= 0:
                pause_mode = False
                print("🔓 Camera processing resumed.")
            continue

        # ============================================================
        # 🔍 VISUAL PROCESSING PIPELINE (Optimized frame sampling loop)
        # ============================================================
        if frame_count % 10 == 0:
            results = car_detector(frame, verbose=False)

            for box in results[0].boxes:
                # Class 2: Car target identification under validation threshold
                if int(box.cls[0]) == 2 and float(box.conf[0]) > 0.5:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    car_width = x2 - x1

                    if car_width > MIN_CAR_WIDTH:
                        # Crop License Plate Region (Targeting the bottom 30% of the vehicle box)
                        plate_y1 = int(y2 - (y2 - y1) * 0.3)
                        plate_region = frame[plate_y1:y2, x1:x2]

                        if plate_region.size > 0:
                            # Apply structural lighting normalization before text extraction
                            gray = cv2.cvtColor(plate_region, cv2.COLOR_BGR2GRAY)
                            gray = cv2.equalizeHist(gray)
                            _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

                            # ============================================================
                            # ✅ CHARACTER MAP EXTRACTION (Enforcing spatial geometry mapping)
                            # ============================================================
                            ocr_results = reader.readtext(thresh, detail=1, paragraph=False)

                            for (bbox, text, conf) in ocr_results:
                                cleaned = re.sub(r'[^A-Z0-9]', '', text.upper().replace(' ', ''))
                                # Use the REAL OCR confidence returned by EasyOCR and drop
                                # low-confidence noise, instead of faking a score from the
                                # string length (which reported 80–95% for any garbage read).
                                if len(cleaned) >= 4 and conf >= MIN_OCR_CONF:
                                    # Deduce physical bounding metric width parameters
                                    x_coords = [point[0] for point in bbox]
                                    plate_width_px = max(x_coords) - min(x_coords)

                                    # Reject micro-box artifacts or parsing exceptions
                                    if plate_width_px < 10:
                                        continue

                                    confidence = round(conf * 100)
                                    distance = calculate_distance(plate_width_px)
                                    distance_text = f"{distance:.2f}m" if distance != float('inf') else "Unknown"
                                    is_ambulance = (cleaned == AMBULANCE_ID)

                                    # Log entry to local unique registry for data evaluation
                                    if cleaned not in detected_plates:
                                        detected_plates[cleaned] = {
                                            'frame': frame_count,
                                            'confidence': confidence,
                                            'distance': distance
                                        }
                                        status_icon = "🚑 AMBULANCE" if is_ambulance else "🚗 CAR"
                                        print(f"✅ {status_icon} DETECTED: {cleaned} | Distance: {distance_text} (Plate Width: {plate_width_px}px)")

                                    # ============================================================
                                    # 📡 V2X CLOUD INFRASTRUCTURE SYNCHRONIZATION
                                    # ============================================================
                                    payload = {
                                        "plate_id": cleaned,
                                        "distance_m": round(distance, 2) if distance != float('inf') else 999.0,
                                        "is_ambulance": is_ambulance,
                                        "timestamp": int(time.time())
                                    }
                                    mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))

                                    if frame_count % 30 == 0:
                                        print(f"📤 Published -> {cleaned} at {distance_text}")

                                    # ============================================================
                                    # 🚑 INTERSECT PREEMPTION ACTIVATION
                                    # ============================================================
                                    if is_ambulance and distance != float('inf'):
                                        pause_seconds = max(2.0, min(20.0, distance / SPEED_ASSUMED))
                                        pause_frames = int(pause_seconds * fps)
                                        pause_mode = True
                                        pause_frames_remaining = pause_frames
                                        print(f"⏳ Ambulance at {distance_text} → Pausing for {pause_seconds:.1f}s ({pause_frames} frames)")

                                    # ============================================================
                                    # 🎨 TELEMETRY GRAPHICS OVERLAY MANAGEMENT
                                    # ============================================================
                                    rect_color = (0, 0, 255) if is_ambulance else (0, 255, 0)
                                    cv2.rectangle(frame, (x1, y1), (x2, y2), rect_color, 3)
                                    cv2.putText(frame, f"{'AMBULANCE' if is_ambulance else 'Car'}: {cleaned}", (x1 + 5, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
                                    cv2.putText(frame, f"Dist: {distance_text}", (x1 + 5, y1 - 35), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

                                    # Conclude nested iterations upon successful structural processing
                                    break

        if recording:
            out.write(frame)

except KeyboardInterrupt:
    print("\n🛑 Stopped by user.")

finally:
    cap.release()
    out.release()
    cv2.destroyAllWindows()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()

    print("\n" + "="*60)
    print("📊 FINAL RESULTS")
    print("="*60)
    for plate, info in detected_plates.items():
        dist_str = f"{info['distance']:.2f}m" if info['distance'] != float('inf') else "Unknown"
        print(f"    {'🚑' if plate == AMBULANCE_ID else '🚗'} {plate} - Frame {info['frame']} - Distance: {dist_str}")

    csv_path = 'plate_detections_with_distance.csv'
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['Plate', 'Frame', 'Confidence (%)', 'Distance (m)'])
        for plate, info in detected_plates.items():
            dist_str = f"{info['distance']:.2f}" if info['distance'] != float('inf') else "Unknown"
            writer.writerow([plate, info['frame'], f"{info['confidence']:.0f}", dist_str])

    print(f"\n📊 Results saved to: {os.path.abspath(csv_path)}")
    print("✅ Done!")