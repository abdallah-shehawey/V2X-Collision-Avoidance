"""
dashboard_bridge.py - Hub -> data.json bridge for the V2X Dashboard
=====================================================================
هذا الملف عميل IPC تاني بس (زي ipc_node.py / hub.py). مش بيعمل أي
بيانات بنفسه — بس بيسمع لتوبيكين بالظبط على الـ hub:

    "v2n_frame"   — منشور من Car_client-1.py (طبقة V2N الـ flags المضغوطة)
    "v2p_frame"   — منشور من V2P.py          (طبقة V2P الـ flags المضغوطة)

وبيكتب القيم دي في data.json بنفس بساطة قسم "adas" الموجود أصلاً —
يعني الـ flag هو القيمة المعروضة مباشرة، مفيش dict متداخل زيادة عن
اللزوم.

ملحوظة مهمة
-------------
Car_client-1.py و V2P.py بينشروا كمان توبيكات تانية كتير على الـ hub
("local/traffic/processed", "pedestrian_status", "vehicle_data") عشان
أي عميل تاني على الـ hub يقدر ياخدها لو احتاجها. هذا الملف عمداً
**مش بيسمعش** للتوبيكات دي ولا بيكتبها في data.json — هو بس بيهتم
بالـ frame المضغوط (v2n_frame / v2p_frame) اللي هو الملخص النهائي
اللي الداشبورد محتاجه.

شكل data.json المتوقع (القيم دي موجودة فعلاً وبتتحدث بس مكانها)
------------------------------------------------------------------
    "v2n": {
        "trafficLight":      0 | 1 | 2,   # 0=مفيش إشارة، 1=قف (أحمر/أصفر)، 2=اعدي (أخضر)
        "ambulance":         0 | 1,        # 0=ممنوع تعدي، 1=مسموح (إسعاف معدّاة)
        "distanceToLightM":  <float|null>  # المسافة من كاميرا الإشارة بالمتر
    },
    "v2p": {
        "pedestrian": 0 | 1 | 2,           # 0=آمن، 1=واقفين جنب، 2=بيعدوا (warning)
        "position":   0 | 1 | 2 | 3 | null # مكان أقرب شخص عابر (ربع الطريق)
    }

التزامن / أمان الملف
----------------------
data.json ممكن كمان حد يعدله يدويًا (test mode) أو عملية تانية تكتبه
(زي uart_reader في server.py لجزء ADAS/drive، auto mode). هذا الجسر
دايمًا بيعمل read -> modify -> atomic replace — نفس النمط الموثق في
server.py — عشان المتصفح ميشوفش ملف نص مكتوب، حتى لو فيه أكتر من
كاتب شغال في نفس الوقت.
"""

import json
import os
import threading
import time

from ipc_node import IPCNode

# ──────────────────────────────────────────────────────────────────────
# Configuration
# ──────────────────────────────────────────────────────────────────────
NODE_NAME = "dashboard_bridge"

HERE = os.path.dirname(os.path.abspath(__file__))
DATA_FILE = os.path.join(HERE, "data.json")

# التوبيكات الوحيدة اللي الجسر ده بيسمعها وبيعكسها في data.json.
TOPIC_V2N_FRAME = "v2n_frame"   # from car_display (Car_client-1.py)
TOPIC_V2P_FRAME = "v2p_frame"   # from v2p_camera  (V2P.py)

# يحمي كل دورة read-modify-write على data.json عشان لو رسالتين
# (v2n_frame و v2p_frame) وصلوا في نفس اللحظة تقريبًا، الكتابة متتعارضش.
_file_lock = threading.Lock()


# ──────────────────────────────────────────────────────────────────────
# data.json helpers
# ──────────────────────────────────────────────────────────────────────
def _load_data() -> dict:
    """
    Load data.json from disk.

    بترجع dict فاضي بدل ما تعمل raise لو الملف مش موجود أو وقتيًا
    invalid (يعني عملية تانية بتكتب فيه في نفس اللحظة)، عشان قراءة
    وحشة وحدة متوقفش الجسر.
    """
    try:
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def _save_data(data: dict) -> None:
    """
    Write data.json atomically.

    بنكتب في ملف مؤقت الأول، وبعدين os.replace() فوق الملف الحقيقي.
    os.replace() atomic على POSIX وWindows، فأي حاجة بتقرا data.json
    (المتصفح، عن طريق server.py) محتعرفش تشوف ملف نص مكتوب.
    """
    tmp_path = DATA_FILE + ".tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.replace(tmp_path, DATA_FILE)


