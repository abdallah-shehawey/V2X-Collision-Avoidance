
"""distance_after_edit.py - Clean version with no -1, no Unknown, all English"""


import cv2
from ultralytics import YOLO
import easyocr
import re
import csv
import os
import sys
import json
import time
import ssl
import paho.mqtt.client as mqtt

print("="*60)
print("VEHICLE LICENSE PLATE READER - DISTANCE FIXED + PAUSE ON AMBULANCE")
print("="*60)

# ============================================================
# LANE IDENTITY (matches the Traffic_light_GUI.py instance this camera
# watches, so Intelligent_Gateway.py's Conflict Resolution Module knows
# which lane an approaching ambulance is actually in)
# ============================================================
LANE_ID = os.environ.get("LANE_ID") or (sys.argv[1] if len(sys.argv) > 1 else "A")

# ============================================================
# MQTT SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"

AMBULANCE_ID = "T4RR"

def _on_mqtt_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("Successfully connected to MQTT Broker!")
    else:
        print(f"MQTT authentication/connection rejected by broker (reason code: {reason_code})")


def _on_mqtt_disconnect(client, userdata, flags, reason_code=None, properties=None):
    print(f"MQTT disconnected (reason code: {reason_code})")


mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set(USERNAME, PASSWORD)
mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
mqtt_client.on_connect = _on_mqtt_connect
mqtt_client.on_disconnect = _on_mqtt_disconnect

try:
    print("Connecting to HiveMQ Cloud Server...")
    # connect() only raises on network-level failures (unreachable host, etc).
    # Auth failures are reported asynchronously via on_connect's reason_code,
    # so the actual "connected" confirmation is printed there, not here.
    mqtt_client.connect(BROKER, PORT, 60)
    mqtt_client.loop_start()
except Exception as e:
    print(f"MQTT Connection Failed: {e}")

# ============================================================
# VIDEO & MODEL CONFIGURATION
# ============================================================
video_name = "/content/WhatsApp Video 2026-06-18 at 7.35.56 PM.mp4"
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

car_detector = YOLO('yolov8n.pt')
reader = easyocr.Reader(['en'], gpu=False, verbose=False)

frame_count = 0
detected_plates = {}
MIN_CAR_WIDTH = 150

# ============================================================
# DISTANCE CALIBRATION (Plate width, not car width)
# ============================================================
FOCAL_LENGTH = 700
KNOWN_PLATE_WIDTH = 0.45

def calculate_distance(plate_width_px, focal_length=FOCAL_LENGTH, known_width=KNOWN_PLATE_WIDTH):
    if plate_width_px > 0:
        return (known_width * focal_length) / plate_width_px
    return None  # <-- Returns None instead of -1 or inf

# ============================================================
# PAUSE CONTROL FOR AMBULANCE
# ============================================================
pause_mode = False
pause_frames_remaining = 0
SPEED_ASSUMED = 5.0

# While paused, YOLO/OCR stop running entirely, so nothing would otherwise be
# republished. Traffic_light_GUI.py's own AMBULANCE_PRESENCE_TIMEOUT is 5s, but
# a pause can last up to 20s - without this, the light (or the gateway) would
# conclude the ambulance left mid-pause even though it's still approaching.
AMBULANCE_REPUBLISH_INTERVAL = 2.0
last_ambulance_payload = None
last_republish_time = 0.0

print(f"\nProcessing video: {video_name}")
print(f"Ambulance ID: {AMBULANCE_ID} (Temporary camera pause when detected)")
print("Distance is now calculated from actual plate width, not car width.")
print("Press Ctrl+C to stop.\n")

