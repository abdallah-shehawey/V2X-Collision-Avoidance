# Traffic_Light — Roadside Unit (RSU)

The infrastructure side of V2I/V2N: a simulated traffic light + a
camera/plate-detection pipeline, merged by a central gateway and published to
**HiveMQ Cloud (MQTT)** for the car to consume (see
[`../RPI/V2N/README.md`](../RPI/V2N/README.md)). Runs on a separate machine
from the car — communication is MQTT-only, no direct link.

```text
Traffic_light_GUI.py (lane A) ◄────────────┐  v2n/camera/vehicle_data
        │  v2n/traffic/light/state/A       │  (ambulance preemption input)
        │  ▲ v2n/traffic/light/command/A   │
        │  │                               │
Traffic_light_GUI.py (lane B) ◄────────────┤
        │  v2n/traffic/light/state/B       │
        │  ▲ v2n/traffic/light/command/B   │
        ▼  │                               │
distance.py (per lane) ─┬──►  Intelligent_Gateway.py  ──►  V2X/zone1/traffic/processed
  (camera + YOLO + OCR) │   (merges + Conflict Resolution)        │
                        │                                          ▼
                        └── v2n/camera/vehicle_data       RPI/V2N/Car_client.py (car)
```

## Files

| File | Role |
| --- | --- |
| `Traffic_light_GUI.py` | Tkinter RSU simulator for **one lane** (`LANE_ID`, default `"A"`): cycles RED → YELLOW → GREEN → YELLOW on configured durations, publishes state + transition code + remaining time to `v2n/traffic/light/state/{LANE_ID}`. Subscribes to `v2n/camera/vehicle_data` directly for local ambulance preemption, and to `v2n/traffic/light/command/{LANE_ID}` for cross-lane commands from the gateway's Conflict Resolution Module (see below). Run a second instance with a different `LANE_ID` to simulate a perpendicular lane at the same intersection. |
| `distance.py` | Reads a video feed (a file path via `cv2.VideoCapture`, not a live camera device — no camera hardware required), runs YOLO for vehicle detection and EasyOCR for plate reads, estimates distance per vehicle, publishes to `v2n/camera/vehicle_data` tagged with its own `LANE_ID` (default `"A"`, matching whichever `Traffic_light_GUI.py` instance it watches). The bundled default path is a Colab-only location; if missing it prompts for a local video file instead. |
| `Intelligent_Gateway.py` | Central node ("the brain"): subscribes to all lanes' feeds, tracks a vehicle registry, resolves ambulance/emergency preemption for the car-facing output, **and runs the Conflict Resolution Module** that arbitrates simultaneous emergency requests across conflicting lanes. Publishes the merged packet to `V2X/zone1/traffic/processed` — this is what `Car_client.py` on the car actually consumes (mirrors `PRIMARY_LANE`, `"A"` by default). |
| `yolov8n.pt` | YOLOv8-nano weights used by `distance.py`. |
| `plate_detections_with_distance.csv` / `results.txt` | Sample output/logs from a `distance.py` run (now include Detection Time and Emergency Status columns). |

## MQTT topics

| Topic | Publisher | Subscriber |
| --- | --- | --- |
| `v2n/traffic/light/state/{lane}` | `Traffic_light_GUI.py` (per lane) | `Intelligent_Gateway.py` (wildcard `.../state/+`) |
| `v2n/traffic/light/command/{lane}` | `Intelligent_Gateway.py` | the matching lane's `Traffic_light_GUI.py` |
| `v2n/camera/vehicle_data` | `distance.py` (per lane, tagged `lane_id`) | `Intelligent_Gateway.py`, `Traffic_light_GUI.py` |
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