def _update_data(mutate) -> None:
    """
    تشغيل دورة read -> mutate -> write واحدة، محمية بـ _file_lock.

    Parameters
    ----------
    mutate : callable(dict) -> None
        فنكشن بتعدل الـ data dict اللي اتحمّل، في مكانها (مثلاً بتملى
        data["v2n"] أو data["v2p"]).
    """
    with _file_lock:
        data = _load_data()
        mutate(data)
        _save_data(data)


# ──────────────────────────────────────────────────────────────────────
# Hub callbacks — واحد لكل توبيك مشترك فيه
# ──────────────────────────────────────────────────────────────────────
def _on_v2n_frame(topic: str, payload: dict, sender: str) -> None:
    """
    استقبال "v2n_frame" (الـ flags المضغوطة من Car_client-1.py).

    بتكتب data["v2n"] = {"trafficLight": .., "ambulance": .., "distanceToLightM": ..}
    بنفس بساطة data["adas"] — كل قيمة flag رقم مباشر.
    """
    try:
        traffic_flag   = payload.get("traffic_flag", 0)
        ambulance_flag = payload.get("ambulance_flag", 0)
        distance_m     = payload.get("distance_to_light_m")

        def mutate(data: dict) -> None:
            data["v2n"] = {
                "trafficLight":     traffic_flag,
                "ambulance":        ambulance_flag,
                "distanceToLightM": distance_m,
            }

        _update_data(mutate)
        print(f"[{NODE_NAME}] v2n updated <- '{topic}' "
              f"(trafficLight={traffic_flag} ambulance={ambulance_flag} "
              f"distanceToLightM={distance_m}) (from {sender})")

    except Exception as exc:
        print(f"[{NODE_NAME}] error handling '{topic}': {exc}")


def _on_v2p_frame(topic: str, payload: dict, sender: str) -> None:
    """
    استقبال "v2p_frame" (الـ flags المضغوطة من V2P.py).

    بتكتب data["v2p"] = {"pedestrian": .., "position": ..}
    بنفس بساطة data["adas"].
    """
    try:
        pedestrian_flag = payload.get("pedestrian_flag", 0)
        position_flag   = payload.get("position_flag")  # ممكن تكون None

        def mutate(data: dict) -> None:
            data["v2p"] = {
                "pedestrian": pedestrian_flag,
                "position":   position_flag,
            }

        _update_data(mutate)
        print(f"[{NODE_NAME}] v2p updated <- '{topic}' "
              f"(pedestrian={pedestrian_flag} position={position_flag}) "
              f"(from {sender})")

    except Exception as exc:
        print(f"[{NODE_NAME}] error handling '{topic}': {exc}")


# ──────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────
def main() -> None:
    print("=" * 60)
    print("  V2X Dashboard Bridge — hub frames -> data.json")
    print("=" * 60)

    node = IPCNode(NODE_NAME)
    if not node.connect():
        print(f"[{NODE_NAME}] ERROR: could not reach the hub. "
              f"Make sure hub.py is running first.")
        return

    # الاشتراك في الـ frames المضغوطة بس — مش الداتا الخام.
    node.subscribe(TOPIC_V2N_FRAME, _on_v2n_frame)
    node.subscribe(TOPIC_V2P_FRAME, _on_v2p_frame)

    # start_listening() بتشغل thread في الخلفية بيفضل يقرا frames من
    # الهب ويشغل الكولباكس فوق؛ الـ main thread بس بيفضل حي عشان
    # العملية متخرجش.
    node.start_listening()
    print(f"[{NODE_NAME}] listening — data.json will update automatically "
          f"whenever v2n_frame / v2p_frame arrive.\n")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\n[{NODE_NAME}] stopped by user.")


if __name__ == "__main__":
    main()
