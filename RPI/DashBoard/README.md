# V2X Dashboard (Web)

A web interface for the vehicle. Open it from any device on the same network —
no car screen required.

**Shows only real project data:**

- **ADAS safety warnings** — EEBL, FCW, BSW, DNPW, IMA (Safe / Warning / Critical)
- **Speed + heading + attitude** — from the MPU9250 (speedometer, compass, pitch/roll)
- **Engine temperature + weather** — stat tiles on the left
- **6 ultrasonic sensors** — top-down car view; each sensor shows live distance
  and lights up green → yellow → red as an obstacle gets closer
- **Live motion** — the road scrolls in proportion to speed (stops when parked)
- **Event log + overall risk level** — auto-generated from state changes

## How it works

```text
data.json  -->  web dashboard (index.html)
(one file,       (polls the file every
 all values)      ~1s and displays it)
```

- **`data.json`** — the single source of truth. Every value on screen lives here.
- **`server.py`** — only *serves* the files. It never changes any value.
- The front-end (`index.html`, `css/`, `js/`) only *reads* `data.json`.

## Run

```bash
cd DashBoard
python3 server.py
```

Then open the printed URL (e.g. `http://localhost:8000`), or the `network`
URL from your phone / another laptop on the same Wi-Fi.

## Test mode (now): change values by hand

The dashboard reads `data.json` live. To check it works:

1. Open `data.json` in any editor.
2. Change a value and **save**:
   - `drive.speedKmh` → moves the speedometer **and** the road-scroll speed
   - `drive.heading` (0–360) → turns the compass
   - `drive.pitch` / `drive.roll` → tilts the attitude indicator
   - `drive.vehicleTempC` → engine-temp tile (turns amber ≥95, red ≥105)
   - `weather.condition` / `tempC` / `humidity` → weather tile
   - `ultrasonic.*` distances in **cm** — `front, frontLeft, frontRight, rear,
     rearLeft, rearRight` (≥120 = clear, 50–120 = near/yellow, <50 = close/red)
   - `adas.*` warnings use **0 = Safe, 1 = Warning, 2 = Critical**
3. The page updates within ~1 second. No refresh needed.

## Auto mode (later): real UART

When the STM is connected, a small reader parses the serial frame and writes the
**same** `data.json`. The dashboard does not change at all. A ready-to-fill
skeleton (`uart_reader`) is at the bottom of `server.py`.

## Files

| File | Purpose |
|------|---------|
| `data.json` | All vehicle values (edit by hand now, UART writes it later) |
| `server.py` | Static web server (+ UART reader skeleton for later) |
| `index.html` | Dashboard layout |
| `css/style.css` | Dark theme styling |
| `js/app.js` | Reads `data.json` and renders |
| `Logo/` | Sponsor / partner logos |
