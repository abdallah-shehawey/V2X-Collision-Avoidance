# Traffic_Light — Roadside Unit (RSU)

The infrastructure side of V2I/V2N: a simulated traffic light + a
camera/plate-detection pipeline, merged by a central gateway and published to
**HiveMQ Cloud (MQTT)** for the car to consume (see
[`../RPI/V2N/README.md`](../RPI/V2N/README.md)). Runs on a separate machine
from the car — communication is MQTT-only, no direct link.

```text
Traffic_light_GUI.py ◄─────────────────────┐  v2n/camera/vehicle_data
        │  v2n/traffic/light/state         │  (ambulance preemption input)
        │                                  │
        ▼                                  │
distance.py ────────────┬──►  Intelligent_Gateway.py  ──►  V2X/zone1/traffic/processed
  (camera + YOLO + OCR) │        (merges + decides)              │
                        │                                         ▼
                        └── v2n/camera/vehicle_data      RPI/V2N/Car_client.py (car)
```

## Files

| File | Role |
| --- | --- |
| `Traffic_light_GUI.py` | Tkinter RSU simulator: cycles RED → YELLOW → GREEN → YELLOW on configured durations, publishes state + transition code + remaining time to `v2n/traffic/light/state`. Also subscribes to `v2n/camera/vehicle_data` directly, so it can preempt its own cycle on an ambulance detection (see "Emergency preemption" below) instead of just reporting state one-way. |
| `distance.py` | Reads a video feed (a file path via `cv2.VideoCapture`, not a live camera device — no camera hardware required), runs YOLO for vehicle detection and EasyOCR for plate reads, estimates distance per vehicle, publishes to `v2n/camera/vehicle_data`. The bundled default path is a Colab-only location; if missing it prompts for a local video file instead. |
| `Intelligent_Gateway.py` | Central node: subscribes to both feeds above, tracks a vehicle registry, resolves ambulance/emergency preemption, and publishes the merged packet to `V2X/zone1/traffic/processed` — this is what `Car_client.py` on the car actually consumes. |
| `yolov8n.pt` | YOLOv8-nano weights used by `distance.py`. |
| `plate_detections_with_distance.csv` / `results.txt` | Sample output/logs from a `distance.py` run. |

## MQTT topics

| Topic | Publisher | Subscriber |
| --- | --- | --- |
| `v2n/traffic/light/state` | `Traffic_light_GUI.py` | `Intelligent_Gateway.py` |
| `v2n/camera/vehicle_data` | `distance.py` | `Intelligent_Gateway.py`, `Traffic_light_GUI.py` |
| `V2X/zone1/traffic/processed` | `Intelligent_Gateway.py` | `RPI/V2N/Car_client.py` (on the car) |

Broker is HiveMQ Cloud (TLS, port 8883). Credentials are currently hardcoded
in each script rather than loaded from environment/config — same caveat as
noted in [`../RPI/V2N/README.md`](../RPI/V2N/README.md).

## Emergency preemption

Two independent things react to an ambulance detection on `v2n/camera/vehicle_data`:

1. **`Traffic_light_GUI.py`** actually changes the physical light based on its
   current phase:
   - **RED** → immediately advances through **YELLOW** into **GREEN** (never
     skips straight from RED to GREEN — the YELLOW step is always kept).
   - **GREEN** with more than `GREEN_EXTENSION_THRESHOLD` (3s) left → no
     action needed, the ambulance will make the light in time.
   - **GREEN** with `GREEN_EXTENSION_THRESHOLD` seconds or fewer left → if the
     ambulance is still detected, extends GREEN by `GREEN_EXTENSION_SECONDS`
     (5s); if it's no longer detected, the phase changes on schedule. Capped
     at `MAX_GREEN_EXTENSIONS` (3) extensions per GREEN phase so a stuck
     detection can't hold the light forever.
   - **YELLOW** is left alone — it's already a short, fixed transition.
2. **`Intelligent_Gateway.py`** separately latches a global `is_emergency`
   flag (from `ambulance_active`, `camera_confirmed`, or `camera_ambulance`)
   into the packet it forwards to the car, regardless of the light's phase.

Both sides trust the `is_ambulance` boolean each camera detection payload
carries as the primary signal, falling back to matching `plate_id` against a
local `AMBULANCE_ID` constant only if that flag is missing. This matters
because **that constant's value is inconsistent across files** (the gateway
uses `"REX"`, the camera scripts use `"T4RR"`) — relying on the boolean flag
first means preemption still works correctly despite the mismatch, but the
mismatch itself is still worth fixing by picking one real ID and using it
everywhere.

## Run

```bash
python3 Traffic_light_GUI.py    # RSU state machine + GUI
python3 distance.py             # camera/plate pipeline (needs a video FILE, not a live camera)
python3 Intelligent_Gateway.py  # merges both, publishes the processed packet
```

Each is a standalone process — start the gateway last so it has both feeds to
subscribe to.
