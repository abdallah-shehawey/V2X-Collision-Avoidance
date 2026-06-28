import sys
import tty
import termios
import select
from time import time, sleep
from gpiozero import PWMOutputDevice, DigitalOutputDevice

# =============================================================
#  Drive the car with the keyboard arrows over SSH.
#  Reads the arrows directly from the terminal (no extra libraries).
#
#   Arrow Up    = forward
#   Arrow Down  = backward
#   Arrow Right = turn right
#   Arrow Left  = turn left
#   space       = stop
#   q           = quit
#
#  Run it from the laptop after you SSH in:
#       /usr/bin/python3 keyboard_control.py
# =============================================================

# Motor A
ENA = PWMOutputDevice(18)
IN1 = DigitalOutputDevice(23)
IN2 = DigitalOutputDevice(24)

# Motor B
ENB = PWMOutputDevice(19)
IN3 = DigitalOutputDevice(27)
IN4 = DigitalOutputDevice(22)

# ----------------- Speed settings -----------------
SPEED     = 0.22   # current speed (changed with r / e) — starts at the slowest
STEP      = 0.10   # increment/decrement per r or e press
MIN_SPEED = 0.15   # lowest allowed speed (below this the motor won't spin)
MAX_SPEED = 1.00   # maximum speed
KICK      = 0.70   # startup kick so the motors break away from standstill
KICK_T    = 0.12   # kick duration in seconds
IDLE_T    = 0.12   # the car stops this long after you release the key (real-time)


def drive_a(speed, forward=True):
    """speed=0 stops. forward=False reverses the direction."""
    if speed == 0:
        ENA.off(); IN1.off(); IN2.off(); return
    ENA.value = speed
    IN1.value = forward
    IN2.value = not forward


def drive_b(speed, forward=True):
    if speed == 0:
        ENB.off(); IN3.off(); IN4.off(); return
    ENB.value = speed
    IN3.value = not forward
    IN4.value = forward


# Whether the car is currently moving — so we know if a startup kick is needed
_moving = False


def _kick(a_fwd, b_fwd):
    """Short startup kick, only if the car was standing still."""
    global _moving
    if not _moving:
        drive_a(KICK, a_fwd); drive_b(KICK, b_fwd)
        sleep(KICK_T)
        _moving = True


def stop():
    global _moving
    drive_a(0); drive_b(0)
    _moving = False


def forward():
    _kick(True, True)
    drive_a(SPEED, True);  drive_b(SPEED, True)


def backward():
    _kick(False, False)
    drive_a(SPEED, False); drive_b(SPEED, False)


def turn_right():
    # right wheel backward, left wheel forward = spin in place to the right
    _kick(False, True)
    drive_a(SPEED, False); drive_b(SPEED, True)


def turn_left():
    _kick(True, False)
    drive_a(SPEED, True);  drive_b(SPEED, False)


def change_speed(delta):
    """Increase/decrease the speed within the allowed bounds."""
    global SPEED
    SPEED = round(min(MAX_SPEED, max(MIN_SPEED, SPEED + delta)), 2)
    print(f"    speed = {SPEED:.2f}")


def read_key(timeout):
    """Read one key from stdin within `timeout`. Returns None if nothing arrived."""
    r, _, _ = select.select([sys.stdin], [], [], timeout)
    if not r:
        return None
    ch = sys.stdin.read(1)
    # arrow keys arrive as ESC [ A/B/C/D
    if ch == "\x1b":
        seq = sys.stdin.read(2)
        return {"[A": "UP", "[B": "DOWN", "[C": "RIGHT", "[D": "LEFT"}.get(seq)
    return ch


def main():
    print(">>> Keyboard control (SSH)")
    print("    arrows = move | space = stop | q = quit")
    print("    r = speed + | e = speed -")
    print(f"    speed = {SPEED:.2f}")

    # last movement function pressed — we re-apply it while the key is held
    moves = {"UP": forward, "DOWN": backward, "RIGHT": turn_right, "LEFT": turn_left}
    current = None        # last active direction
    last_press = 0
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        while True:
            key = read_key(IDLE_T)

            if key is None:
                # key released (auto-repeat stopped) — stop immediately (real-time)
                if current is not None and time() - last_press > IDLE_T:
                    stop()
                    current = None
                continue

            if key in moves:
                last_press = time()
                current = key
                moves[key]()          # apply the direction
            elif key in ("r", "R"):
                change_speed(+STEP)
                if current:            # apply the new speed immediately
                    moves[current]()
            elif key in ("e", "E"):
                change_speed(-STEP)
                if current:
                    moves[current]()
            elif key == " ":
                stop(); current = None; print("    stop")
            elif key in ("q", "Q"):
                break

    except KeyboardInterrupt:
        pass
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        stop()
        print("\n    Stopped")


if __name__ == "__main__":
    main()
