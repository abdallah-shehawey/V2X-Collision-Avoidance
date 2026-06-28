from gpiozero import PWMOutputDevice, DigitalOutputDevice
from time import sleep

# =============================================================
#  L298N Motor Driver on Raspberry Pi
#  Motor A:  ENA=GPIO18, IN1=GPIO23, IN2=GPIO24
#  Motor B:  ENB=GPIO19, IN3=GPIO27, IN4=GPIO22
#  Run it with the system python:  /usr/bin/python3 car.py
# =============================================================

# Motor A
ENA = PWMOutputDevice(18)
IN1 = DigitalOutputDevice(23)
IN2 = DigitalOutputDevice(24)

# Motor B
ENB = PWMOutputDevice(19)
IN3 = DigitalOutputDevice(27)
IN4 = DigitalOutputDevice(22)


def forward(speed=0.8):
    ENA.value = speed
    ENB.value = speed
    IN1.on()
    IN2.off()
    IN3.on()
    IN4.off()


def backward(speed=0.8):
    ENA.value = speed
    ENB.value = speed
    IN1.off()
    IN2.on()
    IN3.off()
    IN4.on()


def stop():
    ENA.off()
    ENB.off()
    IN1.off()
    IN2.off()
    IN3.off()
    IN4.off()


try:
    print("Forward")
    forward(0.8)
    sleep(2)

    print("Stop")
    stop()

except KeyboardInterrupt:
    stop()