running = True
has_looped = False
try:
    while running:
        ret, frame = cap.read()
        if not ret:
            print("\nVideo ended. Restarting loop...")
            has_looped = True
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ret, frame = cap.read()
            if not ret:
                break

        frame_count += 1

        # ============================================================
        # PAUSE MODE: Skip processing
        # ============================================================
        if pause_mode:
            pause_frames_remaining -= 1
            if not has_looped:
                out.write(frame)
            if (last_ambulance_payload is not None
                    and time.time() - last_republish_time >= AMBULANCE_REPUBLISH_INTERVAL):
                last_ambulance_payload["timestamp"] = int(time.time())
                mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(last_ambulance_payload))
                last_republish_time = time.time()
            if pause_frames_remaining <= 0:
                pause_mode = False
                print("Camera resumed.")
            continue

        # ============================================================
        # PROCESS EVERY 10 FRAMES
        # ============================================================
        if frame_count % 10 == 0:
            results = car_detector(frame, verbose=False)

            for box in results[0].boxes:
                # COCO classes: 2=car, 5=bus, 7=truck - real ambulances are
                # often box/van-bodied and get classified as bus/truck, not
                # "car"; restricting to class 2 alone can miss them entirely.
                if int(box.cls[0]) in (2, 5, 7) and float(box.conf[0]) > 0.5:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    car_width = x2 - x1

                    if car_width > MIN_CAR_WIDTH:
                        # Crop plate region (lower 30% of the car)
                        plate_y1 = int(y2 - (y2 - y1) * 0.3)
                        plate_region = frame[plate_y1:y2, x1:x2]

                        if plate_region.size > 0:
                            gray = cv2.cvtColor(plate_region, cv2.COLOR_BGR2GRAY)
                            gray = cv2.equalizeHist(gray)
                            _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

                            # Read text with bounding boxes (detail=1)
                            ocr_results = reader.readtext(thresh, detail=1, paragraph=False)

                            for (bbox, text, conf) in ocr_results:
                                cleaned = re.sub(r'[^A-Z0-9]', '', text.upper().replace(' ', ''))
                                if len(cleaned) >= 4:
                                    # Extract actual plate width in pixels from bbox coordinates
                                    x_coords = [point[0] for point in bbox]
                                    plate_width_px = max(x_coords) - min(x_coords)

                                    # Skip if plate is too small (likely false reading)
                                    if plate_width_px < 10:
                                        continue

                                    # Calculate distance - returns None if invalid
                                    distance = calculate_distance(plate_width_px)
                                    
                                    # ============================================================
                                    # ✅ SKIP if distance is invalid (no -1, no Unknown)
                                    # ============================================================
                                    if distance is None:
                                        continue

                                    # Real EasyOCR confidence (0-1) was already unpacked
                                    # above as `conf` - use it directly instead of a
                                    # fabricated string-length formula.
                                    confidence = round(conf * 100, 1)
                                    distance_text = f"{distance:.2f}m"
                                    is_ambulance = (cleaned == AMBULANCE_ID)
                                    detection_time = int(time.time())

                                    # Record first occurrence only
                                    if cleaned not in detected_plates:
                                        detected_plates[cleaned] = {
                                            'frame': frame_count,
                                            'confidence': confidence,
                                            'distance': distance,
                                            'timestamp': detection_time,
                                            'is_ambulance': is_ambulance
                                        }
                                        status_icon = "🚑 AMBULANCE" if is_ambulance else "🚗 CAR"
                                        print(f"{status_icon} DETECTED: {cleaned} | Distance: {distance_text} (Plate Width: {plate_width_px}px)")

                                    # ============================================================
                                    # SEND MQTT PAYLOAD
                                    # ============================================================
                                    payload = {
                                        "plate_id": cleaned,
                                        "distance_m": round(distance, 2),
                                        "is_ambulance": is_ambulance,
                                        "timestamp": detection_time,
                                        "lane_id": LANE_ID
                                    }
                                    mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))

                                    if frame_count % 30 == 0:
                                        print(f"Published -> {cleaned} at {distance_text}")

                                    # ============================================================
                                    # PAUSE CAMERA IF AMBULANCE
                                    # ============================================================
                                    if is_ambulance:
                                        pause_seconds = max(2.0, min(20.0, distance / SPEED_ASSUMED))
                                        pause_frames = int(pause_seconds * fps)
                                        pause_mode = True
                                        pause_frames_remaining = pause_frames
                                        last_ambulance_payload = dict(payload)
                                        last_republish_time = time.time()
                                        print(f"Ambulance at {distance_text} -> Pausing for {pause_seconds:.1f}s ({pause_frames} frames)")

                                    # ============================================================
                                    # DRAW ON VIDEO
                                    # ============================================================
                                    rect_color = (0, 0, 255) if is_ambulance else (0, 255, 0)
                                    cv2.rectangle(frame, (x1, y1), (x2, y2), rect_color, 3)
                                    cv2.putText(frame, f"{'AMBULANCE' if is_ambulance else 'Car'}: {cleaned}", (x1 + 5, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
                                    cv2.putText(frame, f"Dist: {distance_text}", (x1 + 5, y1 - 35), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
                                    break

        if not has_looped:
            out.write(frame)

except KeyboardInterrupt:
    print("\nStopped by user.")

finally:
    cap.release()
    out.release()
    cv2.destroyAllWindows()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()

    print("\n" + "="*60)
    print("FINAL RESULTS")
    print("="*60)
    for plate, info in detected_plates.items():
        print(f"    {'🚑' if plate == AMBULANCE_ID else '🚗'} {plate} - Frame {info['frame']} - Distance: {info['distance']:.2f}m")

    csv_path = 'plate_detections_with_distance.csv'
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['Plate', 'Frame', 'Confidence (%)', 'Distance (m)', 'Detection Time', 'Emergency Status'])
        for plate, info in detected_plates.items():
            detection_time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(info['timestamp']))
            emergency_status = 'AMBULANCE' if info['is_ambulance'] else 'NORMAL'
            writer.writerow([plate, info['frame'], f"{info['confidence']:.1f}", f"{info['distance']:.2f}",
                              detection_time_str, emergency_status])

    print(f"\nResults saved to: {os.path.abspath(csv_path)}")
    print("Done!")
