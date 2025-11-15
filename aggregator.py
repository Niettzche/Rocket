from __future__ import annotations

import math
import queue
import sys
import threading
import time
from typing import Any, Callable, Dict, Iterable, List, Optional

from logger import log, log_payload
from lora_transport import has_link_failure
from sensor_messages import SensorMessage, build_payload
try:
    from zero_accel_gpio import activate as gpio_activate
    from zero_accel_gpio import cleanup as gpio_cleanup
except Exception:
    def gpio_activate() -> bool:
        log("MPU6050", "GPIO no disponible, omitiendo activación física uwu", "WARN")
        return False

    def gpio_cleanup() -> None:
        log("MPU6050", "GPIO no disponible, nada que limpiar uwu", "DEBUG")

ZERO_ACCEL_REF = 1.0
ZERO_ACCEL_TOLERANCE = 0.05
ZERO_ACCEL_REQUIRED = 2
ZERO_ACCEL_MIN_DELAY = 1.0

class ActivityTracker:
    def __init__(self, sensors: Iterable[str]):
        self._state: Dict[str, Dict[str, bool]] = {
            sensor: {"seen": False, "last_dummy": True} for sensor in sensors
        }
        self._zero_acc_signal: Dict[str, Optional[float]] = {
            "timestamp": None,
            "magnitude": None,
        }
        self._zero_acc_sent = False

    def update(self, sensor: str, is_dummy: bool) -> None:
        if sensor not in self._state:
            return
        self._state[sensor]["seen"] = True
        self._state[sensor]["last_dummy"] = is_dummy

    def groups(self) -> Dict[str, List[str]]:
        reales: List[str] = []
        dummy: List[str] = []
        sin_datos: List[str] = []
        for sensor, state in self._state.items():
            if not state["seen"]:
                sin_datos.append(sensor)
            elif state["last_dummy"]:
                dummy.append(sensor)
            else:
                reales.append(sensor)
        return {"reales": reales, "dummy": dummy, "sin_datos": sin_datos}

    def sensors(self) -> List[str]:
        return list(self._state.keys())

    def zero_accel_signal_sent(self) -> bool:
        return self._zero_acc_sent

    def record_zero_accel_signal(self, timestamp: float, magnitude: float) -> None:
        if self._zero_acc_sent:
            return
        self._zero_acc_signal["timestamp"] = timestamp
        self._zero_acc_signal["magnitude"] = magnitude
        self._zero_acc_sent = True

    def zero_accel_signal_details(self) -> Dict[str, Optional[float]]:
        return {
            "sent": self._zero_acc_sent,
            "timestamp": self._zero_acc_signal["timestamp"],
            "magnitude": self._zero_acc_signal["magnitude"],
        }

    # Hint left to help future readers: movement tracking was dropped in
    # favour of a simpler zero-acceleration signal, so there is intentionally
    # no extra state beyond bookkeeping above.

def aggregator_loop(
    inbox: queue.Queue[SensorMessage],
    stop_event: threading.Event,
    expected_sensors: Iterable[str],
    tracker: ActivityTracker,
    send_payload: Callable[[Dict[str, Any]], None],
    emit_every: float = 0.5,
) -> None:
    latest: Dict[str, SensorMessage] = {}
    expected = list(expected_sensors)
    last_emit = 0.0
    zero_acc_count = 0
    zero_acc_last_detection = 0.0
    warned_link_failure = False
    try:
        while not stop_event.is_set():
            try:
                message = inbox.get(timeout=0.2)
            except queue.Empty:
                continue
            tracker.update(message.sensor, bool(message.data.get("dummy", False)))
            magnitude: Optional[float] = None
            if message.sensor == "mpu6050":
                accel = message.data.get("accel_g")
                if isinstance(accel, dict):
                    ax_raw = accel.get("ax", accel.get("x", 0.0))
                    ay_raw = accel.get("ay", accel.get("y", 0.0))
                    az_raw = accel.get("az", accel.get("z", 0.0))
                    try:
                        ax = float(ax_raw)
                        ay = float(ay_raw)
                        az = float(az_raw)
                    except (TypeError, ValueError):
                        ax = ay = az = 0.0
                    magnitude = math.sqrt(ax * ax + ay * ay + az * az)
            if (
                message.sensor == "mpu6050"
                and magnitude is not None
                and not tracker.zero_accel_signal_sent()
                and not message.data.get("dummy", False)
            ):
                if abs(magnitude - ZERO_ACCEL_REF) <= ZERO_ACCEL_TOLERANCE:
                    if message.timestamp - zero_acc_last_detection > ZERO_ACCEL_MIN_DELAY:
                        zero_acc_count += 1
                        zero_acc_last_detection = message.timestamp
                        log(
                            "MPU6050",
                            f"Detección {zero_acc_count}: sin aceleración lineal (|a|={magnitude:.3f}g)",
                            "INFO",
                        )
                        if zero_acc_count >= ZERO_ACCEL_REQUIRED:
                            tracker.record_zero_accel_signal(message.timestamp, magnitude)
                            if gpio_activate():
                                log("MPU6050", "GPIO 26 activado uwu", "WARN")
                            log(
                                "MPU6050",
                                "Señal registrada por aceleración cero",
                                "WARN",
                            )
            latest[message.sensor] = message
            now = time.time()
            if now - last_emit < emit_every:
                continue
            payload = build_payload(latest, expected, now)
            log_payload(payload)
            if has_link_failure():
                if not warned_link_failure:
                    log(
                        "LORA",
                        "LoRa sin respuesta: omito envíos y sólo imprimiré lecturas",
                        "ERROR",
                        sys.stderr,
                    )
                    warned_link_failure = True
                continue
            warned_link_failure = False
            try:
                send_payload(payload)
            except Exception as exc:
                log("LORA", f"error inesperado al enviar uwu: {exc}", "ERROR", sys.stderr)
            last_emit = now
    finally:
        gpio_cleanup()

def create_aggregator_thread(
    inbox: queue.Queue[SensorMessage],
    stop_event: threading.Event,
    expected_sensors: Iterable[str],
    tracker: ActivityTracker,
    send_payload: Callable[[Dict[str, Any]], None],
    emit_every: float = 0.5,
) -> threading.Thread:
    return threading.Thread(
        target=aggregator_loop,
        args=(inbox, stop_event, expected_sensors, tracker, send_payload, emit_every),
        name="Agregador",
        daemon=True,
    )
