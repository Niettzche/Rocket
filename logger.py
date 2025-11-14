from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict

LEVEL_COLORS = {
    "INFO": "\033[92m",
    "WARN": "\033[93m",
    "ERROR": "\033[91m",
    "DEBUG": "\033[94m",
    "SYS": "\033[96m",
    "PAYLOAD": "\033[95m",
    "RESET": "\033[0m",
}

_PAYLOAD_LOG_FILE = Path(__file__).resolve().parent / "logs" / "payloads.log"
_CONFIG_FILE = Path(__file__).resolve().parent / "config.json"

def _load_config() -> Dict[str, Any]:
    try:
        with _CONFIG_FILE.open("r", encoding="utf-8") as fp:
            data = json.load(fp)
    except FileNotFoundError:
        return {}
    except json.JSONDecodeError as exc:
        sys.stderr.write(f"[WARN] Configuración inválida en {_CONFIG_FILE}: {exc}\n")
        sys.stderr.flush()
        return {}
    return data if isinstance(data, dict) else {}

def _bool_setting(key: str, default: bool) -> bool:
    raw = _load_config().get(key, default)
    if isinstance(raw, bool):
        return raw
    if isinstance(raw, str):
        normalized = raw.strip().lower()
        if normalized in {"true", "1", "yes", "on"}:
            return True
        if normalized in {"false", "0", "no", "off"}:
            return False
    return default

PRINT_PAYLOADS = _bool_setting("print_payloads", True)

def log(sensor: str, message: str, level: str = "INFO", stream: Any = sys.stdout) -> None:
    level_key = level.upper()
    color = LEVEL_COLORS.get(level_key, LEVEL_COLORS["INFO"])
    reset = LEVEL_COLORS["RESET"]
    prefix = f"{color}[{level_key}] [{sensor}] {reset}"
    print(prefix + message, file=stream)
    stream.flush()

def log_payload(payload: Dict[str, Any]) -> None:
    pretty = json.dumps(payload, indent=2, ensure_ascii=True)
    if PRINT_PAYLOADS:
        color = LEVEL_COLORS["PAYLOAD"]
        reset = LEVEL_COLORS["RESET"]
        print(f"{color}[PAYLOAD] [AGREGADOR]{reset} fotito uwu\n{pretty}")
        sys.stdout.flush()
    _persist_payload(payload)


def _persist_payload(payload: Dict[str, Any]) -> None:
    timestamp = datetime.now(timezone.utc).isoformat()
    serialized = json.dumps(payload, ensure_ascii=True)
    try:
        _PAYLOAD_LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
        with _PAYLOAD_LOG_FILE.open("a", encoding="utf-8") as fp:
            fp.write(f"{timestamp} {serialized}\n")
    except OSError as exc:
        log("SYSTEM", f"No pude guardar payloads en {_PAYLOAD_LOG_FILE}: {exc}", "ERROR", sys.stderr)
