from __future__ import annotations

import math
import queue
import sys
import threading
import time
from typing import Dict, Tuple

from logger import log
from sensor_messages import SensorMessage, isoformat_utc

HAS_MPU = True
try:
    import acceleration as mpu
except Exception:
    HAS_MPU = False
    mpu = None

HAS_BMP = True
try:
    import BMP180
except Exception:
    HAS_BMP = False
    BMP180 = None

HAS_GPS = True
try:
    import neo3
except Exception:
    HAS_GPS = False
    neo3 = None

SENSORS: Tuple[str, ...] = ("mpu6050", "bmp180", "neo6m")
CAPS: Dict[str, bool] = {
    "mpu6050": HAS_MPU,
    "bmp180": HAS_BMP,
    "neo6m": HAS_GPS,
}

def mpu6050_worker(outbox: queue.Queue[SensorMessage], stop_event: threading.Event) -> None:
    if not HAS_MPU:
        log("MPU6050", "sin sensor, usando datos dummy uwu", "WARN")
        phase = 0.0
        while not stop_event.is_set():
            now = time.time()
            ax = 0.01 * math.sin(phase)
            ay = 0.01 * math.cos(phase)
            az = 1.0
            gx = 0.1 * math.sin(phase)
            gy = 0.1 * math.cos(phase)
            gz = 0.0
            phase += 0.05
            outbox.put(
                SensorMessage(
                    sensor="mpu6050",
                    timestamp=now,
                    data={
                        "accel_g": {"x": round(ax, 4), "y": round(ay, 4), "z": round(az, 4)},
                        "gyro_dps": {"x": round(gx, 3), "y": round(gy, 3), "z": round(gz, 3)},
                        "attitude_deg": {"pitch": 0.0, "roll": 0.0, "yaw": 0.0},
                        "dummy": True,
                    },
                )
            )
            if stop_event.wait(0.05):
                break
        return
    log("MPU6050", "Calibrando el sensor uwu")
    try:
        offsets = mpu.calibrate_sensor()
    except Exception as exc:
        log("MPU6050", f"ups, falló la calibración uwu: {exc}", "ERROR", sys.stderr)
        return
    state = {"pitch": 0.0, "roll": 0.0, "yaw": 0.0, "pitch_smooth": 0.0, "roll_smooth": 0.0}
    alpha_filter = {axis: 0.0 for axis in ("ax", "ay", "az", "gx", "gy", "gz")}
    last_time = time.time()
    dt_sleep = getattr(mpu, "DT_SLEEP", 0.05)
    log("MPU6050", "Arrancó el bucle de captura uwu", "DEBUG")
    while not stop_event.is_set():
        now = time.time()
        dt = max(now - last_time, 1e-3)
        last_time = now
        try:
            ax, ay, az, gx, gy, gz = mpu.read_accel_gyro(offsets)
        except Exception as exc:
            log("MPU6050", f"ouch, no pude leer uwu: {exc}", "ERROR", sys.stderr)
            break
        alpha = getattr(mpu, "ALPHA", 0.3)
        for axis, value in zip(alpha_filter.keys(), (ax, ay, az, gx, gy, gz)):
            alpha_filter[axis] = alpha * value + (1 - alpha) * alpha_filter[axis]
        try:
            state = mpu.complementary_filter(
                alpha_filter["ax"],
                alpha_filter["ay"],
                alpha_filter["az"],
                alpha_filter["gx"],
                alpha_filter["gy"],
                alpha_filter["gz"],
                dt,
                state,
            )
        except Exception as exc:
            log("MPU6050", f"filtro complementario falló uwu: {exc}", "WARN")
        outbox.put(
            SensorMessage(
                sensor="mpu6050",
                timestamp=now,
                data={
                    "accel_g": {k: round(alpha_filter[k], 4) for k in ("ax", "ay", "az")},
                    "gyro_dps": {k: round(alpha_filter[k], 3) for k in ("gx", "gy", "gz")},
                    "attitude_deg": {
                        "pitch": round(state.get("pitch_smooth", 0.0), 2),
                        "roll": round(state.get("roll_smooth", 0.0), 2),
                        "yaw": round(state.get("yaw", 0.0), 2),
                    },
                },
            )
        )
        if stop_event.wait(dt_sleep):
            break
    log("MPU6050", "Bucle de captura detenido uwu", "DEBUG")

