"""
v2v_uart_sender.py  –  V2V UART Sender Node
=============================================
يستقبل البيانات من الـ IPC hub (اللي V2P بيبعتها)
وبعدين يبعتها للـ STM32 التاني عبر UART.

الـ STM32 من جهته يستقبل الرسالة ويحفظها في ملف
(أو يعملها process) — شرحه في الأسفل.

Hardware path
-------------
    Raspberry Pi  ──UART──►  STM32 #2

UART Protocol (RPi → STM32)
----------------------------
    الـ RPi بيبعت سطر ASCII بالشكل ده:
        V2P:<car_count>,<front_cm>,<alert_level>,<tl_state>\n
    مثال:
        V2P:2,350.5,caution,RED\n

    الـ STM32 يحلل السطر ده ويستخدمه.

STM32 جانب الاستقبال (pseudocode / C)
--------------------------------------
    // في الـ STM32 firmware:
    char buf[64];
    HAL_UART_Receive(&huart2, (uint8_t*)buf, sizeof(buf), 1000);
    // buf يحتوي: "V2P:2,350.5,caution,RED\n"
    // parse وحفظ في متغيرات:
    //   car_count, front_cm, alert_level, tl_state

IPC Role
--------
    Node name  : "v2v_sender"
    Subscribes : "vehicle_data"       (from v2p_camera)
                 "pedestrian_status"  (from v2p_camera)

Data file written by STM32 side
---------------------------------
    لما الـ STM32 يستقبل، المفروض يعمل HAL_UART_Transmit confirmation:
        ACK:OK\n
    والـ RPi هيلوج ده.
    لو عايز تحاكي الجانب ده، فيه stub في الأسفل.
"""

import time
import json
import sys
import os
import threading
from datetime import datetime

# ── IPC ───────────────────────────────────────────────────────────────────────
from ipc_node import IPCNode

# ═══════════════════════════════════════════════════════════════════════════════
# Configuration
# ═══════════════════════════════════════════════════════════════════════════════

NODE_NAME       = "v2v_sender"
SUB_TOPICS      = ["vehicle_data", "pedestrian_status"]

# UART settings (RPi → STM32 #2)
UART_PORT       = "/dev/ttyAMA0"   # عدّلها للـ port الصح
# UART_PORT     = "/dev/ttyUSB0"
UART_BAUDRATE   = 9600

# لو مفيش STM32 متوصل، اشغّل dry-run (بس اطبع بدل ما تبعت)
SIMULATION_MODE = True

# الملف اللي الـ STM32 هيحفظ فيه البيانات المستقبلة (على الـ RPi side للـ ACK)
ACK_LOG_FILE    = "v2v_stm32_ack.jsonl"

# ═══════════════════════════════════════════════════════════════════════════════
# Shared state (updated by IPC callbacks)
# ═══════════════════════════════════════════════════════════════════════════════

_latest: dict = {
    "vehicle_data":       None,
    "pedestrian_status":  None,
}
_lock = threading.Lock()


# ═══════════════════════════════════════════════════════════════════════════════
# UART Sender
# ═══════════════════════════════════════════════════════════════════════════════

class UARTSender:
    """يبعت رسائل للـ STM32 عبر UART."""

    def __init__(self, port: str, baudrate: int) -> None:
        self.port     = port
        self.baudrate = baudrate
        self.ser      = None

    def open(self) -> bool:
        try:
            import serial
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0,
            )
            print(f"[V2V-TX] ✅  UART opened: {self.port} @ {self.baudrate} baud")
            return True
        except ImportError:
            print("[V2V-TX] ❌  pyserial not installed — run: pip install pyserial")
            return False
        except Exception as exc:
            print(f"[V2V-TX] ❌  Cannot open {self.port}: {exc}")
            return False

    def send(self, message: str) -> bool:
        """يبعت سطر نصي للـ STM32. يرجع True لو نجح."""
        if not self.ser or not self.ser.is_open:
            return False
        try:
            self.ser.write((message + "\n").encode("utf-8"))

            # انتظر ACK من الـ STM32 (اختياري)
            ack = self.ser.readline().decode("utf-8", errors="ignore").strip()
            if ack.startswith("ACK"):
                return True
            return True   # نكمل حتى لو مفيش ACK
        except Exception as exc:
            print(f"[V2V-TX] ⚠️  UART send error: {exc}")
            return False

    def close(self) -> None:
        if self.ser and self.ser.is_open:
            self.ser.close()


class SimulatedUARTSender:
    """بيطبع بدل ما يبعت — للاختبار بدون hardware."""

    def open(self) -> bool:
        print("[V2V-TX] 🤖  Simulation mode — will PRINT instead of sending via UART")
        return True

    def send(self, message: str) -> bool:
        print(f"[V2V-TX] 📤  [SIMULATED UART → STM32]: {message}")
        return True

    def close(self) -> None:
        pass


