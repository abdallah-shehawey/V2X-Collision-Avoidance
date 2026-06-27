#!/usr/bin/env python3
"""
V2P System - Traffic Light Camera with Distance Estimation
Compatible with BOTH Windows (Webcam) and Raspberry Pi (PiCamera2)
Direct MQTT Server Integration (No IPC Needed)
"""

import cv2
import numpy as np
import time
import os
import sys
import json
import ssl
from datetime import datetime
import paho.mqtt.client as mqtt

# التحقق تلقائياً من نظام التشغيل والمكتبة المتاحة
try:
    from picamera2 import Picamera2
    IS_RASPBERRY_PI = True
    print("🐧 System detected: Raspberry Pi Mode")
except ImportError:
    IS_RASPBERRY_PI = False
    print("🪟 System detected: Windows / PC Mode (Webcam will be used)")

# ============================================================
# SERVER CONFIGURATION (HiveMQ Cloud)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# التوبيك المباشر اللي الكاميرا هتبعت عليه للسيرفر
CAMERA_TOPIC = "v2n/camera/detection"

# ============================================================
# SYSTEM CONFIGURATION - Camera & Distance Parameters
# ============================================================
CONFIG = {
    "camera_width": 640,
    "camera_height": 480,
    "camera_fps": 30,
    "car_actual_width": 1.8,      
    "person_actual_height": 1.7,  
    "focal_length_px": 650,       
    "detection_confidence": 0.4,
    "skip_frames": 3,              
    "danger_zone": 10,
    "warning_zone": 20,
    "approach_zone": 40,
    "save_log": False,              
    "verbose": True                
}

# ============================================================
# DISTANCE CALCULATOR
# ============================================================
class DistanceCalculator:
    def __init__(self, config):
        self.config = config
        
    def estimate_distance(self, bbox, obj_type):
        x1, y1, x2, y2 = bbox
        w_px = x2 - x1
        h_px = y2 - y1
        
        if obj_type in ['car', 'truck', 'bus', 'ambulance']:
            actual_w = self.config["car_actual_width"]
            focal = self.config["focal_length_px"]
            distance = (actual_w * focal) / max(1, w_px)
            return distance
        elif obj_type == 'person':
            actual_h = self.config["person_actual_height"]
            focal = self.config["focal_length_px"]
            distance = (actual_h * focal) / max(1, h_px)
            return distance
        return 999.0

# ============================================================
# OBJECT TRACKER (Placeholder for AI Detections)
# ============================================================
class ObjectTracker:
    def __init__(self):
        self.id_counter = 0
        
    def track(self, detections):
        tracked_objects = {}
        for idx, det in enumerate(detections):
            x1, y1, x2, y2, obj_type, distance = det
            tracked_objects[idx] = {
                'id': idx,
                'type': obj_type,
                'distance': distance,
                'bbox': (x1, y1, x2, y2)
            }
        self.id_counter = max(self.id_counter, len(detections))
        return tracked_objects

# ============================================================
# MAIN SYSTEM - WITH DYNAMIC CAMERA & DIRECT MQTT
# ============================================================
class V2PSystem:
    def __init__(self, config):
        self.config = config
        self.distance_calc = DistanceCalculator(config)
        self.tracker = ObjectTracker()
        self.camera = None
        
        # 1. تهيئة وتوصيل الكاميرا بناءً على النظام الشغال
        if IS_RASPBERRY_PI:
            try:
                self.camera = Picamera2()
                self.camera.start()
                print("✅ PiCamera2 successfully started.")
            except Exception as e:
                print(f"❌ Failed to start PiCamera2: {e}")
                sys.exit(1)
        else:
            self.camera = cv2.VideoCapture(0)
            if not self.camera.isOpened():
                print("❌ Error: Could not open Windows Webcam.")
                sys.exit(1)
            print("✅ Windows Webcam successfully opened.")

        # 2. تهيئة وتوصيل الـ MQTT Client مباشرة بالسيرفر
        print("🌐 Connecting to HiveMQ Server...")
        self.mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.mqtt_client.username_pw_set(USERNAME, PASSWORD)
        self.mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
        
        try:
            self.mqtt_client.connect(BROKER, PORT, 60)
            self.mqtt_client.loop_start()  
            print("✅ Successfully Connected DIRECTLY to HiveMQ Server!")
        except Exception as e:
            print(f"❌ Connection to Server Failed: {e}")

    def run(self):
        print("🚀 Camera Tracking & Distance Estimation System Started...")
        frame_count = 0
        
        try:
            while True:
                frame_count += 1
                
                # 3. لقط الفريم حسب النظام الحالي
                if IS_RASPBERRY_PI:
                    frame = self.camera.capture_array()
                else:
                    ret, frame = self.camera.read()
                    if not ret:
                        time.sleep(0.01)
                        continue
                
                # المعالجة بتحصل كل عدد فريمات معين
                if frame_count % self.config["skip_frames"] == 0:
                    
                    # داتا تجريبية لمحاكاة لقط إسعاف
                    mock_detections = [(100, 150, 250, 300, 'ambulance')] 
                    
                    dist_detections = []
                    for x1, y1, x2, y2, obj_type in mock_detections:
                        distance = self.distance_calc.estimate_distance((x1, y1, x2, y2), obj_type)
                        dist_detections.append((x1, y1, x2, y2, obj_type, distance))
                    
                    tracked = self.tracker.track(dist_detections)
                    
                    # الإرسال المباشر للسيرفر
                    if tracked:
                        closest_object = min(tracked.values(), key=lambda x: x['distance'])
                        real_distance = float(closest_object['distance'])
                        obj_type = closest_object['type']
                        
                        payload = {
                            "camera_id": "CAM-INT-001",
                            "type": obj_type,
                            "distance": round(real_distance, 2),
                            "timestamp": int(time.time() * 1000)
                        }
                        
                        self.mqtt_client.publish(CAMERA_TOPIC, json.dumps(payload))
                        print(f"📡 [Server Alert] Published to '{CAMERA_TOPIC}' -> {obj_type} at {payload['distance']}m")
                
                # 4. عرض الفيديو لو شغالين على الويندوز
                if not IS_RASPBERRY_PI:
                    cv2.rectangle(frame, (100, 150), (250, 300), (0, 0, 255), 2)
                    cv2.putText(frame, "SIMULATED AMBULANCE: 45m", (100, 135),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 2)
                    
                    cv2.imshow("V2X Camera Simulator (Press 'q' to quit)", frame)
                    
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
                else:
                    time.sleep(0.03)
                
        except KeyboardInterrupt:
            print("\n🛑 System stopped by user")
        finally:
            # تنظيف وإغلاق الاتصالات بأمان
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
            
            if IS_RASPBERRY_PI:
                if self.camera: self.camera.stop()
            else:
                if self.camera: self.camera.release()
            cv2.destroyAllWindows()
            print("🔌 Disconnected and resources released successfully.")

# ============================================================
# ENTRY POINT
# ============================================================
def main():
    system = V2PSystem(CONFIG)
    system.run()

if __name__ == "__main__":
    main()