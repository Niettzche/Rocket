from __future__ import annotations

from pathlib import Path
import time
from typing import List, Tuple

from logger import log
from lora_transport import get_init_error, has_link_failure, is_ready
from sensor_workers import CAPS
from aggregator import ActivityTracker
from sensor_messages import isoformat_utc

_FINAL_LOG_FILE = Path(__file__).resolve().parent / "logs" / "resumen_final.log"

def log_start_summary() -> None:
    activos = [sensor for sensor, ok in CAPS.items() if ok]
    inactivos = [sensor for sensor, ok in CAPS.items() if not ok]
    log("SYSTEM", "===== RESUMEN INICIAL =====", "SYS")
    log("SYSTEM", f"Sensores disponibles: {', '.join(activos) if activos else 'ninguno'}", "INFO")
    log("SYSTEM", f"Sensores NO disponibles: {', '.join(inactivos) if inactivos else 'ninguno'}", "WARN")
    log("SYSTEM", f"LoRa: {'LISTO' if is_ready() else 'NO LISTO'}", "INFO")

def log_final_summary(tracker: ActivityTracker) -> None:
    records: List[Tuple[str, str]] = []

    def record(message: str, level: str) -> None:
        log("SYSTEM", message, level)
        records.append((level, message))

    header = "===== RESUMEN FINAL ====="
    log("SYSTEM", header, "SYS")
    grupos = tracker.groups()
    record(
        f"Datos REALES recibidos: {', '.join(grupos['reales']) if grupos['reales'] else 'ninguno'}",
        "INFO",
    )
    record(
        f"Datos DUMMY (sin hardware): {', '.join(grupos['dummy']) if grupos['dummy'] else 'ninguno'}",
        "WARN",
    )
    record(
        f"Sensores sin datos: {', '.join(grupos['sin_datos']) if grupos['sin_datos'] else 'ninguno'}",
        "ERROR",
    )
    zero_signal = tracker.zero_accel_signal_details()
    if zero_signal["sent"]:
        ts = zero_signal["timestamp"]
        ts_str = isoformat_utc(ts) if isinstance(ts, (int, float)) else "desconocido"
        magnitude = zero_signal["magnitude"]
        magnitude_str = f"{magnitude:.3f}g" if isinstance(magnitude, (int, float)) else "desconocido"
        record(f"Señal por aceleración cero: ENVIADA (t={ts_str}, |a|={magnitude_str})", "INFO")
    else:
        record("Señal por aceleración cero: NO ENVIADA", "WARN")
    lora_ready = is_ready()
    record(f"LoRa: {'LISTO' if lora_ready else 'NO LISTO'}", "INFO")
    if not lora_ready:
        error = get_init_error()
        detalle = error if error else "motivo desconocido"
        record(f"LoRa no se inició correctamente: {detalle}", "ERROR")
    if has_link_failure():
        record("LoRa no se conectó correctamente (módulo sin respuesta)", "ERROR")
    _persist_final_log(header, records)


def _persist_final_log(header: str, records: List[Tuple[str, str]]) -> None:
    timestamp = isoformat_utc(time.time())
    lines = [f"{header} ({timestamp})"]
    lines.extend(f"[{level}] {message}" for level, message in records)
    try:
        _FINAL_LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
        with _FINAL_LOG_FILE.open("a", encoding="utf-8") as fp:
            fp.write("\n".join(lines) + "\n\n")
        log("SYSTEM", f"Resumen final guardado en {_FINAL_LOG_FILE}", "SYS")
    except OSError as exc:
        log("SYSTEM", f"No pude guardar resumen final en {_FINAL_LOG_FILE}: {exc}", "ERROR")
