#!/usr/bin/env python3
from __future__ import annotations

import queue
import signal
import threading
from typing import List

from aggregator import ActivityTracker, create_aggregator_thread
from logger import log
from lora_transport import (
    MODE_RX,
    MODE_TX,
    configure_from_config,
    is_ready,
    receive_loop,
    send_to_lora,
    record_init_error,
)
from sensor_messages import SensorMessage
from sensor_workers import SENSORS, sensor_threads
from summaries import log_final_summary, log_start_summary

def _run_transmitter(stop_event: threading.Event) -> None:
    log("SYSTEM", "Arrancando el agregador bonito uwu", "SYS")
    log_start_summary()
    inbox: queue.Queue[SensorMessage] = queue.Queue()
    tracker = ActivityTracker(SENSORS)
    sensor_thread_list = list(sensor_threads(inbox, stop_event))
    aggregator_thread = create_aggregator_thread(inbox, stop_event, SENSORS, tracker, send_to_lora)
    workers: List[threading.Thread] = sensor_thread_list + [aggregator_thread]

    for worker in workers:
        worker.start()
        log("SYSTEM", f"Hilo {worker.name} arriba uwu", "SYS")

    try:
        while any(worker.is_alive() for worker in sensor_thread_list):
            if stop_event.wait(0.2):
                break
    finally:
        stop_event.set()
        log("SYSTEM", "Esperando a que todos terminen uwu", "SYS")
        for worker in workers:
            if worker.is_alive():
                try:
                    worker.join(timeout=1.0)
                except Exception:
                    pass
        log_final_summary(tracker)
        log("SYSTEM", "Agregador apagado uwu", "SYS")


def _run_receiver(stop_event: threading.Event) -> None:
    log("SYSTEM", "Arrancando receptor LoRa uwu", "SYS")
    try:
        receive_loop(stop_event=stop_event)
    finally:
        stop_event.set()
        log("SYSTEM", "Receptor apagado uwu", "SYS")


def run() -> None:
    log("SYSTEM", "Preparando LoRa según configuración uwu", "SYS")
    try:
        mode = configure_from_config()
    except Exception as exc:
        log("SYSTEM", f"Fallo al configurar LoRa, continuo sin radio: {exc}", "ERROR")
        record_init_error(f"configuración fallida: {exc}")
        mode = MODE_TX
    log("SYSTEM", f"LoRa tras init ({mode.upper()}): {'LISTO' if is_ready() else 'NO LISTO'}", "SYS")
    stop_event = threading.Event()

    def handle_signal(_sig, _frame) -> None:
        log("SYSTEM", "Nos pidieron parar uwu", "SYS")
        stop_event.set()

    try:
        signal.signal(signal.SIGINT, handle_signal)
        signal.signal(signal.SIGTERM, handle_signal)
    except Exception:
        pass

    if mode == MODE_RX:
        _run_receiver(stop_event)
    else:
        _run_transmitter(stop_event)

if __name__ == "__main__":
    run()
