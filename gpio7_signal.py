"""Simple script to pulse GPIO7 high for 1 second."""

import time

import RPi.GPIO as GPIO

PIN = 7  # BCM numbering (GPIO7)


def send_pulse(duration: float = 1.0) -> None:
    """Drive GPIO7 high for `duration` seconds then return it low."""
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(PIN, GPIO.OUT, initial=GPIO.LOW)

    try:
        GPIO.output(PIN, GPIO.HIGH)
        time.sleep(duration)
        GPIO.output(PIN, GPIO.LOW)
    finally:
        GPIO.cleanup(PIN)


if __name__ == "__main__":
    send_pulse()
