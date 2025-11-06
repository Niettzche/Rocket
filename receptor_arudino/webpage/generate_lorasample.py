#!/usr/bin/env python3
"""Generate randomized LoRa sensor samples until interrupted."""

from __future__ import annotations

import json
import random
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path

OUTPUT_PATH = Path("lora_payload_sample.json")
UPDATE_INTERVAL_SECONDS = 1.0


def utc_now_iso(offset_ms: int = 0) -> str:
    """Return an ISO8601 timestamp in UTC with microsecond precision."""
    now = datetime.now(timezone.utc) - timedelta(milliseconds=offset_ms)
    return now.isoformat(timespec="microseconds").replace("+00:00", "Z")


def random_payload() -> dict[str, object]:
    """Compose a randomized payload that matches the expected structure."""
    return {
        "reported_at": utc_now_iso(),
        "sensors": {
            "mpu6050": {
                "timestamp": utc_now_iso(offset_ms=5),
                "accel_g": {
                    "ax": round(random.uniform(-2.0, 2.0), 3),
                    "ay": round(random.uniform(-2.0, 2.0), 3),
                    "az": round(random.uniform(0.8, 1.2), 3),
                },
                "gyro_dps": {
                    "gx": round(random.uniform(-250.0, 250.0), 2),
                    "gy": round(random.uniform(-250.0, 250.0), 2),
                    "gz": round(random.uniform(-250.0, 250.0), 2),
                },
                "attitude_deg": {
                    "pitch": round(random.uniform(-45.0, 45.0), 2),
                    "roll": round(random.uniform(-45.0, 45.0), 2),
                    "yaw": round(random.uniform(0.0, 360.0), 2),
                },
            },
            "bmp180": {
                "timestamp": utc_now_iso(offset_ms=60),
                "raw": {
                    "T": round(random.uniform(15.0, 40.0), 2),
                    "P": round(random.uniform(980.0, 1040.0), 2),
                },
            },
            "neo6m": {
                "timestamp": utc_now_iso(offset_ms=110),
                "latitude": round(random.uniform(-90.0, 90.0), 5),
                "longitude": round(random.uniform(-180.0, 180.0), 5),
                "altitude": round(random.uniform(0.0, 2000.0), 1),
                "fix_time": utc_now_iso(offset_ms=200),
                "raw": "$GPGGA,000000.00,0000.0000,N,00000.0000,E,1,08,0.9,000.0,M,0.0,M,,*00",
            },
        },
    }


def write_payload(payload: dict[str, object]) -> None:
    OUTPUT_PATH.write_text(json.dumps(payload, indent=2), encoding="utf-8")


if __name__ == "__main__":
    try:
        while True:
            payload = random_payload()
            write_payload(payload)
            print(json.dumps(payload, indent=2))
            time.sleep(UPDATE_INTERVAL_SECONDS)
    except KeyboardInterrupt:
        print("\nDetenido por el usuario.")
