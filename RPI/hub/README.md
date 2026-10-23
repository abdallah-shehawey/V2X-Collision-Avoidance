# IPC Hub — Pub/Sub Core

The central message router for every Python process on the car's Raspberry Pi.
Nothing talks to anything else directly — every process publishes to a topic
or subscribes to one, over a Unix domain socket. This keeps `V2N`, `V2P`,
`DashBoard`, and `Control` fully decoupled: any of them can restart without
the others knowing.

```text
                 publish("v2n_frame", data)
  Car_client ──────────────────────────────────► HUB
                                                    │
                                         topic registry
                                         "v2n_frame" -> [dashboard_bridge]
                                         "v2p_frame" -> [dashboard_bridge]
                                                    │
                                                    ▼
                                          dashboard_bridge (writes data.json)
```

## Files

| File | Role |
| --- | --- |
| `hub.py` | The broker. Listens on `/tmp/v2x_test.sock`, keeps a `name -> connection` map and a `topic -> subscriber names` registry, and fans out every `publish` to that topic's subscribers. |
| `ipc_node.py` | Client library every other process imports. Wraps `connect()` / `publish(topic, data)` / `subscribe(topic, callback)` around the socket + JSON wire protocol. |
| `dashboard_bridge.py` | A hub client itself — the **only** process that writes `RPI/DashBoard/data.json`. Subscribes to `v2n_frame` (from `Car_client.py`), `v2p_frame` and `motorcycle_alert` (from `V2P.py`), merges them into the dashboard's JSON schema. |

## Wire protocol

```text
Client -> Hub:
    {"cmd": "register",  "name": "v2p_camera"}
    {"cmd": "subscribe", "topic": "v2n_frame"}
    {"cmd": "publish",   "topic": "v2p_frame", "data": {...}}

Hub -> Client:
    {"ok": true, "name": "v2p_camera"}                register ack
    {"ok": true, "subscribed": "v2n_frame"}            subscribe ack
    {"topic": "v2n_frame", "data": {...}, "from": "car_client"}   delivered frame
```

## Topics in use

| Topic | Publisher | Subscriber |
| --- | --- | --- |
| `v2n_frame` | `RPI/V2N/Car_client.py` | `dashboard_bridge.py` |
| `v2p_frame` | `RPI/V2P/V2P.py` | `dashboard_bridge.py` |
| `motorcycle_alert` | `RPI/V2P/V2P.py` | `dashboard_bridge.py` |

## Run (on the Pi)

```bash
python3 hub.py              # start the broker first
python3 dashboard_bridge.py # then the bridge (needs the hub up)
```

Both are normally managed by systemd — see [`../systemd/README.md`](../systemd/README.md).
`v2x-hub.service` starts before every other V2X service; if it goes down, every
other node loses its connection (see the note on missing reconnect logic in
[`../../CODE_REVIEW.md`](../../CODE_REVIEW.md)).
