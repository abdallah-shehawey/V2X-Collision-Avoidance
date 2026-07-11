# V2N — Car On-Board Unit (OBU)

`Car_client.py` is the car's link to the outside world: it connects to
**two** data sources and merges them into one decision.

```text
HiveMQ Cloud (MQTT)  ──►  Car_client.py  ◄──  IPC Hub (vehicle_speed, local)
   traffic light             │
   processed state            ▼
                        publish("v2n_frame", ...)
                              │
                              ▼
                     dashboard_bridge.py (../hub)
```

1. **HiveMQ Cloud (MQTT)** — subscribes to `V2X/zone1/traffic/processed`, the
   processed traffic packet published by `Traffic_Light/Intelligent_Gateway.py`.
2. **IPC Hub (local)** — the vehicle's own speed, forwarded from the STM32 over
   UART via the hub (see [`../hub/README.md`](../hub/README.md)).

## What gets published

Topic **`v2n_frame`**, every field always present:

```text
{
  "traffic_flag":        int   # 0=no light, 1=GO, 2=STOP
  "transition_flag":     int   # 0:G→Y, 1:Y→R, 2:R→Y, 3:Y→G, -1:mid-phase
  "state":                str   # raw "RED"|"GREEN"|"YELLOW"
  "remaining_time":       int   # seconds until state changes
  "is_emergency":         bool
  "warning":               str
  "density":                int
  "nearby_count":           int
  "closest_vehicle": {
      "plate_id":   str|null,
      "distance_m": float|null
  }
}
```

## `traffic_flag` logic

Single unified flag, ordered from least to most dangerous:

- **0 — no light/unknown**: no traffic light detected, normal driving.
- **1 — GO**: green with enough time to cross, or an emergency vehicle is
  active (the light will change for it — treated as a normal "go").
- **2 — STOP**: red or yellow, or green but *not* enough time left to cross.

The crossing-time check that resolves GREEN into GO/STOP:

```text
t_car = distance_m / (speed_kmh / 3.6)
if remaining_time >= t_car:  GO
else:                         STOP
```

`distance_to_light_m` is computed internally for this check only — it is
**not** part of the published frame and never reaches `data.json`.

## Run (on the Pi)

```bash
python3 Car_client.py
```

Normally managed by `v2x-car-client.service` (see
[`../systemd/README.md`](../systemd/README.md)), which starts after the hub
and the dashboard bridge are up.

## Note

The MQTT broker credentials are currently hardcoded at the top of
`Car_client.py` (and duplicated in `Traffic_Light/Intelligent_Gateway.py`).
That's tracked as a known issue — see [`../../CODE_REVIEW.md`](../../CODE_REVIEW.md).
