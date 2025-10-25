
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

try:
    import serial
except ImportError as exc:  # pragma: no cover
    serial = None  # type: ignore[assignment]
    _SERIAL_IMPORT_ERROR = exc
else:
    _SERIAL_IMPORT_ERROR = None

try:
    import pynmea2
except ImportError as exc:  # pragma: no cover
    pynmea2 = None  # type: ignore[assignment]
    _PYNMEA_IMPORT_ERROR = exc
else:
    _PYNMEA_IMPORT_ERROR = None


DEFAULT_PORT = "/dev/serial0"
DEFAULT_BAUDRATE = 9600
DEFAULT_TIMEOUT = 0.4


class SerialNotAvailable(RuntimeError):
    """Raised when pyserial could not be imported."""


class ParserNotAvailable(RuntimeError):
    """Raised when pynmea2 could not be imported."""


@dataclass
class Fix:
    latitude: Optional[float]
    longitude: Optional[float]
    altitude: Optional[float]
    fix_time: Optional[str]
    raw_sentence: str


def _ensure_imports() -> None:
    if serial is None:  # pragma: no cover
        raise SerialNotAvailable("pyserial is required") from _SERIAL_IMPORT_ERROR
    if pynmea2 is None:  # pragma: no cover
        raise ParserNotAvailable("pynmea2 is required") from _PYNMEA_IMPORT_ERROR


def open_connection(
    port: str = DEFAULT_PORT,
    baudrate: int = DEFAULT_BAUDRATE,
    timeout: float = DEFAULT_TIMEOUT,
):
    """Return a configured serial port to the GPS."""
    _ensure_imports()
    return serial.Serial(port, baudrate, timeout=timeout)


def read_fix(conn) -> Optional[Fix]:
    """Parse the next GGA/RMC fix from the serial stream."""
    raw = conn.readline()
    if not raw:
        return None

    line = raw.decode("ascii", errors="replace").strip()
    if not line or not (line.startswith("$GPGGA") or line.startswith("$GPRMC")):
        return None

    try:
        msg = pynmea2.parse(line)
    except Exception:
        return None

    latitude = getattr(msg, "latitude", None)
    longitude = getattr(msg, "longitude", None)
    altitude = getattr(msg, "altitude", None)
    timestamp = getattr(msg, "timestamp", None)

    fix_time = timestamp.isoformat() if timestamp else None
    return Fix(latitude, longitude, altitude, fix_time, line)


def iter_fixes(**kwargs):
    """Yield parsed fixes indefinitely."""
    conn = open_connection(**kwargs)
    try:
        while True:
            fix = read_fix(conn)
            if fix is None:
                continue
            yield fix
    finally:
        conn.close()


def main() -> None:  # pragma: no cover
    try:
        for fix in iter_fixes():
            print(
                f"latitud: {fix.latitude}, longitud: {fix.longitude}, "
                f"altitud: {fix.altitude}, hora: {fix.fix_time}"
            )
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":  # pragma: no cover
    main()
