from __future__ import annotations

import json
import sys
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

def log(sensor: str, message: str, level: str = "INFO", stream: Any = sys.stdout) -> None:
    level_key = level.upper()
    color = LEVEL_COLORS.get(level_key, LEVEL_COLORS["INFO"])
    reset = LEVEL_COLORS["RESET"]
    prefix = f"{color}[{level_key}] [{sensor}] {reset}"
    print(prefix + message, file=stream)
    stream.flush()

def log_payload(payload: Dict[str, Any]) -> None:
    pretty = json.dumps(payload, indent=2, ensure_ascii=True)
    color = LEVEL_COLORS["PAYLOAD"]
    reset = LEVEL_COLORS["RESET"]
    print(f"{color}[PAYLOAD] [AGREGADOR]{reset} fotito uwu\n{pretty}")
    sys.stdout.flush()
