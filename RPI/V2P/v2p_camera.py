import cv2
import numpy as np
import time
import math
from collections import deque
from picamera2 import Picamera2
from ultralytics import YOLO

# ============================================
# Pedestrian Tracker Class
# ============================================
class PedestrianTracker:
    def __init__(self, max_history=15, prediction_horizon=8):
        self.tracked_pedestrians = {}
        self.max_history = max_history
        self.prediction_horizon = prediction_horizon
        self.next_id = 0
        
    def update(self, pedestrians, frame_width, frame_height, timestamp=None):
        if timestamp is None:
            timestamp = time.time()
            
        new_centers = []
        for (x1, y1, x2, y2) in pedestrians:
            center_x = (x1 + x2) // 2
            center_y = (y1 + y2) // 2
            new_centers.append({
                'bbox': (x1, y1, x2, y2),
                'center': (center_x, center_y),
                'width': x2 - x1,
                'height': y2 - y1,
                'timestamp': timestamp
            })
        
        matched_ids = set()
        for new in new_centers:
            best_match = None
            best_dist = 100
            
            for pid, data in self.tracked_pedestrians.items():
                if pid in matched_ids:
                    continue
                if 'last_center' not in data:
                    continue
                    
                last_center = data['last_center']
                dist = math.sqrt((last_center[0] - new['center'][0])**2 + 
                                 (last_center[1] - new['center'][1])**2)
                
                if dist < best_dist:
                    best_dist = dist
                    best_match = pid
            
            if best_match is not None:
                pid = best_match
                matched_ids.add(pid)
                
                self.tracked_pedestrians[pid]['positions'].append({
                    'center': new['center'],
                    'timestamp': new['timestamp'],
                    'bbox': new['bbox']
                })
                self.tracked_pedestrians[pid]['last_center'] = new['center']
                self.tracked_pedestrians[pid]['last_bbox'] = new['bbox']
                self.tracked_pedestrians[pid]['last_seen'] = timestamp
                
                if len(self.tracked_pedestrians[pid]['positions']) > self.max_history:
                    self.tracked_pedestrians[pid]['positions'].pop(0)
                    
            else:
                self.tracked_pedestrians[self.next_id] = {
                    'positions': deque([{
                        'center': new['center'],
                        'timestamp': new['timestamp'],
                        'bbox': new['bbox']
                    }], maxlen=self.max_history),
                    'last_center': new['center'],
                    'last_bbox': new['bbox'],
                    'last_seen': timestamp,
                    'intention': 'walking',
                    'predicted_path': [],
                    'predicted_target': None,
                    'velocity': (0, 0),
                    'acceleration': (0, 0)
                }
                self.next_id += 1
        
        current_time = timestamp
        to_delete = []
        for pid, data in self.tracked_pedestrians.items():
            if current_time - data['last_seen'] > 1.5:
                to_delete.append(pid)
                
        for pid in to_delete:
            del self.tracked_pedestrians[pid]
        
        results = []
        for pid, data in self.tracked_pedestrians.items():
            positions = list(data['positions'])
            
            if len(positions) >= 2:
                self._calculate_motion(data, positions)
                self._predict_movement(data, positions, frame_width, frame_height)
                self._determine_intention(data, positions, frame_width, frame_height)
            
            results.append({
                'id': pid,
                'bbox': data['last_bbox'],
                'center': data['last_center'],
                'intention': data['intention'],
                'predicted_path': data.get('predicted_path', []),
                'predicted_target': data.get('predicted_target', None),
                'velocity': data.get('velocity', (0, 0))
            })
        
        return results
    
    def _calculate_motion(self, data, positions):
        last = positions[-1]['center']
        prev = positions[-2]['center']
        dt = positions[-1]['timestamp'] - positions[-2]['timestamp']
        if dt <= 0:
            dt = 0.1
        
        vx = (last[0] - prev[0]) / dt
        vy = (last[1] - prev[1]) / dt
        data['velocity'] = (vx, vy)
        
        if len(positions) >= 3:
            prev2 = positions[-3]['center']
            dt2 = positions[-2]['timestamp'] - positions[-3]['timestamp']
            if dt2 > 0:
                vx_prev = (prev[0] - prev2[0]) / dt2
                vy_prev = (prev[1] - prev2[1]) / dt2
                ax = (vx - vx_prev) / dt
                ay = (vy - vy_prev) / dt
                data['acceleration'] = (ax, ay)
    
    def _predict_movement(self, data, positions, w, h):
        vx, vy = data['velocity']
        ax, ay = data.get('acceleration', (0, 0))
        last = positions[-1]['center']
        
        path = []
        for t in range(1, self.prediction_horizon + 1):
            dt = t * 0.2
            px = last[0] + (vx * dt) + (0.5 * ax * dt * dt)
            py = last[1] + (vy * dt) + (0.5 * ay * dt * dt)
            px = max(0, min(w - 1, px))
            py = max(0, min(h - 1, py))
            path.append((int(px), int(py)))
        
        data['predicted_path'] = path
        self._predict_target(data, path, w, h)
    
    def _predict_target(self, data, path, w, h):
        if not path:
            data['predicted_target'] = 'unknown'
            return
        
        fx, fy = path[-1]
        left = w * 0.2
        right = w * 0.8
        zone = (w * 0.35, w * 0.65)
        
        if fx < left:
            target = 'sidewalk_left'
        elif fx > right:
            target = 'sidewalk_right'
        elif zone[0] < fx < zone[1]:
            target = 'road_crossing'
        elif fx < w // 2:
            target = 'approaching_road_left'
        else:
            target = 'approaching_road_right'
        
        positions = list(data['positions'])
        if len(positions) >= 2:
            prev_x = positions[-2]['center'][0]
            curr_x = positions[-1]['center'][0]
            if prev_x < zone[0] and curr_x > zone[0]:
                target = 'entering_road'
            elif prev_x > zone[1] and curr_x < zone[1]:
                target = 'entering_road'
        
        data['predicted_target'] = target
    
    def _determine_intention(self, data, positions, w, h):
        vx, vy = data['velocity']
        cx, cy = data['last_center']
        target = data.get('predicted_target', 'unknown')
        
        score = 0
        danger = 0
        
        if abs(vx) > 30:
            score += 30
        elif abs(vx) > 15:
            score += 15
        
        if cx > w * 0.3 and cx < w * 0.7:
            score += 25
        
        if len(positions) >= 3:
            start_x = positions[0]['center'][0]
            end_x = positions[-1]['center'][0]
            if abs(end_x - start_x) > 50:
                score += 20
        
        if target == 'road_crossing':
            score += 40
            danger += 30
        elif target == 'entering_road':
            score += 50
            danger += 40
        elif target in ['approaching_road_left', 'approaching_road_right']:
            score += 20
            danger += 15
        
        if abs(vy) < 10:
            score += 10
        
        if score >= 60:
            data['intention'] = 'crossing_immediate'
        elif score >= 40:
            data['intention'] = 'crossing_likely'
        elif score >= 20:
            data['intention'] = 'approaching'
        else:
            data['intention'] = 'walking'
        
        if danger >= 50:
            data['intention'] = 'danger_' + data['intention']
        
        if target in ['sidewalk_left', 'sidewalk_right']:
            data['intention'] = 'walking_safe'


