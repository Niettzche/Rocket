"""BMP180 reader helper module.

Provides functions to open the serial link to the Arduino proxy and fetch raw
lines produced by the pressure/temperature sensor.
"""

from __future__ import annotations

import time
from typing import Optional, Tuple

try:
    import serial
except ImportError as exc:  # pragma: no cover
    serial = None  # type: ignore[assignment]
    _SERIAL_IMPORT_ERROR = exc
else:
    _SERIAL_IMPORT_ERROR = None


DEFAULT_PORT = "/dev/ttyUSB0"
DEFAULT_BAUDRATE = 9600
DEFAULT_TIMEOUT = 0.2
DEFAULT_SETTLING_TIME = 2.0


class SerialNotAvailable(RuntimeError):
    """Raised when pyserial could not be imported."""


def _ensure_serial_imported() -> None:
    if serial is None:  # pragma: no cover
        raise SerialNotAvailable("pyserial is required") from _SERIAL_IMPORT_ERROR


def open_connection(
    port: str = DEFAULT_PORT,
    baudrate: int = DEFAULT_BAUDRATE,
    timeout: float = DEFAULT_TIMEOUT,
    settling_time: float = DEFAULT_SETTLING_TIME,
):
    """Return an opened and configured serial connection to the Arduino."""
    _ensure_serial_imported()
    conn = serial.Serial(port, baudrate, timeout=timeout)
    if settling_time > 0:
        time.sleep(settling_time)
    return conn


def read_measurement(conn) -> Optional[Tuple[float, str]]:
    """Read a single raw measurement from the Arduino.

    Returns a tuple (timestamp, payload) or ``None`` if no full line was
    available yet.
    """
    raw = conn.readline()
    if not raw:
        return None

    decoded = raw.decode("utf-8", errors="replace").strip()
    if not decoded:
        return None

    return time.time(), decoded


def iter_measurements(**kwargs):
    """Yield successive measurements until interrupted."""
    conn = open_connection(**kwargs)
    try:
        while True:
            sample = read_measurement(conn)
            if sample is None:
                continue
            yield sample
    finally:
        conn.close()


def main() -> None:  # pragma: no cover
    try:
        for timestamp, payload in iter_measurements():
            print(f"Recibido @ {timestamp:.03f}: {payload}")
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":  # pragma: no cover
    main()
