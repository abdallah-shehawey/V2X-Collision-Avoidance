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
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"

AMBULANCE_ID = "T4RR"

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


def process_frame(frame, registry):
    """
    Executes frame-by-frame deep learning classification logic.
    Identifies bounds, handles character extractions, updates tracking registries,
    and returns annotated graphical bounding maps.
    """
    results = model(frame, verbose=False)
    
    for r in results:
        boxes = r.boxes
        for box in boxes:
            cls_id = int(box.cls[0])
            if cls_id in [2, 5, 7]: # Select structural vehicle objects
                xyxy = box.xyxy[0].cpu().numpy()
                x1, y1, x2, y2 = map(int, xyxy)

                h = y2 - y1
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

                                # Track persistence logs and record baseline values
                                if cleaned not in registry:
                                    registry[cleaned] = {'count': 1, 'distance': calc_distance}
                                else:
                                    registry[cleaned]['count'] += 1
                                    registry[cleaned]['distance'] = calc_distance

                                # Package synchronized json mapping definitions
                                payload = {
                                    "plate_id": cleaned,
                                    "distance_m": round(float(calc_distance), 2),
                                    "is_ambulance": bool(is_ambulance)
                                }
                                
                                try:
                                    mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))
                                except Exception as network_error:
                                    print(f"⚠️ Transmission drop: {network_error}")

                                # Paint tracking visualization boundaries
                                color = (0, 0, 255) if is_ambulance else (0, 255, 0)
                                cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
                                label = f"{'AMBULANCE' if is_ambulance else 'Car'}: {cleaned}"
                                cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)
                                cv2.putText(frame, f"Dist: {calc_distance:.2f}m", (x1, y1 - 30), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)
                                break
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
        
        # Performance optimization: run heavy object processing loops on every 5th frame
        if frame_count % 5 == 0:
            frame = process_frame(frame, detected_plates)

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