"""
V2X Car — phone control dashboard (server).

A tiny stand-alone web remote for the car, kept completely separate from the
telemetry DashBoard (that one is read-only on :8000; this one DRIVES on :8001).

  * SERVES the control UI (index.html + css/js) over plain HTTP.
  * Receives drive commands from the phone and turns them into L298N motor
    output — reusing the exact gpiozero pin map and "kick + hold-to-move"
    feel from keyboard_control.py.

Driving model (matches the keyboard remote):
  * The phone HOLDS a direction: while your finger is on an arrow it POSTs the
    command every ~120ms. Lift the finger → the posts stop → a WATCHDOG in this
    server stops the motors. So "press = move, release = stop", and if the Wi-Fi
    drops or the tab closes the car stops on its own within IDLE_T.

Endpoints (all return tiny JSON):
  POST /cmd   body: {"dir":"F|B|L|R|S", "speed":0.15..1.0}
  POST /speed body: {"speed":0.15..1.0}
  GET  /state  → current {dir, speed, moving}

Run on the Pi:
    /usr/bin/python3 control_server.py
Then open the printed URL on your phone (same Wi-Fi).
"""

import json
import os
import socket
import threading
import time
import urllib.request
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

# gpiozero is only present on the Pi. Import lazily so the file can still be
# served / inspected on a laptop (SIM mode just logs instead of driving GPIO).
try:
    from gpiozero import PWMOutputDevice, DigitalOutputDevice
    HAVE_GPIO = True
except Exception as _exc:  # noqa: BLE001 — any import problem → simulation mode
    HAVE_GPIO = False
    _GPIO_ERR = _exc

HERE = os.path.dirname(os.path.abspath(__file__))
PORT = 8001

# ───────────────────────── Speed / feel settings ─────────────────────────
# (Same numbers and meaning as keyboard_control.py so the car behaves identically.)
SPEED_DEFAULT = 0.22   # speed used until the phone sends another value
MIN_SPEED     = 0.15   # below this the motors won't actually spin
MAX_SPEED     = 1.00
KICK          = 0.70   # brief full-ish kick to break away from standstill
KICK_T        = 0.12   # kick duration (s)
IDLE_T        = 0.30   # stop the car if no command arrives for this long (watchdog)

# ───────────────────────── ADAS safety guard ─────────────────────────────
# The telemetry DashBoard (:8000) owns the live V2V state. We poll its /adas
# endpoint and, on a CRITICAL, refuse the dangerous move so the car can't drive
# into the hazard the firmware already flagged:
#   • FCW critical            → block FORWARD ("F")
#   • BSW critical on a side  → block the TURN into that side ("L" / "R")
# The poll is best-effort: if the dashboard is down or slow we fail OPEN (no
# restriction) so ordinary driving never depends on it.
ADAS_URL      = "http://127.0.0.1:8000/adas"
ADAS_POLL_S   = 0.15   # how often to refresh the ADAS snapshot
ADAS_TIMEOUT  = 0.3    # per-request timeout (short — never stall a drive command)
SYS_CRITICAL  = 2      # matches the firmware RiskLevel_t / sys_flags encoding


