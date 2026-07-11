# RPI — Car On-Board Computer

Everything that runs on the car's Raspberry Pi: a small pub/sub IPC hub at
the center, with the traffic-light bridge, the pedestrian-detection camera,
the telemetry dashboard, and the phone remote control all wired to it as
independent processes. Each subsystem also has its own detailed README —
this file is the map of how they fit together and how to bring the whole
stack up.

```text
                         STM32 (V2V) ── UART ──► ESP32 ── ESP-NOW ──► (other cars)
                                          │
                                          │ UART (speed, telemetry)
                                          ▼
 HiveMQ Cloud (MQTT)          ┌─────────────────────────────────────────┐
   Traffic_Light RSU  ───────►│  V2N/Car_client.py                       │
                               │      │ publish("v2n_frame")             │
                               │      ▼                                  │
 Pi Camera            ───────►│  V2P/V2P.py                              │
   (pedestrian/moto AI)       │      │ publish("v2p_frame",              │
                               │      │         "motorcycle_alert")      │
                               │      ▼                                  │
                               │  hub/hub.py  (pub/sub broker)            │
                               │      │                                  │
                               │      ▼                                  │
                               │  hub/dashboard_bridge.py                 │
                               │      │ writes                           │
                               │      ▼                                  │
                               │  DashBoard/data.json                     │
                               └─────────────────────────────────────────┘
                                      │                        │
                                      ▼                        ▼
                          DashBoard/server.py :8000   Control/control_server.py :8001
                          (read-only telemetry UI)     (phone remote, polls /adas
                                                         on :8000 for the safety guard)
```

Every arrow inside the dashed box is the **IPC hub** — no process talks to
another directly, they all publish/subscribe through `hub/hub.py` over a Unix
domain socket. That means any one of them (camera AI, MQTT bridge, dashboard)
can crash and restart without taking the others down.

## Components

| Folder | What it does | Detail |
| --- | --- | --- |
| [`hub/`](hub/README.md) | Pub/sub broker (`hub.py`) + client library (`ipc_node.py`) + the **only** writer of `data.json` (`dashboard_bridge.py`) | Central IPC — everything below depends on this being up first |
| [`V2N/`](V2N/README.md) | `Car_client.py` — bridges HiveMQ Cloud (processed traffic-light state) with local vehicle speed, decides GO/STOP, publishes `v2n_frame` | Talks to the `Traffic_Light/` roadside unit over MQTT |
| [`V2P/`](V2P/README.md) | `V2P.py` — ONNX model on the Pi Camera feed, detects/tracks people, bikes, cars, motorcycles, publishes `v2p_frame` + `motorcycle_alert` | Also consumes `v2n_frame` to know the current light state |
| [`DashBoard/`](DashBoard/README.md) | `server.py` — serves the read-only telemetry web UI on `:8000`, reads `data.json` | Shows ADAS warnings, V2N/V2P/AI flags, speed/heading, ultrasonic sensors, event log |
| [`Control/`](Control/README.md) | `control_server.py` — phone remote-control UI on `:8001`, drives the L298N motors | Polls DashBoard's `/adas` endpoint and blocks unsafe moves (FCW/BSW critical, red light, pedestrian crossing, etc.) |
| [`systemd/`](systemd/README.md) | Unit files + `install_services.sh` | Runs `hub`, `dashboard_bridge`, `Car_client`, `V2P`, and `DashBoard/server.py` on boot, in dependency order |

## Data flow in one line

**STM32/ESP32 telemetry + MQTT traffic state + camera AI** all converge on
`data.json` through the hub, and `data.json` is the single source of truth
that both the telemetry dashboard and the remote-control safety guard read.

## Run everything (on the Pi)

The supported way is systemd — see [`systemd/README.md`](systemd/README.md):

```bash
cd systemd
chmod +x install_services.sh
sudo ./install_services.sh
```

This installs and starts, **in order**: `v2x-hub` → `v2x-dashboard-bridge` →
`v2x-car-client` → `v2x-v2p` → `v2x-server`. `Control/control_server.py`
(`:8001`) isn't part of the systemd set and is started manually — see
[`Control/README.md`](Control/README.md).

### Manual order (for local testing, no systemd)

```bash
python3 hub/hub.py                     # 1. broker — everything else needs this
python3 hub/dashboard_bridge.py        # 2. only writer of data.json
python3 V2N/Car_client.py              # 3. traffic-light bridge
python3 V2P/V2P.py                     # 4. pedestrian/moto camera AI
python3 DashBoard/server.py            # 5. telemetry UI — http://<pi-ip>:8000
python3 Control/control_server.py      # 6. phone remote — http://<pi-ip>:8001
```

`V2N` and `V2P` don't depend on each other and can start in either order, but
both need the hub (step 1) up first, and `DashBoard/server.py` needs
`dashboard_bridge.py` (step 2) actively writing `data.json` for the UI to show
live values.

## Known issues

Hardcoded MQTT credentials, no hub reconnect logic, an empty `model2.onnx`
placeholder in git, and a few other open items across these subsystems are
tracked in [`../CODE_REVIEW.md`](../CODE_REVIEW.md).
