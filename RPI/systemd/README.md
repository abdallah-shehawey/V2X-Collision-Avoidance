# systemd — Run the V2X Stack on Boot

Unit files + an installer script that turn every Raspberry Pi Python process
into an auto-starting, auto-restarting systemd service.

## Services

| Service | Runs | Depends on |
| --- | --- | --- |
| `v2x-hub.service` | [`../hub/hub.py`](../hub/README.md) | network only — starts first |
| `v2x-dashboard-bridge.service` | [`../hub/dashboard_bridge.py`](../hub/README.md) | hub |
| `v2x-car-client.service` | [`../V2N/Car_client.py`](../V2N/README.md) | hub, dashboard-bridge |
| `v2x-v2p.service` | [`../V2P/V2P.py`](../V2P/README.md) | hub, car-client (+ camera warm-up delay) |
| `v2x-server.service` | [`../DashBoard/server.py`](../DashBoard/README.md) | hub, dashboard-bridge |

`Control/control_server.py` (the phone remote, `:8001`) has no service file
here — it's normally started manually/separately, see
[`../Control/README.md`](../Control/README.md).

Startup order matters because everything downstream of the hub needs a live
socket to publish/subscribe on: hub → dashboard-bridge → car-client → v2p →
server, with short `ExecStartPre` sleeps between dependent services as a
safety margin.

## Install (on the Pi)

```bash
cd RPI/systemd
chmod +x install_services.sh
sudo ./install_services.sh
```

The script:

1. Checks it's running as root and that
   `/home/rpi/V2X-Collision-Avoidance/RPI` exists with all required Python
   files in place.
2. Copies all five `.service` files into `/etc/systemd/system/`.
3. Runs `systemctl daemon-reload`, then `enable`s each service (auto-start on
   every boot).
4. Starts them in the correct order and prints a status summary + the
   dashboard URL.

> The `WorkingDirectory=`/`ExecStart=` paths in the `.service` files assume
> the repo lives at `/home/rpi/V2X-Collision-Avoidance` — update them first if
> your Pi checkout is elsewhere.

## Useful commands

```bash
sudo systemctl status v2x-hub              # check hub
sudo journalctl -u v2x-car-client -f       # live logs for one service
sudo systemctl restart v2x-v2p             # restart V2P
sudo systemctl stop v2x-hub                # stop all (hub down = cascade)
```
