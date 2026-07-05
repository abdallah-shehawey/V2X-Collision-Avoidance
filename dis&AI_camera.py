# -*- coding: utf-8 -*-
"""
Author: Eng. Gamila
Date: June 2026
Description: Live vision processing framework using computational optimization loops to handle continuous webcam tracking.
"""

import cv2
from ultralytics import YOLO
import easyocr
import re
import numpy as np
import json
import time
import ssl
import paho.mqtt.client as mqtt

print("="*60)
print("🚗 REAL-TIME V2X CAMERA NODE (LOCAL LIVE MODE)")
print("="*60)

# ============================================================
# MQTT SERVER NETWORKING CONFIGURATION
# ============================================================
# Shared broker + ambulance identity (single source of truth, env-overridable).
from v2x_config import BROKER, PORT, USERNAME, PASSWORD, AMBULANCE_ID
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"


def _on_connect(client, userdata, flags, reason_code, properties=None):
    # Authentication is confirmed here (in CONNACK), not by connect() returning.
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
    print("⚠️ Script will execute locally without edge cloud sync capabilities.")

# ============================================================
# INITIALIZE DETECTION PIPELINES
# ============================================================
print("\n🔄 Initializing YOLO Model context parameters...")
model = YOLO('yolov8n.pt')

print("🔄 Spinning up independent character interpretation pipelines...")
reader = easyocr.Reader(['en'], gpu=False)

# Mathematical triangulation properties
KNOWN_WIDTH = 0.45    
FOCAL_LENGTH = 700.0  

def preprocess_for_ocr(roi_image):
    """
    Applies image preprocessing techniques to enhance text readability.
    Converts to grayscale, balances local illumination via equalization,
    and returns a clean thresholded matrix optimized for OCR parsing.
    """
    if roi_image.size == 0:
        return None
    gray = cv2.cvtColor(roi_image, cv2.COLOR_BGR2GRAY)
    equalized = cv2.equalizeHist(gray)
    _, threshed = cv2.threshold(equalized, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    return threshed


# Tracked vehicles registry
tracked_vehicles = []
vehicle_counter = 0

def update_tracking_and_ocr(frame, registry):
    global tracked_vehicles, vehicle_counter
    
    results = model(frame, verbose=False)
    current_detections = []
    
    # 1. Extract all vehicle bounding boxes in this frame
    for r in results:
        for box in r.boxes:
            cls_id = int(box.cls[0])
            if cls_id in [2, 5, 7]:  # car, bus, truck
                xyxy = box.xyxy[0].cpu().numpy()
                x1, y1, x2, y2 = map(int, xyxy)
                current_detections.append([x1, y1, x2, y2])
                
    # 2. Match with existing tracked vehicles using center distance
    updated_tracked = []
    for d_box in current_detections:
        d_x1, d_y1, d_x2, d_y2 = d_box
        cx_d = (d_x1 + d_x2) / 2
        cy_d = (d_y1 + d_y2) / 2
        
        best_match = None
        min_dist = float('inf')
        for v in tracked_vehicles:
            v_x1, v_y1, v_x2, v_y2 = v['bbox']
            cx_v = (v_x1 + v_x2) / 2
            cy_v = (v_y1 + v_y2) / 2
            
            dist = ((cx_d - cx_v) ** 2 + (cy_d - cy_v) ** 2) ** 0.5
            if dist < min_dist and dist < 120:  # matching threshold in pixels
                min_dist = dist
                best_match = v
                
        if best_match is not None:
            # Exclude match from tracked_vehicles temporarily to prevent double-matching
            if best_match in tracked_vehicles:
                tracked_vehicles.remove(best_match)
            best_match['bbox'] = d_box
            best_match['missed_frames'] = 0
            updated_tracked.append(best_match)
        else:
            # New vehicle detected
            vehicle_counter += 1
            new_v = {
                'id': vehicle_counter,
                'bbox': d_box,
                'plate': None,
                'distance': None,
                'is_ambulance': False,
                'missed_frames': 0
            }
            updated_tracked.append(new_v)
            
    # For tracked vehicles not in current frame, increment missed frames
    for v in tracked_vehicles:
        v['missed_frames'] += 1
        if v['missed_frames'] < 8:  # Allow surviving up to 8 frames (~1.5 seconds)
            updated_tracked.append(v)
            
    tracked_vehicles = updated_tracked
    
    # 3. For all tracked vehicles, attempt OCR and update distance dynamically
    for v in tracked_vehicles:
        x1, y1, x2, y2 = v['bbox']
        h = y2 - y1
        # Crop Plate Region (bottom 30% of target bounding box)
        plate_roi = frame[int(y1 + h*0.7):y2, x1:x2]
        processed_roi = preprocess_for_ocr(plate_roi)
        
        if processed_roi is not None:
            ocr_results = reader.readtext(processed_roi)
            for (bbox, text, prob) in ocr_results:
                cleaned = re.sub(r'[^A-Za-z0-9]', '', text).upper()
                if len(cleaned) >= 3 and prob > 0.35:
                    p_width_px = bbox[1][0] - bbox[0][0]
                    if p_width_px > 0:
                        calc_distance = (KNOWN_WIDTH * FOCAL_LENGTH) / p_width_px
                        is_ambulance = (cleaned == AMBULANCE_ID)
                        
                        # Update tracking state
                        v['plate'] = cleaned
                        v['distance'] = calc_distance
                        v['is_ambulance'] = is_ambulance
                        
                        # Log and registry
                        is_new = (cleaned not in registry)
                        if is_new:
                            registry[cleaned] = {'count': 1, 'distance': calc_distance}
                        else:
                            registry[cleaned]['count'] += 1
                            registry[cleaned]['distance'] = calc_distance
                            
                        # Terminal Print (matching Distance.py style)
                        status_icon = "🚑 AMBULANCE" if is_ambulance else "🚗 CAR"
                        print(f"✅ {status_icon} DETECTED: {cleaned} | Distance: {calc_distance:.2f}m (Plate Width: {p_width_px:.2f}px)")
                            
                        # Send MQTT payload
                        payload = {
                            "plate_id": cleaned,
                            "distance_m": round(float(calc_distance), 2),
                            "is_ambulance": bool(is_ambulance)
                        }
                        try:
                            mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))
                            print(f"📤 Published -> {cleaned} at {calc_distance:.2f}m")
                        except Exception as network_error:
                            print(f"⚠️ MQTT Transmission drop: {network_error}")
                    break  # OCR processing complete for this vehicle

