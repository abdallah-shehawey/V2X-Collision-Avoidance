# Car Control — phone remote

A small web remote that turns your phone into the car's steering wheel:
**direction arrows + a 5-speed gear shifter**, landscape, dark dashboard, made
to run with the screen kept awake. Separate from the telemetry DashBoard
(`../DashBoard`, read-only on `:8000`) — this one **drives** the car on `:8001`.

## Run (on the Pi)

```bash
/usr/bin/python3 control_server.py
```

It prints a `http://<pi-ip>:8001` URL. Open it on your phone (same Wi-Fi),
rotate to **landscape**, and tap **☀** once to keep the screen awake.

> On a laptop (no `gpiozero`) it runs in **SIM mode**: the UI and all endpoints
> work, but no GPIO is driven — handy for testing the page.

## How it drives

- **Hold** an arrow → the car moves in that direction (▲ forward, ▼ reverse,
  ◄/► spin-in-place turns). **Release** → it stops.
- The phone re-sends the held direction every ~120 ms; a **watchdog** in the
  server stops the motors if nothing arrives for `IDLE_T` (0.3 s). So lifting
  your finger stops the car — and so does a dropped Wi-Fi, a hidden tab, or a
  closed page. The **■** button forces an immediate stop.
- The gear shifter (1–5) sets the speed (15 % → 100 %); changing gear while
  holding a direction updates the speed live.

Pin map / motor logic is identical to `../keyboard_control.py`
(Motor A: ENA=18, IN1=23, IN2=24 · Motor B: ENB=19, IN3=27, IN4=22).

## Safety guard (ADAS + V2N / V2P / AI)

The server polls the telemetry DashBoard's `/adas` endpoint (`:8000`) and refuses
any move the safety state flags as dangerous:

**V2V (STM32 firmware, CRITICAL only):**

- **FCW critical** → **forward (▲) is blocked** (downgraded to stop).
- **BSW critical** → the **turn into the flagged blind-spot side** is blocked
  (◄ for a left alert, ► for a right alert, both for a both-sides alert).

**Smart systems (v2n / v2p / ai flags relayed through the DashBoard):**

- **AI lead-car DANGER** (`leadCarCollision = 2`) → forward blocked.
- **Red light / road closed** (`trafficLight = 2`) → forward blocked.
- **Pedestrians crossing** (`pedestrian = 2`) → forward blocked.
- **Motorcycle collision risk** (`motorcycleCollision = 1`) → forward blocked.
- **V2P side hazard** (`position = 1` right / `2` left) → the turn into that
  side is blocked.

The block happens before the motors are driven, so it holds no matter what the
phone sends. The poll is best-effort: if the DashBoard is down it **fails open**
(ordinary driving is never blocked), and the `/cmd` reply carries a `blocked`
field (e.g. `"FCW"`, `"RED-LIGHT"`, `"V2P-LEFT"`) when a move was refused — the
phone shows it as a red **BLOCKED** banner with a short vibration.

## Files

| file | what |
|------|------|
| `control_server.py` | HTTP server + L298N motor driver + safety watchdog |
| `index.html`        | dashboard layout (arrows, gears, read-out) |
| `css/style.css`     | dark landscape theme, full-bleed, no-scroll |
| `js/app.js`         | hold-to-move, gear select, wake-lock, link status |
