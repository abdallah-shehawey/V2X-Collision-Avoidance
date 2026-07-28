# V2P — Vehicle-to-Pedestrian Safety System

Camera-based pedestrian and motorcycle detection running on the Raspberry Pi.
`V2P.py` reads frames from the Pi Camera, runs an ONNX object-detection model,
tracks each object across frames, and publishes safety flags onto the IPC hub.

```text
Pi Camera → ONNX model → CentroidTracker → risk logic → IPC Hub
                                                          ├─ v2p_frame
                                                          └─ motorcycle_alert
```

## Detection

- **Model**: `model2.onnx`, run via `onnxruntime`, input size 640×640.
- **Classes**: `person`, `bicycle`, `car`, `motorcycle`.
- **CentroidTracker**: assigns a stable ID to each detection across frames so
  approach speed and crossing behavior can be evaluated over time (not just
  per-frame).

## Risk logic

- **Proximity**: bounding-box area ratio vs. frame area, thresholded into
  safe / warning / danger bands.
- **Crossing zone**: a horizontal band across the frame (`CROSSING_ZONE_RATIO`)
  used to flag pedestrians actively crossing vs. just nearby.
- **Approach speed**: centroid displacement over `APPROACH_FRAMES`, classified
  fast/slow to distinguish an approaching pedestrian from a stationary one.
- **Warning priority**: `EMERGENCY > CROSSING > TOO_CLOSE > APPROACHING > CLOSE > NONE`
  — only the highest-priority state is reported per cycle.
- **Distance estimate**: `FOCAL_PX` and a per-class `REAL_HEIGHT_M` table give a
  rough monocular distance from bounding-box height.

## Traffic-light awareness

Subscribes to `v2n_frame` from [`../V2N/Car_client.py`](../V2N/README.md) (via
the hub) to know the current light state, and derives a pedestrian
walk/don't-walk string from the car's traffic flag.

## Emergency-vehicle awareness

`v2n_frame` also carries `is_emergency` (Car_client.py forwards it straight
from `Intelligent_Gateway.py`'s ambulance-preemption decision over MQTT).
When it's `True`, `V2P.py`:

- Detects **every frame** instead of every `skip_frames`-th one
  (`EMERGENCY_SKIP_FRAMES = 1`), for the fastest possible reaction while an
  ambulance is active.
- Draws a persistent on-screen banner ("EMERGENCY VEHICLE APPROACHING -
  CLEAR PATH").
- Publishes `emergency_active` on `v2p_frame` so the dashboard reflects it
  even between `v2n_frame` updates.

No new transport was needed for this — the flag was already flowing all the
way to this file's own `_on_v2n_frame` callback, just discarded before.

## What gets published

Two topics on the IPC hub (see [`../hub/README.md`](../hub/README.md)):

```text
"v2p_frame": {
  "pedestrian_flag":         int,  # 0/1/2 style severity
  "position_flag":           int,  # 0=none, 1=right, 2=left
  "lead_car_collision_flag": int,
  "emergency_active":        bool  # ambulance/emergency vehicle active on the network
}

"motorcycle_alert": {
  "motorcycle_collision_flag": int   # published only on state change
}
```

Both are consumed by `dashboard_bridge.py`, the only writer of
`RPI/DashBoard/data.json`.

## Run (on the Pi)

```bash
python3 V2P.py
```

Needs the camera (`picamera2`) and `onnxruntime` installed; not runnable
off-Pi. Normally managed by `v2x-v2p.service` (see
[`../systemd/README.md`](../systemd/README.md)), which waits for the camera
and the hub to be ready before starting.
