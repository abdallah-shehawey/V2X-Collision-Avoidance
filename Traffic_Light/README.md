# Traffic_Light — Roadside Unit (RSU)

The infrastructure side of V2I/V2N: a simulated traffic light + a
camera/plate-detection pipeline, merged by a central gateway and published to
**HiveMQ Cloud (MQTT)** for the car to consume (see
[`../RPI/V2N/README.md`](../RPI/V2N/README.md)). Runs on a separate machine
from the car — communication is MQTT-only, no direct link.

```text
Traffic_light_GUI.py ──┐  v2n/traffic/light/state
                        │
distance.py ────────────┼──►  Intelligent_Gateway.py  ──►  V2X/zone1/traffic/processed
  (camera + YOLO + OCR) │        (merges + decides)              │
                        │                                         ▼
                        └── v2n/camera/vehicle_data      RPI/V2N/Car_client.py (car)
```

## Files

| File | Role |
| --- | --- |
| `Traffic_light_GUI.py` | Tkinter RSU simulator: cycles GREEN → YELLOW → RED on fixed durations, publishes state + transition code + remaining time to `v2n/traffic/light/state`. |
| `distance.py` | Reads a video/camera feed, runs YOLO for vehicle detection and EasyOCR for plate reads, estimates distance per vehicle, publishes to `v2n/camera/vehicle_data`. |
| `Intelligent_Gateway.py` | Central node: subscribes to both feeds above, tracks a vehicle registry, resolves ambulance/emergency preemption, and publishes the merged packet to `V2X/zone1/traffic/processed` — this is what `Car_client.py` on the car actually consumes. |
| `yolov8n.pt` | YOLOv8-nano weights used by `distance.py`. |
| `plate_detections_with_distance.csv` / `results.txt` | Sample output/logs from a `distance.py` run. |

## MQTT topics

| Topic | Publisher | Subscriber |
| --- | --- | --- |
| `v2n/traffic/light/state` | `Traffic_light_GUI.py` | `Intelligent_Gateway.py` |
| `v2n/camera/vehicle_data` | `distance.py` | `Intelligent_Gateway.py` |
| `V2X/zone1/traffic/processed` | `Intelligent_Gateway.py` | `RPI/V2N/Car_client.py` (on the car) |

Broker is HiveMQ Cloud (TLS, port 8883). Credentials are currently hardcoded
in each script rather than loaded from environment/config — same caveat as
noted in [`../RPI/V2N/README.md`](../RPI/V2N/README.md).

## Emergency preemption

Both `Intelligent_Gateway.py` and the camera scripts recognize a hardcoded
ambulance/priority vehicle ID and latch an emergency state so the gateway
reports GO regardless of the light's actual phase. **Note:** the ID currently
differs between the gateway (`"REX"`) and the camera scripts (`"T4RR"`) — see
[`../CODE_REVIEW.md`](../CODE_REVIEW.md) for this and other open issues in
this subsystem (deadlocks, stale MQTT connection reporting, watchdog corner
cases).

## Run

```bash
python3 Traffic_light_GUI.py    # RSU state machine + GUI
python3 distance.py             # camera/plate pipeline (needs a video/camera source)
python3 Intelligent_Gateway.py  # merges both, publishes the processed packet
```

Each is a standalone process — start the gateway last so it has both feeds to
subscribe to.
