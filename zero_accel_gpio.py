"""GPIO helper for zero-acceleration signal."""
from __future__ import annotations

from logger import log

GPIO_PINS = (12, 7)

try:
    import RPi.GPIO as GPIO  # type: ignore
except Exception:  # pragma: no cover - hardware import guard
    GPIO = None  # type: ignore

_gpio_ready = False


def _ensure_setup() -> bool:
    global _gpio_ready
    if GPIO is None:
        return False
    if _gpio_ready:
        return True
    try:
        GPIO.setmode(GPIO.BCM)
        GPIO.setwarnings(False)
        for pin in GPIO_PINS:
            GPIO.setup(pin, GPIO.OUT, initial=GPIO.LOW)  # type: ignore[attr-defined]
    except Exception as exc:
        log("GPIO", f"no pude preparar los pines uwu: {exc}", "ERROR")
        return False
    _gpio_ready = True
    return True


def activate() -> bool:
    if not _ensure_setup():
        log("GPIO", "RPi.GPIO no disponible, no hay señal física uwu", "WARN")
        return False
    try:
        for pin in GPIO_PINS:
            GPIO.output(pin, GPIO.HIGH)  # type: ignore[attr-defined]
    except Exception as exc:
        log("GPIO", f"falló al subir pines uwu: {exc}", "ERROR")
        return False
    return True


def cleanup() -> None:
    global _gpio_ready
    if GPIO is None or not _gpio_ready:
        return
    try:
        for pin in GPIO_PINS:
            GPIO.output(pin, GPIO.LOW)  # type: ignore[attr-defined]
        GPIO.cleanup()  # type: ignore[attr-defined]
    except Exception as exc:
        log("GPIO", f"falló al limpiar pines uwu: {exc}", "WARN")
    finally:
        _gpio_ready = False
