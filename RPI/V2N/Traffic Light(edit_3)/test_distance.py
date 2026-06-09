#!/usr/bin/env python3
"""
V2P System - Traffic Light Camera with Distance Estimation
For Raspberry Pi with Camera Module
GitHub Ready - Production Version
"""

import cv2
import numpy as np
import time
import os
import sys
import json
from datetime import datetime
from picamera2 import Picamera2

# ============================================================
# CONFIGURATION - Edit these values for your setup
# ============================================================

CONFIG = {
    # Camera settings
    "camera_width": 640,
    "camera_height": 480,
    "camera_fps": 30,
    
    # Distance calculation parameters
    "car_actual_width": 1.8,      # Average car width in meters
    "person_actual_height": 1.7,  # Average person height in meters
    "focal_length_px": 650,       # Camera focal length (calibrate this!)
    
    # Detection thresholds
    "detection_confidence": 0.4,
    "skip_frames": 3,              # Process every 3rd frame
    
    # Alert zones (meters)
    "danger_zone": 10,
    "warning_zone": 20,
    "approach_zone": 40,
    
    # Output options
    "show_lcd": False,             # Set True if LCD connected
    "save_log": True,              # Save detections to log file
    "verbose": True                # Print to terminal
}

# ============================================================
# DISTANCE CALCULATOR
# ============================================================

class DistanceCalculator:
    def __init__(self, config):
        self.config = config
        self.car_width_real = config["car_actual_width"]
        self.person_height_real = config["person_actual_height"]
        self.focal_length = config["focal_length_px"]
    
    def car_distance(self, width_px):
        """Calculate distance to car based on width in pixels"""
        if width_px <= 0:
            return 999
        return (self.car_width_real * self.focal_length) / width_px
    
    def person_distance(self, height_px):
        """Calculate distance to person based on height in pixels"""
        if height_px <= 0:
            return 999
        return (self.person_height_real * self.focal_length) / height_px
    
    def get_zone(self, distance):
        """Get alert zone based on distance"""
        if distance < self.config["danger_zone"]:
            return "DANGER", "🔴"
        elif distance < self.config["warning_zone"]:
            return "WARNING", "🟡"
        elif distance < self.config["approach_zone"]:
            return "APPROACHING", "🟢"
        else:
            return "FAR", "⚪"
    
    def get_time_to_arrival(self, distance, speed_ms=13.8):
        """Estimate seconds until car reaches camera (speed ~50km/h = 13.8m/s)"""
        if speed_ms <= 0:
            return 999
        return distance / speed_ms

# ============================================================
# OBJECT TRACKER
# ============================================================

class ObjectTracker:
    def __init__(self):
        self.objects = {}
        self.id_counter = 0
    
    def track(self, detections):
        """
        Track objects across frames
        detections: list of (x1, y1, x2, y2, obj_type, distance)
        """
        now = time.time()
        new_tracked = {}
        
        for det in detections:
            x1, y1, x2, y2, obj_type, distance = det
            cx = (x1 + x2) // 2
            
            # Find matching object from previous frame
            match_id = None
            for oid, data in self.objects.items():
                if abs(cx - data['cx']) < 80:  # Max movement pixel threshold
                    match_id = oid
                    break
            
            if match_id is not None:
                # Update existing object
                old_cx = self.objects[match_id]['cx']
                speed = (cx - old_cx) * 0.3  # Approximate speed m/s
                new_tracked[match_id] = {
                    'box': (x1, y1, x2, y2),
                    'cx': cx,
                    'distance': distance,
                    'type': obj_type,
                    'speed': round(speed, 1),
                    't': now
                }
            else:
                # New object
                new_tracked[self.id_counter] = {
                    'box': (x1, y1, x2, y2),
                    'cx': cx,
                    'distance': distance,
                    'type': obj_type,
                    'speed': 0,
                    't': now
                }
                self.id_counter += 1
        
        # Remove old objects (not seen for >1 second)
        self.objects = {k: v for k, v in new_tracked.items() if now - v['t'] < 1.0}
        return self.objects
    
    def get_stats(self):
        """Get tracking statistics"""
        cars = [o for o in self.objects.values() if o['type'] == 'car']
        persons = [o for o in self.objects.values() if o['type'] == 'person']
        return {
            'total': len(self.objects),
            'cars': len(cars),
            'persons': len(persons),
            'danger_cars': [o for o in cars if o['distance'] < 10]
        }

# ============================================================
# YOLO MODEL LOADER (for Raspberry Pi)
# ============================================================

