import sys
import tty
import termios
import select
from time import time, sleep
from gpiozero import PWMOutputDevice, DigitalOutputDevice

# =============================================================
#  تحكم في العربية بأسهم الكيبورد عن طريق SSH.
#  بيقرأ الأسهم مباشرة من الـ terminal (من غير مكتبات إضافية).
#
#   السهم فوق   = قدّام
#   السهم تحت   = ورا
#   السهم يمين  = لفّة يمين
#   السهم شمال  = لفّة شمال
#   space       = وقوف
#   q           = خروج
#
#  شغّله من اللاب بعد ما تعمل SSH:
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

# ----------------- إعدادات السرعة -----------------
SPEED     = 0.22   # السرعة الحالية (بتتغير بـ r / e) — تبدأ أبطأ حاجة
STEP      = 0.10   # مقدار الزيادة/النقصان لكل ضغطة r أو e
MIN_SPEED = 0.15   # أقل سرعة مسموحة (تحت كده المحرك مش هيلف)
MAX_SPEED = 1.00   # أقصى سرعة
KICK      = 0.70   # دفعة بداية عشان المحركات تبدأ من السكون
KICK_T    = 0.12   # مدة دفعة البداية بالثواني
IDLE_T    = 0.12   # أول ما تشيل إيدك بمدة دي العربية بتقف (real-time)


def drive_a(speed, forward=True):
    """speed=0 يوقّف. forward=False يعكس الاتجاه."""
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


# آخر اتجاه كانت ماشية فيه العربية — عشان نعرف نحتاج دفعة بداية ولا لأ
_moving = False


def _kick(a_fwd, b_fwd):
    """دفعة بداية قصيرة بس لو العربية كانت واقفة."""
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
    # العجلة اليمين ورا, الشمال قدّام = لفّة في المكان لليمين
    _kick(False, True)
    drive_a(SPEED, False); drive_b(SPEED, True)


def turn_left():
    _kick(True, False)
    drive_a(SPEED, True);  drive_b(SPEED, False)


def change_speed(delta):
    """يزوّد/يقلل السرعة في الحدود المسموحة."""
    global SPEED
    SPEED = round(min(MAX_SPEED, max(MIN_SPEED, SPEED + delta)), 2)
    print(f"    speed = {SPEED:.2f}")


def read_key(timeout):
    """يقرأ مفتاح واحد من الـ stdin خلال مدة timeout. يرجّع None لو مفيش."""
    r, _, _ = select.select([sys.stdin], [], [], timeout)
    if not r:
        return None
    ch = sys.stdin.read(1)
    # الأسهم بتيجي على شكل ESC [ A/B/C/D
    if ch == "\x1b":
        seq = sys.stdin.read(2)
        return {"[A": "UP", "[B": "DOWN", "[C": "RIGHT", "[D": "LEFT"}.get(seq)
    return ch


def main():
    print(">>> Keyboard control (SSH)")
    print("    الأسهم = حركة | space = وقوف | q = خروج")
    print("    r = سرعة + | e = سرعة -")
    print(f"    speed = {SPEED:.2f}")

    # آخر دالة حركة مدوس عليها — بنعيد تطبيقها طول ما المفتاح ماسك
    moves = {"UP": forward, "DOWN": backward, "RIGHT": turn_right, "LEFT": turn_left}
    current = None        # آخر اتجاه شغّال
    last_press = 0
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        while True:
            key = read_key(IDLE_T)

            if key is None:
                # شِلت إيدك (وقف التكرار) — تقف فورًا (real-time)
                if current is not None and time() - last_press > IDLE_T:
                    stop()
                    current = None
                continue

            if key in moves:
                last_press = time()
                current = key
                moves[key]()          # ادوس على الاتجاه
            elif key in ("r", "R"):
                change_speed(+STEP)
                if current:            # طبّق السرعة الجديدة فورًا
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
