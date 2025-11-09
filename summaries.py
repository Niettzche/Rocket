from __future__ import annotations

from logger import log
from lora_transport import get_init_error, has_link_failure, is_ready
from sensor_workers import CAPS
from aggregator import ActivityTracker
from sensor_messages import isoformat_utc

def log_start_summary() -> None:
    activos = [sensor for sensor, ok in CAPS.items() if ok]
    inactivos = [sensor for sensor, ok in CAPS.items() if not ok]
    log("SYSTEM", "===== RESUMEN INICIAL =====", "SYS")
    log("SYSTEM", f"Sensores disponibles: {', '.join(activos) if activos else 'ninguno'}", "INFO")
    log("SYSTEM", f"Sensores NO disponibles: {', '.join(inactivos) if inactivos else 'ninguno'}", "WARN")
    log("SYSTEM", f"LoRa: {'LISTO' if is_ready() else 'NO LISTO'}", "INFO")

def log_final_summary(tracker: ActivityTracker) -> None:
    grupos = tracker.groups()
    log("SYSTEM", "===== RESUMEN FINAL =====", "SYS")
    log(
        "SYSTEM",
        f"Datos REALES recibidos: {', '.join(grupos['reales']) if grupos['reales'] else 'ninguno'}",
        "INFO",
    )
    log(
        "SYSTEM",
        f"Datos DUMMY (sin hardware): {', '.join(grupos['dummy']) if grupos['dummy'] else 'ninguno'}",
        "WARN",
    )
    log(
        "SYSTEM",
        f"Sensores sin datos: {', '.join(grupos['sin_datos']) if grupos['sin_datos'] else 'ninguno'}",
        "ERROR",
    )
    zero_signal = tracker.zero_accel_signal_details()
    if zero_signal["sent"]:
        ts = zero_signal["timestamp"]
        ts_str = isoformat_utc(ts) if isinstance(ts, (int, float)) else "desconocido"
        magnitude = zero_signal["magnitude"]
        magnitude_str = f"{magnitude:.3f}g" if isinstance(magnitude, (int, float)) else "desconocido"
        log(
            "SYSTEM",
            f"Señal por aceleración cero: ENVIADA (t={ts_str}, |a|={magnitude_str})",
            "INFO",
        )
    else:
        log("SYSTEM", "Señal por aceleración cero: NO ENVIADA", "WARN")
    lora_ready = is_ready()
    log("SYSTEM", f"LoRa: {'LISTO' if lora_ready else 'NO LISTO'}", "INFO")
    if not lora_ready:
        error = get_init_error()
        detalle = error if error else "motivo desconocido"
        log("SYSTEM", f"LoRa no se inició correctamente: {detalle}", "ERROR")
    if has_link_failure():
        log("SYSTEM", "LoRa no se conectó correctamente (módulo sin respuesta)", "ERROR")