# ───────────────────────── Motor layer (L298N) ───────────────────────────
# Wraps the GPIO so the HTTP handler never touches pins directly. In SIM mode
# (no gpiozero) every call is a no-op except for a one-line print, so the whole
# server is testable on a laptop.
class Motors:
    def __init__(self):
        self._lock = threading.Lock()
        self._moving = False
        if HAVE_GPIO:
            # Motor A
            self.ENA = PWMOutputDevice(18)
            self.IN1 = DigitalOutputDevice(23)
            self.IN2 = DigitalOutputDevice(24)
            # Motor B
            self.ENB = PWMOutputDevice(19)
            self.IN3 = DigitalOutputDevice(27)
            self.IN4 = DigitalOutputDevice(22)
        else:
            print(f"[motors] SIM MODE (gpiozero unavailable: {_GPIO_ERR})")

    # --- low-level: drive one motor (matches keyboard_control.drive_a/b) ---
    def _drive_a(self, speed, forward=True):
        if not HAVE_GPIO:
            return
        if speed == 0:
            self.ENA.off(); self.IN1.off(); self.IN2.off(); return
        self.ENA.value = speed
        self.IN1.value = forward
        self.IN2.value = not forward

    def _drive_b(self, speed, forward=True):
        if not HAVE_GPIO:
            return
        if speed == 0:
            self.ENB.off(); self.IN3.off(); self.IN4.off(); return
        self.ENB.value = speed
        self.IN3.value = not forward
        self.IN4.value = forward

    def _kick(self, a_fwd, b_fwd):
        """Short startup kick, only when the car was standing still."""
        if not self._moving:
            self._drive_a(KICK, a_fwd); self._drive_b(KICK, b_fwd)
            time.sleep(KICK_T)
            self._moving = True

    # --- high-level moves (same geometry as the keyboard remote) ---
    def apply(self, direction, speed):
        """direction: 'F' forward, 'B' back, 'L' left, 'R' right, 'S' stop."""
        with self._lock:
            if direction == "S":
                self._drive_a(0); self._drive_b(0); self._moving = False
                return
            # spin-in-place turns: one side forward, the other backward
            if direction == "F":
                self._kick(True, True);   a = (speed, True);  b = (speed, True)
            elif direction == "B":
                self._kick(False, False); a = (speed, False); b = (speed, False)
            elif direction == "R":
                self._kick(False, True);  a = (speed, False); b = (speed, True)
            elif direction == "L":
                self._kick(True, False);  a = (speed, True);  b = (speed, False)
            else:
                return
            self._drive_a(*a); self._drive_b(*b)

    def stop(self):
        with self._lock:
            self._drive_a(0); self._drive_b(0); self._moving = False

    @property
    def moving(self):
        return self._moving


# ───────────────────────── Shared command state ──────────────────────────
# The HTTP handler writes the latest command here; the watchdog reads it.
_state_lock = threading.Lock()
_state = {"dir": "S", "speed": SPEED_DEFAULT, "last": 0.0}

motors = Motors()


# ───────────────────────── ADAS snapshot (from :8000) ─────────────────────
# Latest ADAS state polled from the dashboard. Empty until the first successful
# poll (and reset to empty on failure) → fail OPEN: no critical means no block.
_adas_lock = threading.Lock()
_adas: dict = {}


def adas_poll_loop():
    """Refresh the ADAS snapshot from the dashboard every ADAS_POLL_S. Best-effort:
    any error (dashboard down, timeout, bad JSON) clears the snapshot so we fall
    back to unrestricted driving instead of getting stuck on a stale CRITICAL."""
    while True:
        try:
            with urllib.request.urlopen(ADAS_URL, timeout=ADAS_TIMEOUT) as r:
                data = json.loads(r.read() or b"{}")
            snap = data if isinstance(data, dict) else {}
        except Exception:
            snap = {}            # fail open — never let a dead link block driving
        with _adas_lock:
            _adas.clear()
            _adas.update(snap)
        time.sleep(ADAS_POLL_S)


def blocked_reason(direction):
    """Return a short reason string if the ADAS state forbids *direction* right
    now, else None. FCW critical blocks forward; BSW critical blocks the turn
    into the flagged blind-spot side."""
    with _adas_lock:
        fcw  = _adas.get("fcw", 0)
        bsw  = _adas.get("bsw", 0)
        side = _adas.get("bswSide")
    if direction == "F" and fcw == SYS_CRITICAL:
        return "FCW"
    if direction == "L" and bsw == SYS_CRITICAL and side in ("left", "both"):
        return "BSW-LEFT"
    if direction == "R" and bsw == SYS_CRITICAL and side in ("right", "both"):
        return "BSW-RIGHT"
    return None