class YOLODetector:
    def __init__(self, model_path='yolov8n.onnx'):
        self.model_path = model_path
        self.session = None
        self.input_name = None
        
    def load(self):
        """Load ONNX model"""
        try:
            import onnxruntime as ort
            print(f"Loading model from {self.model_path}...")
            opts = ort.SessionOptions()
            opts.intra_op_num_threads = 2
            self.session = ort.InferenceSession(self.model_path, sess_options=opts, providers=['CPUExecutionProvider'])
            self.input_name = self.session.get_inputs()[0].name
            print("✅ Model loaded successfully")
            return True
        except Exception as e:
            print(f"❌ Failed to load model: {e}")
            return False
    
    def detect(self, frame, confidence_threshold=0.4):
        """Run detection on frame"""
        if self.session is None:
            return []
        
        # Prepare input
        h, w = frame.shape[:2]
        blob = cv2.resize(frame, (416, 416))
        blob = blob.astype(np.float32) / 255.0
        blob = blob.transpose(2, 0, 1)[np.newaxis, ...]
        
        # Run inference
        outputs = self.session.run(None, {self.input_name: blob})
        preds = np.squeeze(outputs[0]).T
        
        # Parse detections
        detections = []
        scale_x = w / 416
        scale_y = h / 416
        
        for p in preds:
            if len(p) >= 5 and p[4] > confidence_threshold:
                cx, cy, box_w, box_h = p[0:4]
                class_id = int(p[5]) if len(p) > 5 else -1
                
                x1 = int((cx - box_w/2) * scale_x)
                y1 = int((cy - box_h/2) * scale_y)
                x2 = int((cx + box_w/2) * scale_x)
                y2 = int((cy + box_h/2) * scale_y)
                
                x1, y1 = max(0, x1), max(0, y1)
                x2, y2 = min(w, x2), min(h, y2)
                
                # Class IDs: 0=person, 2=car (COCO dataset)
                if class_id == 0 and (x2-x1) > 20 and (y2-y1) > 40:
                    detections.append((x1, y1, x2, y2, 'person', p[4]))
                elif class_id == 2 and (x2-x1) > 40 and (y2-y1) > 30:
                    detections.append((x1, y1, x2, y2, 'car', p[4]))
        
        return detections

# ============================================================
# LOGGER
# ============================================================

class Logger:
    def __init__(self, log_file='v2p_log.json'):
        self.log_file = log_file
        self.logs = []
    
    def log(self, data):
        """Save detection data to log"""
        data['timestamp'] = datetime.now().isoformat()
        self.logs.append(data)
        
        if len(self.logs) > 100:
            self.save()
    
    def save(self):
        """Write logs to file"""
        try:
            with open(self.log_file, 'w') as f:
                json.dump(self.logs[-100:], f, indent=2)
        except:
            pass

# ============================================================
# DISPLAY (Terminal)
# ============================================================

class Display:
    def __init__(self, config):
        self.config = config
        self.last_update = 0
    
    def show(self, stats, tracked_objects):
        """Display results in terminal"""
        if not self.config["verbose"]:
            return
        
        # Update every 0.5 seconds
        if time.time() - self.last_update < 0.5:
            return
        self.last_update = time.time()
        
        # Clear screen
        os.system('clear')
        
        print("="*70)
        print("🚦 V2P SYSTEM - TRAFFIC LIGHT CAMERA")
        print("="*70)
        print(f"📊 Stats: Cars: {stats['cars']} | Persons: {stats['persons']} | Total: {stats['total']}")
        print("-"*70)
        
        if tracked_objects:
            print("\n📌 DETECTED OBJECTS:")
            for oid, data in list(tracked_objects.items())[:10]:
                obj_type = data['type'].upper()
                distance = data['distance']
                zone, emoji = DistanceCalculator(CONFIG).get_zone(distance)
                
                if obj_type == 'CAR':
                    print(f"  {emoji} CAR #{oid:2d} | Distance: {distance:5.1f}m | {zone}")
                else:
                    print(f"  {emoji} PERSON #{oid:2d} | Distance: {distance:5.1f}m | {zone}")
            
            # Show Danger alerts
            if stats['danger_cars']:
                print(f"\n🚨 ALERT: {len(stats['danger_cars'])} car(s) in DANGER ZONE (<10m)!")
        else:
            print("\n✅ No objects detected")
        
        print("\n" + "="*70)
        print("Press Ctrl+C to stop")
    
    def show_alert(self, alert):
        """Display alert message"""
        if not self.config["verbose"]:
            return
        print(f"\n🔔 {alert}")

