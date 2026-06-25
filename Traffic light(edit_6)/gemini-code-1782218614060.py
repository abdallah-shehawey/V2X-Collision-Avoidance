# -*- coding: utf-8 -*-
import cv2
from ultralytics import YOLO
import easyocr
import re
import numpy as np

# MQTT and handling libraries
import json
import time
import ssl
import paho.mqtt.client as mqtt

print("="*60)
print("🚗 REAL-TIME V2X CAMERA NODE (LOCAL MODE)")
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

# ============================================
# CAMERA & DETECTION SETTINGS
# ============================================
FOCAL_LENGTH = 700          # Camera focal length (requires calibration)
KNOWN_PLATE_WIDTH = 0.45    # Real plate width in meters (45 cm)
MIN_CAR_WIDTH = 150

# Colors (BGR)
GREEN = (0, 255, 0)
RED = (0, 0, 255)
YELLOW = (0, 255, 255)
WHITE = (255, 255, 255)
CYAN = (255, 255, 0)

# ============================================
# MODEL LOADING
# ============================================
print("\n📥 Loading AI Models (YOLO & EasyOCR)...")
car_detector = YOLO('yolov8n.pt')
reader = easyocr.Reader(['en'], gpu=False, verbose=False)
print("✅ Models loaded successfully!")

# ============================================
# HELPER FUNCTIONS
# ============================================

def calculate_distance(plate_width_px, focal_length=FOCAL_LENGTH, known_width=KNOWN_PLATE_WIDTH):
    if plate_width_px > 0:
        distance = (known_width * focal_length) / plate_width_px
        return distance
    return float('inf')

def preprocess_for_ocr(plate_img):
    gray = cv2.cvtColor(plate_img, cv2.COLOR_BGR2GRAY)
    gray = cv2.equalizeHist(gray)
    _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
    return thresh

# ============================================
# LICENSE PLATE DETECTION & OCR
# ============================================

def process_frame(frame, detected_plates):
    """Detect cars, read plates, and publish MQTT data"""
    results = car_detector(frame, verbose=False, conf=0.5)

    for box in results[0].boxes:
        if int(box.cls[0]) == 2:  # Class 2 is 'car' in COCO dataset
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            car_width = x2 - x1

            if car_width > MIN_CAR_WIDTH:
                # Estimate plate region (lower 30% of the car box)
                plate_y1 = int(y2 - (y2 - y1) * 0.3)
                plate_region = frame[plate_y1:y2, x1:x2]

                if plate_region.size > 0:
                    thresh = preprocess_for_ocr(plate_region)
                    result = reader.readtext(thresh, detail=0, paragraph=True)
                    
                    if result:
                        cleaned = re.sub(r'[^A-Z0-9]', '', result[0].upper().replace(' ', ''))

                        if len(cleaned) >= 4:
                            confidence = min(95, 60 + len(cleaned) * 5)
                            plate_width_px = x2 - x1
                            distance = calculate_distance(plate_width_px)

                            # --- Data Tracking ---
                            if cleaned not in detected_plates:
                                detected_plates[cleaned] = {'count': 0, 'confidence': confidence, 'distance': distance}
                            detected_plates[cleaned]['count'] += 1

                            distance_text = f"{distance:.2f}m" if distance != float('inf') else "Unknown"
                            is_ambulance = (cleaned == AMBULANCE_ID)
                            
                            # --- Drawing on the Frame ---
                            cv2.rectangle(frame, (x1, y1), (x2, y2), GREEN, 2)
                            cv2.rectangle(frame, (x1, plate_y1), (x2, y2), RED, 2)
                            font = cv2.FONT_HERSHEY_SIMPLEX
                            cv2.putText(frame, f"Plate: {cleaned}", (x1, y1 - 50), font, 0.6, YELLOW, 2)
                            cv2.putText(frame, f"Dist: {distance_text}", (x1, y1 - 10), font, 0.5, WHITE, 2)

                            # --- MQTT Publishing ---
                            payload = {
                                "plate_id": cleaned,
                                "distance_m": round(distance, 2) if distance != float('inf') else 999.0,
                                "is_ambulance": is_ambulance,
                                "timestamp": int(time.time())
                            }
                            mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))

    return frame

# ============================================
# LIVE CAMERA PIPELINE (LOCAL PC)
# ============================================
def start_local_camera():
    cap = cv2.VideoCapture(0) # 0 for built-in webcam, 1 for external USB camera
    
    if not cap.isOpened():
        print("❌ Error: Could not open the webcam.")
        return

    print("\n📸 Live Camera Feed Started!")
    print("👉 Press the 'q' key on your keyboard to stop the camera and exit safely.")
    print("="*60)

    detected_plates = {}
    frame_count = 0

    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("❌ Failed to grab frame. Exiting...")
                break

            frame_count += 1
            
            # Process 1 out of every 5 frames to reduce CPU load
            if frame_count % 5 == 0:
                frame = process_frame(frame, detected_plates)

            # Display the resulting frame in a new desktop window
            cv2.imshow('V2X Smart Camera Node', frame)

            # Wait for 'q' key to quit
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

# Run the program
if __name__ == "__main__":
    start_local_camera()