# ============================================
# MAIN SYSTEM
# ============================================
print("=" * 60)
print("V2P SYSTEM - PEDESTRIAN TRACKING & PREDICTION")
print("=" * 60)

print("\nLoading YOLO model...")
# ===== التعديل المهم =====
detector = YOLO('model.pt')  # الموديل بتاعك
# ========================
print("Model loaded!")

print("\nStarting camera...")
picam2 = Picamera2()
config = picam2.create_preview_configuration(
    main={"size": (640, 480), "format": "RGB888"},
    controls={"FrameRate": 30}
)
picam2.configure(config)
picam2.start()
time.sleep(2)
print("Camera ready!")

tracker = PedestrianTracker(max_history=15, prediction_horizon=8)

fps_counter = 0
fps_time = time.time()
frame_count = 0
SKIP_FRAMES = 2

print("\nLive detection started... Press 'q' to exit")
print("-" * 60)

try:
    while True:
        frame = picam2.capture_array()
        frame_count += 1
        
        if frame_count % SKIP_FRAMES == 0:
            results = detector(frame)
            
            pedestrians = []
            for box in results[0].boxes:
                class_id = int(box.cls[0])
                # class 1 = person في موديلك
                if class_id == 1:
                    x1, y1, x2, y2 = map(int, box.xyxy[0])
                    pedestrians.append((x1, y1, x2, y2))
            
            tracked = tracker.update(pedestrians, frame.shape[1], frame.shape[0])
            
            annotated = results[0].plot()
            
            for ped in tracked:
                x1, y1, x2, y2 = ped['bbox']
                intention = ped['intention']
                target = ped.get('predicted_target', 'unknown')
                path = ped.get('predicted_path', [])
                vx, vy = ped.get('velocity', (0, 0))
                speed = math.sqrt(vx**2 + vy**2)
                
                # لون حسب الخطر
                if intention == 'crossing_immediate':
                    color = (0, 0, 255)      # أحمر
                    thick = 3
                elif intention == 'crossing_likely':
                    color = (0, 100, 255)    # برتقالي
                    thick = 2
                elif intention == 'approaching':
                    color = (0, 165, 255)    # برتقالي فاتح
                    thick = 2
                elif 'danger' in intention:
                    color = (0, 0, 200)      # أحمر غامق
                    thick = 3
                else:
                    color = (0, 255, 0)      # أخضر
                    thick = 2
                
                cv2.rectangle(annotated, (x1, y1), (x2, y2), color, thick)
                
                # كتابة النية
                cv2.putText(annotated, intention.replace('_', ' ').title(), 
                           (x1, y1 - 35), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
                cv2.putText(annotated, f"Target: {target.replace('_', ' ')}", 
                           (x1, y1 - 20), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200, 200, 200), 1)
                cv2.putText(annotated, f"Speed: {speed:.1f}", 
                           (x1, y1 - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (200, 200, 200), 1)
                
                # رسم المسار المتوقع
                if len(path) > 1:
                    for i in range(len(path) - 1):
                        cv2.line(annotated, path[i], path[i + 1], color, 2)
                    cv2.circle(annotated, path[-1], 5, color, -1)
            
            # FPS
            fps_counter += 1
            if time.time() - fps_time >= 1:
                fps = fps_counter
                fps_counter = 0
                fps_time = time.time()
            
            cv2.putText(annotated, f"FPS: {fps}", (10, 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            cv2.putText(annotated, f"Pedestrians: {len(tracked)}", (10, 60),
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            
            cv2.imshow('V2P System', annotated)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

except KeyboardInterrupt:
    print("\nStopped by user")
finally:
    picam2.stop()
    cv2.destroyAllWindows()

print("\n" + "=" * 60)
print("FINAL RESULTS")
print("=" * 60)
print(f"Total frames: {frame_count}")
print("System stopped successfully!")