# ============================================================
# MAIN SYSTEM
# ============================================================

class V2PSystem:
    def __init__(self, config):
        self.config = config
        self.distance_calc = DistanceCalculator(config)
        self.tracker = ObjectTracker()
        self.display = Display(config)
        self.logger = Logger()
        self.detector = None
        self.camera = None
        
    def init_camera(self):
        """Initialize PiCamera"""
        try:
            self.camera = Picamera2()
            camera_config = self.camera.create_preview_configuration(
                main={"format": "RGB888", "size": (self.config["camera_width"], self.config["camera_height"])},
                controls={"FrameRate": self.config["camera_fps"]}
            )
            self.camera.configure(camera_config)
            self.camera.start()
            time.sleep(1)
            print(f"✅ Camera initialized: {self.config['camera_width']}x{self.config['camera_height']}")
            return True
        except Exception as e:
            print(f"❌ Camera error: {e}")
            return False
    
    def init_model(self, model_path='yolov8n.onnx'):
        """Initialize YOLO model"""
        # Try multiple possible paths
        possible_paths = [
            model_path,
            '/home/pi/models/yolov8n.onnx',
            './yolov8n.onnx'
        ]
        
        for path in possible_paths:
            self.detector = YOLODetector(path)
            if self.detector.load():
                return True
        
        print("❌ Could not load model. Falling back to simple detection...")
        return False
    
    def simple_detection(self, frame):
        """Fallback detection (no ML model)"""
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blur = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blur, 50, 150)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        detections = []
        for cnt in contours:
            x, y, w, h = cv2.boundingRect(cnt)
            if 40 < w < 250 and 40 < h < 200:
                # Estimate based on size
                distance = self.distance_calc.car_distance(w)
                obj_type = 'car' if w > h else 'person'
                detections.append((x, y, x+w, y+h, obj_type, distance))
        
        return detections
    
    def run(self):
        """Main loop"""
        print("\n" + "="*70)
        print("🚀 STARTING V2P SYSTEM")
        print("="*70)
        
        # Initialize camera
        if not self.init_camera():
            print("Failed to initialize camera")
            return
        
        # Initialize model (optional)
        model_loaded = self.init_model()
        
        frame_count = 0
        
        print("\n✅ System ready!\n")
        print("Press Ctrl+C to stop\n")
        
        try:
            while True:
                # Capture frame
                frame = self.camera.capture_array()
                frame_count += 1
                
                # Process every Nth frame
                if frame_count % self.config["skip_frames"] == 0:
                    # Detect objects
                    if model_loaded and self.detector:
                        detections = self.detector.detect(frame, self.config["detection_confidence"])
                    else:
                        detections = self.simple_detection(frame)
                    
                    # Calculate distances
                    dist_detections = []
                    for det in detections:
                        x1, y1, x2, y2, obj_type, conf = det
                        if obj_type == 'car':
                            width_px = x2 - x1
                            distance = self.distance_calc.car_distance(width_px)
                        else:
                            height_px = y2 - y1
                            distance = self.distance_calc.person_distance(height_px)
                        
                        if distance < 100:  # Filter unrealistic distances
                            dist_detections.append((x1, y1, x2, y2, obj_type, distance))
                    
                    # Track objects
                    tracked = self.tracker.track(dist_detections)
                    
                    # Get stats
                    stats = self.tracker.get_stats()
                    
                    # Display results
                    self.display.show(stats, tracked)
                    
                    # Log data
                    if self.config["save_log"]:
                        self.logger.log({
                            'frame': frame_count,
                            'stats': stats,
                            'objects': tracked
                        })
                    
                    # Generate alerts
                    for car in stats['danger_cars']:
                        self.display.show_alert(f"Car at {car['distance']:.1f}m is too close!")
                
                time.sleep(0.01)
                
        except KeyboardInterrupt:
            print("\n\n🛑 System stopped by user")
        finally:
            # Cleanup
            if self.camera:
                self.camera.stop()
            cv2.destroyAllWindows()
            self.logger.save()
            print(f"\n📊 Total objects tracked: {self.tracker.id_counter}")
            print(f"📁 Log saved to: {self.logger.log_file}")

# ============================================================
# ENTRY POINT
# ============================================================

def main():
    """Main entry point"""
    system = V2PSystem(CONFIG)
    system.run()

if __name__ == "__main__":
    main()