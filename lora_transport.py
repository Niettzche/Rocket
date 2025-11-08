from __future__ import annotations

import json
import sys
import threading
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

from logger import log

try:
    import loralib  # type: ignore

    _LORALIB_AVAILABLE = True
    _LORALIB_IMPORT_ERROR: Optional[BaseException] = None
except Exception as exc:  # pragma: no cover - depends on env
    loralib = None  # type: ignore
    _LORALIB_AVAILABLE = False
    _LORALIB_IMPORT_ERROR = exc

MODE_TX = "tx"
MODE_RX = "rx"

LORA_MAX_BYTES = 200

_DEFAULT_CONFIG = {
    "mode": MODE_TX,
    "frequency_hz": 433_000_000,
    "spread_factor": 7,
    "poll_interval": 0.05,
    "frame_timeout": 2.0,
}

_CONFIG_PATH = Path(__file__).with_name("lora_config.json")


def _coerce_int(value: Any, default: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _coerce_float(value: Any, default: float) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _load_config() -> Dict[str, Any]:
    config = dict(_DEFAULT_CONFIG)
    try:
        with _CONFIG_PATH.open("r", encoding="utf-8") as fh:
            raw = json.load(fh)
    except FileNotFoundError:
        log("LORA", f"No encontré {_CONFIG_PATH.name}; uso configuración por defecto", "WARN")
        return config
    except (json.JSONDecodeError, OSError) as exc:
        log("LORA", f"No pude leer {_CONFIG_PATH.name}: {exc}; uso defaults", "ERROR", sys.stderr)
        return config
    if not isinstance(raw, dict):
        log("LORA", f"{_CONFIG_PATH.name} debe contener un objeto JSON; uso defaults", "ERROR", sys.stderr)
        return config
    for key in ("mode", "frequency_hz", "spread_factor", "poll_interval", "frame_timeout"):
        if key in raw:
            config[key] = raw[key]
    mode_value = config.get("mode")
    mode_normalized = mode_value.strip().lower() if isinstance(mode_value, str) else ""
    if mode_normalized not in (MODE_TX, MODE_RX):
        log(
            "LORA",
            f"Modo '{mode_value}' inválido en {_CONFIG_PATH.name}; uso '{MODE_TX.upper()}'",
            "WARN",
        )
        mode_normalized = MODE_TX
    config["mode"] = mode_normalized
    config["frequency_hz"] = _coerce_int(config.get("frequency_hz"), _DEFAULT_CONFIG["frequency_hz"])
    if config["frequency_hz"] <= 0:
        log(
            "LORA",
            f"Frecuencia inválida en {_CONFIG_PATH.name}; uso {_DEFAULT_CONFIG['frequency_hz']}",
            "WARN",
        )
        config["frequency_hz"] = _DEFAULT_CONFIG["frequency_hz"]
    config["spread_factor"] = _coerce_int(config.get("spread_factor"), _DEFAULT_CONFIG["spread_factor"])
    if config["spread_factor"] not in {7, 8, 9, 10, 11, 12}:
        log(
            "LORA",
            f"SF inválido en {_CONFIG_PATH.name}; uso {_DEFAULT_CONFIG['spread_factor']}",
            "WARN",
        )
        config["spread_factor"] = _DEFAULT_CONFIG["spread_factor"]
    config["poll_interval"] = max(
        0.0,
        _coerce_float(config.get("poll_interval"), _DEFAULT_CONFIG["poll_interval"]),
    )
    config["frame_timeout"] = max(
        0.1,
        _coerce_float(config.get("frame_timeout"), _DEFAULT_CONFIG["frame_timeout"]),
    )
    return config


_CONFIG = _load_config()

LORA_FREQ_HZ = _CONFIG["frequency_hz"]
LORA_SF = _CONFIG["spread_factor"]
_POLL_INTERVAL = _CONFIG["poll_interval"]
_FRAME_TIMEOUT = _CONFIG["frame_timeout"]

_LORA_MODE = _CONFIG["mode"]
_LORA_READY = False
_LORA_INIT_ERROR: Optional[str] = None

_HAS_RECV = _LORALIB_AVAILABLE and hasattr(loralib, "recv")
_WARNED_RECV_UNAVAILABLE = False
_WARNED_RX_NOT_READY = False


class _FrameAssembler:
    def __init__(self, timeout: float):
        self._timeout = max(0.1, timeout)
        self._pending: Dict[str, Dict[str, Any]] = {}

    def push(self, topic: str, index: int, total: int, payload: bytes, now: float) -> Optional[bytes]:
        if total <= 0:
            total = 1
        if index < 1 or index > total:
            return None
        bucket = self._pending.get(topic)
        if bucket is None or bucket.get("total") != total:
            bucket = {"total": total, "frames": {}, "stamp": now}
            self._pending[topic] = bucket
        bucket["stamp"] = now
        bucket["frames"][index] = payload
        if len(bucket["frames"]) == total and all(
            idx in bucket["frames"] for idx in range(1, total + 1)
        ):
            message = b"".join(bucket["frames"][idx] for idx in range(1, total + 1))
            del self._pending[topic]
            return message
        return None

    def cleanup(self, now: float) -> None:
        stale = [
            topic
            for topic, bucket in self._pending.items()
            if (now - bucket.get("stamp", now)) > self._timeout
        ]
        for topic in stale:
            del self._pending[topic]


_FRAME_ASSEMBLER = _FrameAssembler(_FRAME_TIMEOUT)


def lora_init_tx() -> None:
    global _LORA_MODE, _LORA_READY, _WARNED_RX_NOT_READY, _LORA_INIT_ERROR
    if not _LORALIB_AVAILABLE:
        _LORA_READY = False
        _LORA_INIT_ERROR = (
            f"loralib no disponible: {_LORALIB_IMPORT_ERROR}"
            if _LORALIB_IMPORT_ERROR
            else "loralib no está instalado"
        )
        log("LORA", f"no pude inicializar uwu (dependencia faltante): {_LORA_INIT_ERROR}", "ERROR", sys.stderr)
        return
    try:
        loralib.init(0, LORA_FREQ_HZ, LORA_SF)
        _LORA_MODE = MODE_TX
        _LORA_READY = True
        _LORA_INIT_ERROR = None
        _WARNED_RX_NOT_READY = False
        log(
            "LORA",
            f"cargando uwu: TX @ {LORA_FREQ_HZ} Hz, SF{LORA_SF}",
            "SYS",
        )
    except BaseException as exc:
        _LORA_READY = False
        _LORA_INIT_ERROR = str(exc)
        log("LORA", f"no pude inicializar uwu: {exc}", "ERROR", sys.stderr)


def lora_init_rx() -> None:
    global _LORA_MODE, _LORA_READY, _WARNED_RX_NOT_READY, _LORA_INIT_ERROR
    if not _LORALIB_AVAILABLE:
        _LORA_READY = False
        _LORA_INIT_ERROR = (
            f"loralib no disponible: {_LORALIB_IMPORT_ERROR}"
            if _LORALIB_IMPORT_ERROR
            else "loralib no está instalado"
        )
        log("LORA", f"no pude inicializar uwu en modo RX: {_LORA_INIT_ERROR}", "ERROR", sys.stderr)
        return
    try:
        loralib.init(1, LORA_FREQ_HZ, LORA_SF)
        _LORA_MODE = MODE_RX
        _LORA_READY = True
        _LORA_INIT_ERROR = None
        _WARNED_RX_NOT_READY = False
        log(
            "LORA",
            f"cargando uwu: RX @ {LORA_FREQ_HZ} Hz, SF{LORA_SF}",
            "SYS",
        )
    except BaseException as exc:
        _LORA_READY = False
        _LORA_INIT_ERROR = str(exc)
        log("LORA", f"no pude inicializar uwu en modo RX: {exc}", "ERROR", sys.stderr)


def configure_from_config() -> str:
    if _CONFIG["mode"] == MODE_RX:
        lora_init_rx()
    else:
        lora_init_tx()
    return _LORA_MODE


def get_mode() -> str:
    return _LORA_MODE


def is_ready() -> bool:
    return _LORA_READY


def get_init_error() -> Optional[str]:
    return _LORA_INIT_ERROR


def record_init_error(reason: str) -> None:
    global _LORA_READY, _LORA_INIT_ERROR
    _LORA_READY = False
    _LORA_INIT_ERROR = reason


def _chunk_bytes(data: bytes, max_len: int) -> List[bytes]:
    return [data[i : i + max_len] for i in range(0, len(data), max_len)]


def _make_frames(topic: str, payload: Dict[str, Any], max_len: int) -> List[bytes]:
    body = json.dumps(payload, ensure_ascii=True, separators=(",", ":")).encode("utf-8")
    topic_bytes = topic.encode("ascii", errors="ignore")[:15]
    head_fixed = 1 + 1 + len(topic_bytes) + 1 + 1
    room = max(1, max_len - head_fixed)
    parts = _chunk_bytes(body, room)
    total = len(parts)
    frames = []
    for idx, part in enumerate(parts, 1):
        frame = bytearray()
        frame.extend(b"J")
        frame.append(len(topic_bytes))
        frame.extend(topic_bytes)
        frame.append(idx & 0xFF)
        frame.append(total & 0xFF)
        frame.extend(part)
        frames.append(bytes(frame))
    return frames


def send_to_lora(payload: Dict[str, Any]) -> None:
    if _LORA_MODE != MODE_TX:
        log("LORA", "modo RX activo; omito envío", "WARN")
        return
    if not _LORALIB_AVAILABLE:
        log("LORA", "loralib no está disponible; omito envío", "ERROR", sys.stderr)
        return
    if not _LORA_READY:
        log("LORA", "no listo; omito envío (modo test)", "WARN")
        return
    try:
        frames = _make_frames("sensors", payload, LORA_MAX_BYTES)
        if len(frames) == 1:
            loralib.send(frames[0])
            log("LORA", f"Enviado uwu: {len(frames[0])} B (1/1)", "INFO")
        else:
            for idx, frame in enumerate(frames, 1):
                loralib.send(frame)
                log("LORA", f"Enviado uwu: frame {idx}/{len(frames)} ({len(frame)} B)", "INFO")
                time.sleep(0.05)
    except Exception as exc:
        log("LORA", f"falló envío uwu: {exc}", "ERROR", sys.stderr)


def _parse_frame(frame: bytes) -> Optional[Dict[str, Any]]:
    if len(frame) < 5:
        return None
    if frame[0] != ord("J"):
        return None
    topic_len = frame[1]
    head = 2 + topic_len + 2
    if head > len(frame):
        return None
    topic_bytes = frame[2 : 2 + topic_len]
    topic = topic_bytes.decode("ascii", errors="ignore") or "sensors"
    index = frame[2 + topic_len] or 1
    total = frame[3 + topic_len] or 1
    payload = frame[4 + topic_len :]
    return {
        "topic": topic,
        "index": index,
        "total": total,
        "payload": payload,
    }


def poll_received_payload() -> Optional[Dict[str, Any]]:
    global _WARNED_RECV_UNAVAILABLE, _WARNED_RX_NOT_READY
    if _LORA_MODE != MODE_RX:
        return None
    if not _HAS_RECV:
        if not _WARNED_RECV_UNAVAILABLE:
            log("LORA", "loralib no soporta recv(); no puedo escuchar uwu", "ERROR", sys.stderr)
            _WARNED_RECV_UNAVAILABLE = True
        return None
    if not _LORA_READY:
        if not _WARNED_RX_NOT_READY:
            log("LORA", "LoRa RX no está listo todavía", "WARN")
            _WARNED_RX_NOT_READY = True
        return None
    try:
        buffer, length, last_rssi, current_rssi, snr, error = loralib.recv()
    except Exception as exc:
        log("LORA", f"falló recv uwu: {exc}", "ERROR", sys.stderr)
        return None
    if error != 0 or length <= 0:
        return None
    frame = bytes(buffer[:length])
    parsed = _parse_frame(frame)
    if not parsed:
        log("LORA", f"frame inválido uwu: {frame.hex()}", "WARN")
        return None
    now = time.time()
    _FRAME_ASSEMBLER.cleanup(now)
    assembled = _FRAME_ASSEMBLER.push(
        parsed["topic"],
        int(parsed["index"]),
        int(parsed["total"]),
        parsed["payload"],
        now,
    )
    if assembled is None:
        return None
    try:
        payload_json = json.loads(assembled.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        log("LORA", f"no pude decodificar payload uwu: {exc}", "ERROR", sys.stderr)
        return None
    metadata = {
        "last_rssi": last_rssi,
        "rssi": current_rssi,
        "snr": snr,
        "frame_index": int(parsed["index"]),
        "frame_total": int(parsed["total"]),
    }
    return {
        "topic": parsed["topic"],
        "payload": payload_json,
        "metadata": metadata,
    }


def _default_rx_handler(packet: Dict[str, Any]) -> None:
    metadata = packet.get("metadata", {})
    payload = packet.get("payload", {})
    summary = json.dumps(payload, ensure_ascii=True, separators=(",", ":"))
    last_rssi = metadata.get("last_rssi")
    snr = metadata.get("snr")
    log(
        "LORA",
        f"RX uwu: {packet.get('topic', 'sensors')} -> {summary} (RSSI {last_rssi} dBm, SNR {snr} dB)",
        "INFO",
    )


def receive_loop(
    stop_event: Optional[threading.Event] = None,
    handler: Optional[Callable[[Dict[str, Any]], None]] = None,
    poll_interval: Optional[float] = None,
) -> None:
    if _LORA_MODE != MODE_RX:
        log("LORA", "modo actual no es RX; detengo receive_loop", "WARN")
        return
    if not _LORA_READY:
        log("LORA", "LoRa RX no está listo; seguiré esperando reintentos", "WARN")
    listener = handler or _default_rx_handler
    interval = _POLL_INTERVAL if poll_interval is None else max(0.0, float(poll_interval))
    while stop_event is None or not stop_event.is_set():
        packet = poll_received_payload()
        if packet is not None:
            try:
                listener(packet)
            except Exception as exc:
                log("LORA", f"handler RX lanzó excepción uwu: {exc}", "ERROR", sys.stderr)
        if interval > 0:
            time.sleep(interval)


__all__ = [
    "MODE_RX",
    "MODE_TX",
    "configure_from_config",
    "record_init_error",
    "get_mode",
    "get_init_error",
    "is_ready",
    "lora_init_rx",
    "lora_init_tx",
    "poll_received_payload",
    "receive_loop",
    "send_to_lora",
]