def _clamp_speed(v):
    try:
        v = float(v)
    except (TypeError, ValueError):
        return SPEED_DEFAULT
    return round(min(MAX_SPEED, max(MIN_SPEED, v)), 2)


def set_command(direction, speed=None):
    """Update the active command and drive the motors immediately. If the ADAS
    state forbids the requested direction (FCW/BSW critical), it is downgraded to
    a STOP before it ever reaches the motors. Returns the block reason (or None)."""
    direction = direction if direction in ("F", "B", "L", "R", "S") else "S"

    # ADAS override: a critical hazard turns the dangerous move into a stop so
    # the car physically can't drive into it, no matter what the phone sends.
    blocked = blocked_reason(direction)
    if blocked:
        direction = "S"

    with _state_lock:
        if speed is not None:
            _state["speed"] = _clamp_speed(speed)
        _state["dir"] = direction
        _state["last"] = time.time()
        d, s = _state["dir"], _state["speed"]
    motors.apply(d, s)
    return blocked


def watchdog_loop():
    """Stop the car if no fresh command has arrived within IDLE_T — this is what
    makes "release the arrow → stop" work, and what saves the car if the phone
    disconnects mid-drive. Runs forever in a daemon thread."""
    while True:
        time.sleep(IDLE_T / 3.0)
        with _state_lock:
            stale = (_state["dir"] != "S"
                     and time.time() - _state["last"] > IDLE_T)
            if stale:
                _state["dir"] = "S"
        if stale:
            motors.stop()


# ───────────────────────── HTTP handler ──────────────────────────────────
class Handler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass  # quiet

    def handle(self):
        # The phone opens/closes lots of short-lived connections (every held-arrow
        # POST, tab switches, Wi-Fi blips). When it drops one mid-request the kernel
        # raises ConnectionResetError/BrokenPipeError — harmless, but socketserver
        # would dump a full traceback for each. Swallow them so the console stays
        # readable; anything else still propagates.
        try:
            super().handle()
        except (ConnectionResetError, BrokenPipeError, ConnectionAbortedError):
            pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def _send_json(self, obj, code=200):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _read_json(self):
        try:
            n = int(self.headers.get("Content-Length", 0))
            return json.loads(self.rfile.read(n) or b"{}")
        except (ValueError, TypeError):
            return {}

    def do_GET(self):
        if self.path.split("?")[0] == "/state":
            with _state_lock:
                self._send_json({"dir": _state["dir"], "speed": _state["speed"],
                                 "moving": motors.moving})
            return
        # everything else = static files (index.html, css/, js/)
        super().do_GET()

    def do_POST(self):
        path = self.path.split("?")[0]
        data = self._read_json()
        if path == "/cmd":
            blocked = set_command(str(data.get("dir", "S")), data.get("speed"))
            with _state_lock:
                self._send_json({"ok": True, "dir": _state["dir"],
                                 "speed": _state["speed"], "blocked": blocked})
        elif path == "/speed":
            set_command(_state["dir"], data.get("speed"))
            with _state_lock:
                self._send_json({"ok": True, "speed": _state["speed"]})
        else:
            self._send_json({"ok": False, "error": "unknown endpoint"}, 404)


def lan_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"


def main():
    threading.Thread(target=watchdog_loop, daemon=True).start()
    threading.Thread(target=adas_poll_loop, daemon=True).start()  # ADAS safety guard

    handler = partial(Handler, directory=HERE)
    server = ThreadingHTTPServer(("0.0.0.0", PORT), handler)

    mode = "LIVE (GPIO)" if HAVE_GPIO else "SIM (no GPIO — laptop test)"
    print(f"V2X Car control remote — {mode}")
    print(f"  local  : http://localhost:{PORT}")
    print(f"  phone  : http://{lan_ip()}:{PORT}   (same Wi-Fi)")
    print("Hold an arrow to drive; release to stop. Ctrl+C to quit.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        motors.stop()
        print("\nstopped. bye.")


if __name__ == "__main__":
    main()
