from __future__ import annotations

import threading
from typing import Iterable, List

try:
    import RPi.GPIO as GPIO
except Exception as exc:  # pragma: no cover - solo se ejecuta fuera de la Pi
    raise RuntimeError("RPi.GPIO no disponible") from exc


_LOCK = threading.Lock()
_INITIALIZED = False
_PIN_STATE = False
GPIO_PINS: List[int] = [26]


def _ensure_setup() -> bool:
    global _INITIALIZED
    if _INITIALIZED:
        return True
    with _LOCK:
        if _INITIALIZED:
            return True
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        for pin in GPIO_PINS:
            GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)
        _INITIALIZED = True
    return True


def _drive_pins(level: bool, pins: Iterable[int]) -> None:
    for pin in pins:
        GPIO.output(pin, GPIO.HIGH if level else GPIO.LOW)


def activate() -> bool:
    """Enciende los GPIO definidos (BCM)."""
    global _PIN_STATE
    if not _ensure_setup():
        return False
    _drive_pins(True, GPIO_PINS)
    _PIN_STATE = True
    return True


def cleanup() -> None:
    """Devuelve los GPIO a LOW y limpia el estado de RPi.GPIO."""
    global _INITIALIZED, _PIN_STATE
    if not _INITIALIZED:
        return
    if _PIN_STATE:
        _drive_pins(False, GPIO_PINS)
        _PIN_STATE = False
    GPIO.cleanup(GPIO_PINS)
    _INITIALIZED = False