def draw_overlays(frame):
    for v in tracked_vehicles:
        x1, y1, x2, y2 = v['bbox']
        
        if v['plate'] is not None:
            # Plotted license plate exists
            color = (0, 0, 255) if v['is_ambulance'] else (0, 255, 0) # Red for ambulance, Green for car
            label = f"{'AMBULANCE' if v['is_ambulance'] else 'Car'}: {v['plate']}"
            dist_label = f"Dist: {v['distance']:.2f}m"
        else:
            # Bounding box is scanning
            color = (0, 255, 255) # Yellow/Cyan
            label = "Scanning Plate..."
            dist_label = "Dist: Calculating..."
            
        cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
        cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
        cv2.putText(frame, dist_label, (x1, y1 - 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)
    return frame

# ============================================================
# HARDWARE WEBCAM INTERFACE INTERACTION LOOP
# ============================================================
print("\n🔄 Accessing live video capture interface (Webcam Index 0)...")
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("❌ Critical System Error: Unable to claim webcam interface resource channels.")
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    exit()

print("✅ Webcam interface locked and streaming.")
print("👉 Press 'q' to stop execution and terminate cleanly.")
print("="*60)

detected_plates = {}
frame_count = 0

try:
    while True:
        ret, frame = cap.read()
        if not ret:
            print("❌ Failed to grab frame from video interface pipeline.")
            break

        frame_count += 1
        
        # Run YOLO + OCR update on every 5th frame
        if frame_count % 5 == 0:
            update_tracking_and_ocr(frame, detected_plates)

        # Draw overlays on EVERY frame
        frame = draw_overlays(frame)

        cv2.imshow('V2X Smart Camera Node', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except KeyboardInterrupt:
    print("\n⏹ Manual interruption detected.")
    
finally:
    print("\n🧹 Cleaning up resources...")
    cap.release()
    cv2.destroyAllWindows()
    mqtt_client.loop_stop()
    mqtt_client.disconnect()
    
    print("\n" + "="*60)
    print("📊 FINAL SESSION REPORT")
    print("="*60)
    for plate, info in detected_plates.items():
        dist = f"{info['distance']:.2f}m" if info['distance'] != float('inf') else "Unknown"
        icon = '🚑' if plate == AMBULANCE_ID else '🚗'
        print(f" {icon} {plate} | Detected {info['count']} times | Distance: {dist}")