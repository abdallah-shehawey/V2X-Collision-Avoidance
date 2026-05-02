"""
main3.py - V2V / STM32 UART Bridge
=====================================
Simulates the Vehicle-to-Vehicle (V2V) subsystem. In the real hardware
setup, an STM32 microcontroller reads the IMU (MPU-9250), ultrasonic
sensors, and GPS (NEO-8M) at high frequency and streams structured JSON
frames over UART (/dev/ttyAMA0) to the Raspberry Pi.

This process replaces the UART reader for local testing: it generates
realistic vehicle telemetry frames at 1 Hz and publishes them to the
"vehicle_data" topic so the ADAS process (main4) can consume them.

It also listens on "pedestrian_status" so it can simulate the real
system's behaviour of alerting the STM32 when pedestrians are detected
(the STM32 would then activate the buzzer and warning LEDs on the car).

Topic published   : "vehicle_data"
Topic subscribed  : "pedestrian_status"
Data schema (published):
    {
        "vehicle_id"    : str,
        "speed_kmh"     : float,
        "acceleration"  : float,          m/s²  (negative = braking)
        "brake_pressed" : bool,
        "heading_deg"   : float,          0 = North, 90 = East
        "gps"           : {"lat": float, "lng": float},
        "ultrasonic"    : {               distances in cm
            "front_cm"  : int,
            "rear_cm"   : int,
        },
        "turn_signal"   : "none" | "left" | "right",
        "hazard_lights" : bool
    }
"""

import time
import math
from ipc_node import IPCNode

VEHICLE_ID  = "VH-001"
BASE_LAT    = 30.044399
BASE_LNG    = 31.235712


def on_pedestrian_status(topic: str, data: dict, sender: str) -> None:
    """
    React to pedestrian status updates from the V2P camera.

    In the real system this would trigger the STM32 to activate
    the buzzer and warning LEDs via UART.
    """
    if data.get("alert_level") in ("caution", "danger"):
        print(
            f"\n[V2V] ⚠️  Pedestrian alert received from {sender} | "
            f"alert={data['alert_level']} | "
            f"safe_to_cross={data['crossing_safe']}"
        )
        print("[V2V] → STM32 UART: activate buzzer + warning LEDs")
    elif data.get("crossing_safe"):
        print(f"[V2V] ✅ Pedestrians crossing safely — no action required")


def build_telemetry(tick: int) -> dict:
    """
    Generate a realistic telemetry frame for the current simulation tick.

    Simulates a vehicle decelerating as it approaches a red light.
    """
    # Simulate slowing down: speed drops from 50 to 10 over 17 ticks
    speed     = max(10.0, 50.0 - tick * 2.5)
    accel     = -2.5 if speed > 10.0 else 0.0
    brake     = speed < 30.0
    front_cm  = max(20, 900 - tick * 50)   # obstacle getting closer

    # Advance GPS position slightly each tick (eastward)
    lat = BASE_LAT + tick * 0.000015
    lng = BASE_LNG + tick * 0.000030

    return {
        "vehicle_id":    VEHICLE_ID,
        "speed_kmh":     round(speed, 1),
        "acceleration":  round(accel, 2),
        "brake_pressed": brake,
        "heading_deg":   90.0,
        "gps":           {"lat": round(lat, 6), "lng": round(lng, 6)},
        "ultrasonic":    {"front_cm": front_cm, "rear_cm": 400},
        "turn_signal":   "none",
        "hazard_lights": False,
    }


def main() -> None:
    node = IPCNode("v2v_stm32")

    if not node.connect():
        return

    node.subscribe("pedestrian_status", on_pedestrian_status)
    node.start_listening()

    print("[V2V] STM32 UART bridge started")
    print("[V2V] Publishing to  : 'vehicle_data'")
    print("[V2V] Subscribed to  : 'pedestrian_status'\n")

    tick = 0
    while True:
        telemetry = build_telemetry(tick)
        node.publish("vehicle_data", telemetry)
        print(
            f"[V2V] publish  -> vehicle_data | "
            f"speed={telemetry['speed_kmh']}km/h | "
            f"brake={telemetry['brake_pressed']} | "
            f"front={telemetry['ultrasonic']['front_cm']}cm"
        )
        tick += 1
        time.sleep(1)


if __name__ == "__main__":
    main()
