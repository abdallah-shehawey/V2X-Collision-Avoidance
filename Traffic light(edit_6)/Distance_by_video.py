# -*- coding: utf-8 -*-
"""Distance_by_video.py

Modified to run locally with video looping and upload real-time distance data to Intelligent Gateway via HiveMQ Cloud MQTT.
"""

import cv2
from ultralytics import YOLO
import easyocr
import re
import math
import csv
import os
import json
import time
import ssl
import paho.mqtt.client as mqtt

print("="*60)
print("🚗 LICENSE PLATE READER - WITH DISTANCE & MQTT PUBLISHING")
print("="*60)

# ============================================================
# 🌐 MQTT SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"

# Fixed Ambulance ID
AMBULANCE_ID = "T4RR"

# Setup secure MQTT connection
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set(USERNAME, PASSWORD)
mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

try:
    print("🌐 Connecting to HiveMQ Cloud Server...")
    mqtt_client.connect(BROKER, PORT, 60)
    mqtt_client.loop_start()
    print("✅ Successfully connected to MQTT Broker!")
except Exception as e:
    print(f"❌ MQTT Connection Failed: {e}")
    print("⚠️ The script will continue running locally, but won't send data to the Gateway.")

# ============================================================
# 🎥 VIDEO & MODEL CONFIGURATION
# ============================================================
# Using idv.mp4 as default video
video_name = "idv.mp4"

if not os.path.exists(video_name):
    print(f"❌ Error: The file '{video_name}' was not found!")
    # Allow user to input if default not found
    video_name = input("Enter your video file name (e.g., idv.mp4): ").strip()
    if not os.path.exists(video_name):
        exit()

video_path = video_name
cap = cv2.VideoCapture(video_path)

fps = int(cap.get(cv2.CAP_PROP_FPS)) if cap.get(cv2.CAP_PROP_FPS) > 0 else 30
width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

output_path = 'processed_output.mp4'
out = cv2.VideoWriter(output_path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (width, height))

car_detector = YOLO('yolov8n.pt')
reader = easyocr.Reader(['en'], gpu=False, verbose=False)

frame_count = 0
detected_plates = {}  
MIN_CAR_WIDTH = 150

# Distance Calculation Settings
FOCAL_LENGTH = 700          
KNOWN_PLATE_WIDTH = 0.45    

def calculate_distance(plate_width_px, focal_length=FOCAL_LENGTH, known_width=KNOWN_PLATE_WIDTH):
    if plate_width_px > 0:
        return (known_width * focal_length) / plate_width_px
    return float('inf')

print(f"\n🎥 Processing video: {video_name}")
print(f"📏 Distance measurement & MQTT streaming enabled (Looping Active)")
print(f"⌨️  To stop execution, press Ctrl+C in the terminal\n")

# Use a control flag for the loop
running = True

try:
    while running:
        ret, frame = cap.read()
        
        if not ret:
            print("\n🔄 Video ended. Restarting loop...")
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            ret, frame = cap.read()
            if not ret:
                break
                
        frame_count += 1

        # Process every 10 frames to optimize speed
        if frame_count % 10 == 0:
            results = car_detector(frame, verbose=False)

            for box in results[0].boxes:
                # Class 2 is 'car' in COCO dataset
                if int(box.cls[0]) == 2 and float(box.conf[0]) > 0.5:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    car_width = x2 - x1

                    if car_width > MIN_CAR_WIDTH:
                        # Look for plate in the lower 30% of the car box
                        plate_y1 = int(y2 - (y2 - y1) * 0.3)
                        plate_region = frame[plate_y1:y2, x1:x2]

                        if plate_region.size > 0:
                            gray = cv2.cvtColor(plate_region, cv2.COLOR_BGR2GRAY)
                            gray = cv2.equalizeHist(gray)
                            _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

                            result = reader.readtext(thresh, detail=0, paragraph=True)
                            if result:
                                cleaned = re.sub(r'[^A-Z0-9]', '', result[0].upper().replace(' ', ''))

                                # Basic plate validation
                                if len(cleaned) >= 4:
                                    confidence = min(95, 60 + len(cleaned) * 5)
                                    plate_width_px = x2 - x1
                                    distance = calculate_distance(plate_width_px)
                                    distance_text = f"{distance:.2f}m" if distance != float('inf') else "Unknown"

                                    # Check if it's the fixed ambulance ID
                                    is_ambulance = (cleaned == AMBULANCE_ID)
                                    
                                    if cleaned not in detected_plates:
                                        detected_plates[cleaned] = {
                                            'frame': frame_count,
                                            'confidence': confidence,
                                            'distance': distance  
                                        }
                                        status_icon = "🚑 AMBULANCE" if is_ambulance else "🚗 CAR"
                                        print(f"✅ {status_icon} DETECTED: {cleaned} | Distance: {distance_text}")

                                    # ============================================================
                                    # 📡 SEND DATA TO GATEWAY VIA MQTT (JSON)
                                    # ============================================================
                                    payload = {
                                        "plate_id": cleaned,
                                        "distance_m": round(distance, 2) if distance != float('inf') else 999.0,
                                        "is_ambulance": is_ambulance,
                                        "timestamp": int(time.time())
                                    }
                                    
                                    mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))
                                    
                                    # Only print every few detections to avoid spamming console
                                    if frame_count % 30 == 0:
                                        print(f"📤 Published to Gateway -> {cleaned} at {distance_text}")
                                    # ============================================================

                                    # Draw visual indicators for output video
                                    rect_color = (0, 0, 255) if is_ambulance else (0, 255, 0)
                                    cv2.rectangle(frame, (x1, y1), (x2, y2), rect_color, 3)
                                    cv2.putText(frame, f"{'AMBULANCE' if is_ambulance else 'Car'}: {cleaned}", (x1 + 5, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2)
                                    cv2.putText(frame, f"Dist: {distance_text}", (x1 + 5, y1 - 35), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)

        # 🚫 MODIFICATION: cv2.imshow was completely removed to avoid headless environment errors
        out.write(frame)

except KeyboardInterrupt:
    print("\n🛑 Execution stopped manually by user.")

finally:
    cap.release()
    out.release()
    cv2.destroyAllWindows()

    # Safe disconnect
    print("\nShutting down...")
    mqtt_client.loop_stop()
    mqtt_client.disconnect()

    print("\n" + "="*60)
    print("📊 FINAL SESSION RESULTS")
    print("="*60)
    for plate, info in detected_plates.items():
        dist_str = f"{info['distance']:.2f}m" if info['distance'] != float('inf') else "Unknown"
        print(f"    {'🚑' if plate == AMBULANCE_ID else '🚗'} {plate} - frame {info['frame']} - Distance: {dist_str}")

    # Save results to CSV
    csv_path = 'plate_detections_with_distance.csv'
    with open(csv_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['Plate', 'Frame', 'Confidence (%)', 'Distance (m)'])
        for plate, info in detected_plates.items():
            dist_str = f"{info['distance']:.2f}" if info['distance'] != float('inf') else "Unknown"
            writer.writerow([plate, info['frame'], f"{info['confidence']:.0f}", dist_str])

    print(f"\n📊 Results exported to: {os.path.abspath(csv_path)}")
    print("✅ Session complete!")