def bmp180_worker(outbox: queue.Queue[SensorMessage], stop_event: threading.Event) -> None:
    if not HAS_BMP:
        log("BMP180", "sin sensor, usando datos dummy uwu", "WARN")
        temp = 25.0
        pres = 1013.25
        while not stop_event.is_set():
            now = time.time()
            temp += 0.01
            pres += 0.02
            outbox.put(
                SensorMessage(
                    sensor="bmp180",
                    timestamp=now,
                    data={"raw": {"T": round(temp, 2), "P": round(pres, 2)}, "dummy": True},
                )
            )
            if stop_event.wait(0.2):
                break
        return
    log("BMP180", "Abriendo el puerto serie uwu", "DEBUG")
    try:
        conn = BMP180.open_connection()
    except Exception as exc:
        log("BMP180", f"no logré abrir el puerto uwu: {exc}", "ERROR", sys.stderr)
        return
    with conn:
        log("BMP180", "Escuchando lecturitas del Arduino uwu", "INFO")
        while not stop_event.is_set():
            try:
                sample = BMP180.read_measurement(conn)
            except Exception as exc:
                log("BMP180", f"ouch, error leyendo uwu: {exc}", "ERROR", sys.stderr)
                break
            if sample is None:
                continue
            timestamp, payload = sample
            log("BMP180", f"dato crudo uwu: {payload}", "DEBUG")
            outbox.put(SensorMessage(sensor="bmp180", timestamp=timestamp, data={"raw": payload}))
            if stop_event.wait(0.05):
                break
    log("BMP180", "Cerrando el puerto uwu", "DEBUG")

def neo6m_worker(outbox: queue.Queue[SensorMessage], stop_event: threading.Event) -> None:
    if not HAS_GPS:
        log("NEO6M", "sin GPS, usando datos dummy uwu", "WARN")
        lat = 25.651
        lon = -100.289
        alt = 512.0
        while not stop_event.is_set():
            now = time.time()
            lat += 1e-5
            lon -= 1e-5
            outbox.put(
                SensorMessage(
                    sensor="neo6m",
                    timestamp=now,
                    data={
                        "latitude": round(lat, 6),
                        "longitude": round(lon, 6),
                        "altitude": round(alt, 1),
                        "fix_time": isoformat_utc(now),
                        "raw": "$GPGGA,DUMMY",
                        "dummy": True,
                    },
                )
            )
            if stop_event.wait(0.5):
                break
        return
    log("NEO6M", "Intentando charlar con el GPS uwu", "DEBUG")
    try:
        conn = neo3.open_connection()
    except Exception as exc:
        log("NEO6M", f"el GPS no quiso uwu: {exc}", "ERROR", sys.stderr)
        return
    with conn:
        log("NEO6M", "Esperando sentencias NMEA uwu", "INFO")
        while not stop_event.is_set():
            try:
                fix = neo3.read_fix(conn)
            except Exception as exc:
                log("NEO6M", f"ouch, error leyendo gps uwu: {exc}", "ERROR", sys.stderr)
                break
            if fix is None:
                continue
            log(
                "NEO6M",
                f"lat={fix.latitude} lon={fix.longitude} alt={fix.altitude} hora={fix.fix_time} uwu",
                "DEBUG",
            )
            outbox.put(
                SensorMessage(
                    sensor="neo6m",
                    timestamp=time.time(),
                    data={
                        "latitude": fix.latitude,
                        "longitude": fix.longitude,
                        "altitude": fix.altitude,
                        "fix_time": fix.fix_time,
                        "raw": fix.raw_sentence,
                    },
                )
            )
            if stop_event.wait(0.1):
                break
    log("NEO6M", "Me despido del GPS uwu", "DEBUG")

def sensor_threads(
    inbox: queue.Queue[SensorMessage],
    stop_event: threading.Event,
) -> Tuple[threading.Thread, threading.Thread, threading.Thread]:
    return (
        threading.Thread(target=mpu6050_worker, args=(inbox, stop_event), name="MPU6050"),
        threading.Thread(target=bmp180_worker, args=(inbox, stop_event), name="BMP180"),
        threading.Thread(target=neo6m_worker, args=(inbox, stop_event), name="NEO6M"),
    )
