"""
v2v_uart_receiver.py  –  V2V UART Receiver Node
=================================================
يستقبل بيانات السرعة من STM32 عبر UART، يحفظها في ملف،
ويبعتها للـ IPC hub على topic "v2v_data".

Hardware path
-------------
    STM32  ──UART──►  Raspberry Pi (/dev/ttyAMA0 or /dev/ttyUSB0)

UART Protocol (STM32 → RPi)
----------------------------
    STM32 يبعت كل ثانية سطر ASCII بالشكل ده:
        SPEED:<value_kmh>\n
    مثال:
        SPEED:45.3\n

    لو مفيش hardware حقيقي، في SIMULATION_MODE تشتغل بقيمة ثابتة.

IPC Role
--------
    Node name  : "v2v_receiver"
    Publishes  : "v2v_data"

Published schema ("v2v_data")
------------------------------
{
    "timestamp"  : float,   # Unix time
    "speed_kmh"  : float,   # السرعة المستقبلة من STM32
    "source"     : "uart" | "simulated",
    "raw"        : str      # الرسالة الخام من UART
}

Data file
---------
    كل رسالة مستقبلة تُكتب في:  v2v_speed_log.jsonl
    (JSON Lines — سطر لكل قراءة)
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

NODE_NAME       = "v2v_receiver"
PUBLISH_TOPIC   = "v2v_data"
LOG_FILE        = "v2v_speed_log.jsonl"

# UART settings
UART_PORT       = "/dev/ttyAMA0"   # لـ Raspberry Pi GPIO UART
# UART_PORT     = "/dev/ttyUSB0"   # لو بتستخدم USB-to-UART adapter
UART_BAUDRATE   = 9600

# لو مفيش STM32 متوصل، اشغّل simulation بقيمة ثابتة
SIMULATION_MODE = True
SIM_SPEED_KMH   = 60.0            # القيمة الثابتة للاختبار

PUBLISH_INTERVAL = 1.0             # ثانية بين كل publish


# ═══════════════════════════════════════════════════════════════════════════════
# UART Reader
# ═══════════════════════════════════════════════════════════════════════════════

class UARTReader:
    """
    يقرأ بيانات السرعة من STM32 عبر UART.
    يحلل الرسائل بالشكل: SPEED:<value>\n
    """

    def __init__(self, port: str, baudrate: int) -> None:
        self.port     = port
        self.baudrate = baudrate
        self.ser      = None
        self._last_speed: float = 0.0
        self._lock    = threading.Lock()

    def open(self) -> bool:
        """يفتح UART port. يرجع True لو نجح."""
        try:
            import serial
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=2.0,
            )
            print(f"[V2V-RX] ✅  UART opened: {self.port} @ {self.baudrate} baud")
            return True
        except ImportError:
            print("[V2V-RX] ❌  pyserial not installed — run: pip install pyserial")
            return False
        except Exception as exc:
            print(f"[V2V-RX] ❌  Cannot open {self.port}: {exc}")
            return False

    def read_speed(self) -> tuple[float | None, str]:
        """
        يقرأ سطر واحد من UART.
        Returns (speed_kmh, raw_line) أو (None, "") لو مفيش بيانات.
        """
        if not self.ser or not self.ser.is_open:
            return None, ""
        try:
            line = self.ser.readline().decode("utf-8", errors="ignore").strip()
            if line.startswith("SPEED:"):
                speed = float(line.split(":")[1])
                with self._lock:
                    self._last_speed = speed
                return speed, line
        except (ValueError, IndexError):
            print(f"[V2V-RX] ⚠️  Bad UART line: '{line}'")
        except Exception as exc:
            print(f"[V2V-RX] ⚠️  UART read error: {exc}")
        return None, ""

    def close(self) -> None:
        if self.ser and self.ser.is_open:
            self.ser.close()


# ═══════════════════════════════════════════════════════════════════════════════
# Simulator (بديل UART لما مفيش hardware)
# ═══════════════════════════════════════════════════════════════════════════════

class SpeedSimulator:
    """
    يحاكي STM32 بإرسال قيمة ثابتة للسرعة.
    استخدمه للاختبار قبل توصيل الـ hardware.
    """

    def __init__(self, speed_kmh: float = SIM_SPEED_KMH) -> None:
        self.speed = speed_kmh

    def read_speed(self) -> tuple[float, str]:
        raw = f"SPEED:{self.speed}"
        return self.speed, raw

    def close(self) -> None:
        pass


# ═══════════════════════════════════════════════════════════════════════════════
# Data Logger
# ═══════════════════════════════════════════════════════════════════════════════

class DataLogger:
    """
    يحفظ كل قراءة سرعة في ملف JSONL.
    كل سطر في الملف = JSON object كامل.
    """

    def __init__(self, filepath: str) -> None:
        self.filepath = filepath
        print(f"[V2V-RX] 📁  Logging to: {os.path.abspath(filepath)}")

    def log(self, entry: dict) -> None:
        try:
            with open(self.filepath, "a", encoding="utf-8") as f:
                f.write(json.dumps(entry) + "\n")
        except IOError as exc:
            print(f"[V2V-RX] ⚠️  Log write error: {exc}")


# ═══════════════════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════════════════

def main() -> None:
    print("\n" + "=" * 58)
    print("🚀 V2V UART Receiver Node")
    print(f"   Mode    : {'⚙️  SIMULATION' if SIMULATION_MODE else '🔌 HARDWARE UART'}")
    if not SIMULATION_MODE:
        print(f"   Port    : {UART_PORT}  @  {UART_BAUDRATE} baud")
    print(f"   Topic   : {PUBLISH_TOPIC}")
    print(f"   Log     : {LOG_FILE}")
    print("=" * 58 + "\n")

    # ── Connect to IPC hub ────────────────────────────────────────────
    node = IPCNode(NODE_NAME)
    if not node.connect():
        print("[V2V-RX] ❌  Could not connect to hub. Is hub.py running?")
        sys.exit(1)

    print(f"[V2V-RX] ✅  Connected to hub as '{NODE_NAME}'")
    print(f"[V2V-RX] 📡  Will publish to '{PUBLISH_TOPIC}'\n")

    # ── Init source ───────────────────────────────────────────────────
    if SIMULATION_MODE:
        source = SpeedSimulator(SIM_SPEED_KMH)
        source_label = "simulated"
        print(f"[V2V-RX] 🤖  Simulation: fixed speed = {SIM_SPEED_KMH} km/h")
    else:
        source = UARTReader(UART_PORT, UART_BAUDRATE)
        if not source.open():
            print("[V2V-RX] ❌  UART failed. Switch to SIMULATION_MODE=True to test.")
            sys.exit(1)
        source_label = "uart"

    logger = DataLogger(LOG_FILE)

    print("\n[V2V-RX] ✅  Running — reading speed every second ...\n")

    try:
        while True:
            speed, raw = source.read_speed()

            if speed is None:
                time.sleep(0.1)
                continue

            ts = time.time()

            # ── Build payload ─────────────────────────────────────────
            payload = {
                "timestamp": ts,
                "speed_kmh": speed,
                "source":    source_label,
                "raw":       raw,
            }

            # ── Log to file ───────────────────────────────────────────
            log_entry = {
                **payload,
                "datetime": datetime.fromtimestamp(ts).isoformat(),
            }
            logger.log(log_entry)

            # ── Publish to hub ────────────────────────────────────────
            node.publish(PUBLISH_TOPIC, payload)

            print(
                f"[V2V-RX] 📨  speed={speed:.1f} km/h  "
                f"source={source_label}  "
                f"→ hub topic='{PUBLISH_TOPIC}'"
            )

            time.sleep(PUBLISH_INTERVAL)

    except KeyboardInterrupt:
        print("\n\n" + "=" * 58)
        print("🛑  V2V Receiver stopped.")
        print("=" * 58)

    finally:
        source.close()


if __name__ == "__main__":
    main()
