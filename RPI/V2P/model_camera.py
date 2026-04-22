import cv2
import numpy as np
import time
from picamera2 import Picamera2
import onnxruntime as ort

# ============================================
# 
# # ============================================
class LightTracker:
    def __init__(self):
        self.tracked = {}
        self.next_id = 0
        
    def update(self, pedestrians):
        current_time = time.time()
        new_tracked = {}
        
        for box in pedestrians:
            cx, cy = (box[0]+box[2])//2, (box[1]+box[3])//2
            match_id = None
            for pid, data in self.tracked.items():
                dist = np.hypot(cx - data['c'][0], cy - data['c'][1])
                if dist < 50: 
                    match_id = pid
                    break
            
            if match_id is not None:
                new_tracked[match_id] = {'b': box, 'c': (cx, cy), 't': current_time}
            else:
                new_tracked[self.next_id] = {'b': box, 'c': (cx, cy), 't': current_time}
                self.next_id += 1
        
        self.tracked = {k: v for k, v in new_tracked.items() if current_time - v['t'] < 1.0}
        return self.tracked

# ============================================
# MAIN SYSTEM - ULTRA LIGHT VERSION
# ============================================
print("Starting Ultra-Light V2P System...")

# 1. تحميل ONNX بأقل إعدادات
opts = ort.SessionOptions()
opts.intra_op_num_threads = 2  # تحديد عدد الأنوية لتقليل الضغط
session = ort.InferenceSession('model.onnx', sess_options=opts, providers=['CPUExecutionProvider'])
input_name = session.get_inputs()[0].name
output_names = [session.get_outputs()[0].name]

picam2 = Picamera2()
config = picam2.create_preview_configuration(main={"size": (640, 480), "format": "RGB888"})
picam2.configure(config)
picam2.start()

tracker = LightTracker()
SKIP_FRAMES = 4  # 
frame_count = 0

try:
    while True:
        frame_rgb = picam2.capture_array()
        frame_count += 1
        
        
        if frame_count % SKIP_FRAMES == 0:
            
            input_img = cv2.resize(frame_rgb, (416,416))
            input_img = input_img.astype(np.float32) / 255.0
            input_img = input_img.transpose(2, 0, 1)[np.newaxis, ...]

            outputs = session.run(output_names, {input_name: input_img})
            preds = np.squeeze(outputs[0]).T
            
            pedestrians = []
            for p in preds:
                if p[4] > 0.5: 
                    cx, cy, w, h = p[0:4]
                    x1, y1 = int((cx-w/2)*(640/416)), int((cy-h/2)*(480/416))
                    x2, y2 = int((cx+w/2)*(640/416)), int((cy+h/2)*(480/416))
                    pedestrians.append((x1, y1, x2, y2))
            
            tracked_objects = tracker.update(pedestrians)
            
            frame_bgr = cv2.cvtColor(frame_rgb, cv2.COLOR_RGB2BGR)
            for pid, data in tracked_objects.items():
                x1, y1, x2, y2 = data['b']
                cv2.rectangle(frame_bgr, (x1, y1), (x2, y2), (0, 255, 0), 2)
            
            cv2.imshow('Fast V2P', frame_bgr)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

except Exception as e:
    print(f"Error: {e}")
finally:
    picam2.stop()
    cv2.destroyAllWindows()