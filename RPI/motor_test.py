from gpiozero import PWMOutputDevice, DigitalOutputDevice
from time import sleep

# =============================================================
#  Snake motion at the slowest possible speed.
#  The car moves forward and weaves right/left alternately by
#  slowing one wheel while speeding up the other, so it sways
#  like a snake.
#  Run it:  /usr/bin/python3 motor_test.py
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
BASE   = 0.30   # slowest steady speed once the car is moving (tune up if it stalls)
SLOW   = 0.18   # speed of the slowed wheel during a turn
KICK   = 0.70   # startup kick so the motors start spinning (overcome friction)
KICK_T = 0.25   # kick duration in seconds
TURN_T = 0.8    # duration of each turn (right/left) in seconds


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
    """Short startup kick so the motors break away from standstill."""
    motor_a(True, KICK)
    motor_b(True, KICK)
    sleep(KICK_T)


try:
    print(">>> Snake mode - slowest speed + snake-like weaving")
    kickstart()

    while True:
        # turn right: slow down the right wheel
        print("    turn right")
        motor_a(True, BASE)
        motor_b(True, SLOW)
        sleep(TURN_T)

        # turn left: slow down the left wheel
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
