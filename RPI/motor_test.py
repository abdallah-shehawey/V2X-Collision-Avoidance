from gpiozero import PWMOutputDevice, DigitalOutputDevice
from time import sleep

# =============================================================
#  حركة ثعبانية (Snake) بأبطأ سرعة ممكنة.
#  العربية بتمشي قدّام وتلف يمين/شمال بالتبادل عن طريق
#  إن عجلة تبطّأ والتانية تسرّع، فتعمل تمايل زي الثعبان.
#  شغّله:  /usr/bin/python3 motor_test.py
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
BASE   = 0.30   # أبطأ سرعة ثابتة بعد ما العربية تتحرك (عدّلها لو وقفت)
SLOW   = 0.18   # سرعة العجلة البطيئة وقت اللفّة
KICK   = 0.70   # دفعة بداية عشان المحركات تبدأ تلف (تتغلب على الاحتكاك)
KICK_T = 0.25   # مدة دفعة البداية بالثواني
TURN_T = 0.8    # مدة كل لفّة (يمين/شمال) بالثواني


def motor_a(on, speed=0.9):
    if on:
        ENA.value = speed
        IN1.on()
        IN2.off()
    else:
        ENA.off()
        IN1.off()
        IN2.off()


def motor_b(on, speed=0.9):
    if on:
        ENB.value = speed
        IN3.off()
        IN4.on()
    else:
        ENB.off()
        IN3.off()
        IN4.off()


def kickstart():
    """دفعة بداية قصيرة عشان المحركات تبدأ من السكون."""
    motor_a(True, KICK)
    motor_b(True, KICK)
    sleep(KICK_T)


try:
    print(">>> Snake mode - أبطأ سرعة + تمايل ثعباني")
    kickstart()

    while True:
        # لفّة لليمين: العجلة اليمين تبطّأ
        print("    turn right")
        motor_a(True, BASE)
        motor_b(True, SLOW)
        sleep(TURN_T)

        # لفّة للشمال: العجلة الشمال تبطّأ
        print("    turn left")
        motor_a(True, SLOW)
        motor_b(True, BASE)
        sleep(TURN_T)

except KeyboardInterrupt:
    pass

finally:
    motor_a(False)
    motor_b(False)
    print("\n    Stopped")
