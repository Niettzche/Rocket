from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from typing import Any, Dict, Iterable, Optional

@dataclass
class SensorMessage:
    sensor: str
    timestamp: float
    data: Dict[str, Any]

    def to_payload(self) -> Dict[str, Any]:
        return {"timestamp": isoformat_utc(self.timestamp), **self.data}

def isoformat_utc(ts: float) -> str:
    return datetime.utcfromtimestamp(ts).isoformat(timespec="microseconds") + "Z"

def build_payload(latest: Dict[str, SensorMessage], expected: Iterable[str], reported_at: float) -> Dict[str, Any]:
    body: Dict[str, Optional[Dict[str, Any]]] = {}
    for sensor in expected:
        message = latest.get(sensor)
        body[sensor] = message.to_payload() if message else None
    return {"reported_at": isoformat_utc(reported_at), "sensors": body}
