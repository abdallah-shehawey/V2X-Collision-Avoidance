# -*- coding: utf-8 -*-
"""
distance_and_id_mqtt.py
كود الكاميرا مع MQTT Publisher
يبعت ID العربية والمسافة للـ Intelligent Gateway
"""

import cv2
import easyocr
import numpy as np
from ultralytics import YOLO
import pandas as pd
import paho.mqtt.client as mqtt
import ssl
import json
import time

# ============================================================
# MQTT CONFIGURATION (نفس إعدادات الـ Gateway)
# ============================================================
BROKER   = "2b6738facfbf40f1a86ba770618ae8a6.s1.eu.hivemq.cloud"
PORT     = 8883
USERNAME = "v2n_admin"
PASSWORD = "V2n@2026!"

# Topic جديد خاص بالكاميرا - الـ Gateway هيستمع عليه
CAMERA_DETECTION_TOPIC = "v2n/camera/vehicle_data"

# ============================================================
# MQTT CLIENT SETUP
# ============================================================
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqtt_client.username_pw_set(USERNAME, PASSWORD)
mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("✅ Camera Publisher connected to HiveMQ Cloud!")
    else:
        print(f"❌ MQTT Connection failed: {reason_code}")

mqtt_client.on_connect = on_connect
print("🌐 Connecting Camera Publisher to HiveMQ Cloud...")
mqtt_client.connect(BROKER, PORT, 60)
mqtt_client.loop_start()   # background thread للـ MQTT

time.sleep(2)  # ننتظر الاتصال يتأكد

# ============================================================
# VIDEO INPUT
# ============================================================
video_path = "ddddd.mp4"   # ← غير الاسم ده لمسار الفيديو الحقيقي

print("="*60)
print("🚦 Camera Detection + Distance + MQTT Publisher")
print("="*60)

# ============================================================
# LOAD MODELS
# ============================================================
detector = YOLO('yolov8n.pt')
reader   = easyocr.Reader(['en'], gpu=False, verbose=False)

# ============================================================
# DISTANCE CALCULATOR
# ============================================================
class DistanceCalculator:
    """
    حساب المسافة بالمتر بناءً على عرض العربية في الصورة
    المعادلة: distance = (real_width * focal_length) / pixel_width
    """
    def __init__(self):
        self.car_actual_width = 1.8   # متوسط عرض السيارة بالمتر
        self.focal_length_px  = 650   # focal length تقريبي بالبيكسل

    def calculate(self, width_px):
        if width_px <= 5:
            return None
        return (self.car_actual_width * self.focal_length_px) / width_px

distance_calc = DistanceCalculator()

# ============================================================
# PLATE ENHANCEMENT
# ============================================================
def enhance_plate(img):
    """تحسين صورة اللوحة قبل القراءة بـ OCR"""
    h, w = img.shape[:2]
    img   = cv2.resize(img, (w*3, h*3), interpolation=cv2.INTER_CUBIC)
    gray  = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    gray  = cv2.equalizeHist(gray)
    gray  = cv2.bilateralFilter(gray, 9, 75, 75)
    thresh = cv2.adaptiveThreshold(
        gray, 255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
        cv2.THRESH_BINARY, 11, 2
    )
    return thresh

# ============================================================
# MQTT PUBLISH FUNCTION
# ============================================================
def publish_vehicle_data(plate_id, distance, frame_num, fps):
    """
    بيبعت بيانات العربية على MQTT
    الـ Gateway هيستقبلها ويحدد لو فيه طوارئ أو لا
    """
    # 🛠️ التعديل هنا: فحص ومطابقة المعرف الثابت Rex بنفس حالة الحروف والالتزام بشكلها تماماً
    is_amb = (plate_id.strip() == "Rex")

    payload = {
        "plate_id"    : plate_id,          # ID اللوحة أو "???"
        "distance_m"  : round(distance, 1), # المسافة من الكاميرا
        "frame"       : frame_num,
        "time_sec"    : round(frame_num / fps, 1),
        "is_ambulance": is_amb,             # القيمة المعدلة بناءً على الأي دي الثابت
        "timestamp"   : time.time()
    }

    mqtt_client.publish(CAMERA_DETECTION_TOPIC, json.dumps(payload))
    print(f"   📡 Published → plate: {plate_id} | dist: {distance:.1f}m | emergency: {is_amb}")

# ============================================================
# VIDEO PROCESSING
# ============================================================
cap         = cv2.VideoCapture(video_path)
fps         = int(cap.get(cv2.CAP_PROP_FPS)) or 30
frame_count = 0
all_detections = []
unique_plates  = set()

print(f"\n🎥 Processing video at {fps} FPS ...")

while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame_count += 1

    # نعالج كل 15 فريم بس (تقريباً كل نص ثانية)
    if frame_count % 15 != 0:
        continue

    results = detector(frame, verbose=False)

    for box in results[0].boxes:
        if int(box.cls[0]) != 2 or float(box.conf[0]) <= 0.4:
            continue

        x1, y1, x2, y2 = map(int, box.xyxy[0])

        # حساب المسافة
        distance = distance_calc.calculate(x2 - x1)
        if not distance or distance > 100:
            continue

        plate_id = None

        # قراءة اللوحة من الجزء السفلي من bounding box
        plate_y1     = max(0, int(y2 - (y2 - y1) * 0.35))
        plate_region = frame[plate_y1:y2, x1:x2]

        if plate_region.size > 0:
            enhanced = enhance_plate(plate_region)
            ocr_result = reader.readtext(enhanced, detail=0, paragraph=True)
            if ocr_result:
                plate_text = ocr_result[0].strip().replace(' ', '')
                plate_text = ''.join(c for c in plate_text if c.isalnum() or c == '-')
                if len(plate_text) >= 3:
                    plate_id = plate_text
                    unique_plates.add(plate_text)

        final_plate = plate_id if plate_id else "???"

        # إرسال البيانات للبوابة الذكية
        publish_vehicle_data(final_plate, distance, frame_count, fps)

        # حفظ محلي
        all_detections.append({
            "frame"      : frame_count,
            "plate"      : final_plate,
            "distance_m" : round(distance, 1),
            "time_sec"   : round(frame_count / fps, 1)
        })

        if plate_id:
            print(f"   ✅ Frame {frame_count}: {plate_id} | {distance:.1f}m")
        else:
            print(f"   🚗 Frame {frame_count}: ??? | {distance:.1f}m")

cap.release()
mqtt_client.loop_stop()
mqtt_client.disconnect()

# ============================================================
# RESULTS
# ============================================================
print("\n" + "="*60)
print(f"📊 RESULTS: {len(all_detections)} detections | {len(unique_plates)} unique plates")
print("="*60)

if all_detections:
    df = pd.DataFrame(all_detections)
    print(df.to_string(index=False))
    print(f"\n📈 Unique plates: {unique_plates}")

    df.to_csv("all_detections.csv", index=False)

    with open("results.txt", "w") as f:
        f.write("Frame,Plate,Distance(m),Time(s)\n")
        for d in all_detections:
            f.write(f"{d['frame']},{d['plate']},{d['distance_m']},{d['time_sec']}\n")

print("\n✅ Done!")