# ═══════════════════════════════════════════════════════════════════════════════
# ACK Logger
# ═══════════════════════════════════════════════════════════════════════════════

class AckLogger:
    def __init__(self, filepath: str) -> None:
        self.filepath = filepath
        print(f"[V2V-TX] 📁  ACK log: {os.path.abspath(filepath)}")

    def log(self, sent_msg: str, success: bool) -> None:
        entry = {
            "datetime": datetime.now().isoformat(),
            "sent":     sent_msg,
            "ok":       success,
        }
        try:
            with open(self.filepath, "a", encoding="utf-8") as f:
                f.write(json.dumps(entry) + "\n")
        except IOError:
            pass


# ═══════════════════════════════════════════════════════════════════════════════
# IPC Callbacks
# ═══════════════════════════════════════════════════════════════════════════════

def _on_vehicle_data(topic: str, data: dict, sender: str) -> None:
    """يُحدّث الـ state لما V2P يبعت vehicle_data."""
    with _lock:
        _latest["vehicle_data"] = data
    speed   = data.get("speed_kmh", 0)
    cars    = data.get("car_count", 0)
    front   = data.get("ultrasonic", {}).get("front_cm", 0)
    print(
        f"[V2V-TX] 📥  vehicle_data: "
        f"speed={speed:.1f}km/h  cars={cars}  front={front}cm"
    )


def _on_pedestrian_status(topic: str, data: dict, sender: str) -> None:
    """يُحدّث الـ state لما V2P يبعت pedestrian_status."""
    with _lock:
        _latest["pedestrian_status"] = data
    count = data.get("pedestrian_count", 0)
    alert = data.get("alert_level", "none")
    light = data.get("traffic_light", "?")
    print(
        f"[V2V-TX] 📥  pedestrian_status: "
        f"peds={count}  alert={alert}  light={light}"
    )


# ═══════════════════════════════════════════════════════════════════════════════
# Message Builder
# ═══════════════════════════════════════════════════════════════════════════════

def build_uart_message(veh: dict | None, ped: dict | None) -> str | None:
    """
    يبني الرسالة اللي هتتبعت للـ STM32.

    Format:
        V2P:<car_count>,<front_cm>,<alert_level>,<tl_state>\n

    مثال:
        V2P:3,285.0,danger,RED
    """
    if veh is None and ped is None:
        return None

    car_count   = veh.get("car_count",  0)    if veh else 0
    front_cm    = veh.get("ultrasonic", {}).get("front_cm", 0) if veh else 0
    alert_level = ped.get("alert_level", "none") if ped else "none"
    tl_state    = ped.get("traffic_light", "UNKNOWN") if ped else "UNKNOWN"

    return f"V2P:{car_count},{front_cm:.1f},{alert_level},{tl_state}"


# ═══════════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════════

def main() -> None:
    print("\n" + "=" * 58)
    print("🚀 V2V UART Sender Node")
    print(f"   Mode      : {'⚙️  SIMULATION' if SIMULATION_MODE else '🔌 HARDWARE UART'}")
    if not SIMULATION_MODE:
        print(f"   TX Port   : {UART_PORT}  @  {UART_BAUDRATE} baud")
    print(f"   Subscribes: {SUB_TOPICS}")
    print("=" * 58 + "\n")

    # ── Connect to IPC hub ────────────────────────────────────────────
    node = IPCNode(NODE_NAME)
    if not node.connect():
        print("[V2V-TX] ❌  Could not connect to hub. Is hub.py running?")
        sys.exit(1)

    # Subscribe to V2P topics
    node.subscribe("vehicle_data",      _on_vehicle_data)
    node.subscribe("pedestrian_status", _on_pedestrian_status)
    node.start_listening()

    print(f"[V2V-TX] ✅  Subscribed to: {SUB_TOPICS}\n")

    # ── Init UART ─────────────────────────────────────────────────────
    uart = SimulatedUARTSender() if SIMULATION_MODE else UARTSender(UART_PORT, UART_BAUDRATE)
    if not uart.open():
        print("[V2V-TX] ❌  Cannot open UART. Exiting.")
        sys.exit(1)

    ack_logger = AckLogger(ACK_LOG_FILE)

    print("\n[V2V-TX] ✅  Running — forwarding V2P data to STM32 ...\n")

    try:
        while True:
            with _lock:
                veh = _latest["vehicle_data"]
                ped = _latest["pedestrian_status"]

            msg = build_uart_message(veh, ped)

            if msg:
                ok = uart.send(msg)
                ack_logger.log(msg, ok)
                status = "✅" if ok else "❌"
                print(f"[V2V-TX] {status}  Sent to STM32: '{msg}'")
            else:
                print("[V2V-TX] ⏳  Waiting for data from V2P ...")

            time.sleep(1.0)

    except KeyboardInterrupt:
        print("\n\n" + "=" * 58)
        print("🛑  V2V Sender stopped.")
        print("=" * 58)

    finally:
        uart.close()


if __name__ == "__main__":
    main()
