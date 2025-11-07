#!/usr/bin/env python3
"""Serial bridge for the LoRa receptor Arduino + Electron GUI.

The script listens to the USB serial port, extracts the JSON payload that the
firmware prints once a complete LoRa frame is reconstructed, and writes it to
the file that the Electron/React UI already consumes (`webpage/lora_payload_sample.json`).

It now supports:
  * `--list-ports` to enumerate available serial interfaces (JSON output).
  * `--auto` to pick the first port that looks like an Arduino/USB-serial chip.
  * The original bridge mode (`--port /dev/ttyUSB0`) used both manually and
    by the Electron helper UI.
"""

from __future__ import annotations

import argparse
import json
import logging
import signal
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional, Sequence

try:
    import serial  # type: ignore
    from serial import SerialException  # type: ignore
    from serial.tools import list_ports  # type: ignore
except ModuleNotFoundError as exc:  # pragma: no cover - helpful error for users
    print(
        "[serial-bridge] pyserial no está instalado. Ejecuta `pip install pyserial` "
        "en tu entorno de Python antes de usar este script.",
        file=sys.stderr,
    )
    raise


DEFAULT_OUTPUT = Path(__file__).resolve().parent / "webpage" / "lora_payload_sample.json"
START_MARKER = "===== Payload recibido ====="
END_MARKER = "============================"
ARDUINO_VENDOR_IDS = {0x2341, 0x2A03}
POPULAR_USB_VENDOR_IDS = ARDUINO_VENDOR_IDS | {0x1A86, 0x10C4}  # CH340 / Silicon Labs
PORT_KEYWORDS = ("arduino", "nano", "wch", "ch340", "cp210", "silicon labs", "usb-serial")


def utc_now_iso() -> str:
    """Return an ISO8601 timestamp in UTC with microsecond precision."""
    return datetime.now(timezone.utc).isoformat(timespec="microseconds").replace("+00:00", "Z")


def write_payload(payload: dict[str, Any], output_path: Path) -> None:
    """Persist the payload as pretty JSON to the provided path."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Bridge del puerto serial -> archivo lora_payload_sample.json para Electron."
    )
    parser.add_argument(
        "--port",
        help="Dispositivo serial (ej. /dev/ttyUSB0, /dev/ttyACM0, COM3).",
    )
    parser.add_argument(
        "--auto",
        action="store_true",
        help="Detecta automáticamente el puerto (útil para Arduino Nano).",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="Imprime en JSON los puertos detectados y termina.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baud rate configurado en el sketch de Arduino (default: 115200).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=1.0,
        help="Tiempo máximo de espera (s) para readline sobre el puerto serial.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"Ruta del JSON que consumirá Electron (default: {DEFAULT_OUTPUT}).",
    )
    parser.add_argument(
        "--mirror",
        action="store_true",
        help="Imprime en consola el último payload válido para depuración.",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Nivel de logging del script (default: INFO).",
    )
    return parser


def parse_args() -> tuple[argparse.Namespace, argparse.ArgumentParser]:
    parser = build_parser()
    args = parser.parse_args()

    if not args.list_ports:
        if args.auto and args.port:
            parser.error("Usa solo --port o --auto, pero no ambos.")
        if not args.auto and not args.port:
            parser.error("Debes especificar --port PORT o usar --auto (o ejecutar --list-ports).")

    return args, parser


def serial_port_to_dict(port: Any) -> dict[str, Any]:
    """Convert a pyserial ListPortInfo object to JSON-serializable dict."""
    return {
        "device": port.device,
        "description": port.description,
        "manufacturer": port.manufacturer,
        "product": getattr(port, "product", None),
        "serial_number": port.serial_number,
        "hwid": port.hwid,
        "vid": port.vid,
        "pid": port.pid,
    }


def is_preferred_port(port_info: dict[str, Any]) -> bool:
    """Heurística para elegir puertos que parecen Arduino Nano / USB-Serial."""
    manufacturer = (port_info.get("manufacturer") or "").lower()
    description = (port_info.get("description") or "").lower()
    hwid = (port_info.get("hwid") or "").lower()
    device = (port_info.get("device") or "").lower()
    vid = port_info.get("vid")

    keyword_hit = any(
        keyword in manufacturer or keyword in description or keyword in hwid for keyword in PORT_KEYWORDS
    )
    vid_hit = vid in POPULAR_USB_VENDOR_IDS
    device_hit = any(keyword in device for keyword in ("ttyusb", "ttyacm", "usbserial", "wchusb"))

    return keyword_hit or vid_hit or device_hit


def enumerate_serial_ports() -> list[dict[str, Any]]:
    """Return a sortable list with metadata and preferred flag."""
    entries: list[dict[str, Any]] = []
    for port in sorted(list_ports.comports(), key=lambda item: item.device or ""):
        info = serial_port_to_dict(port)
        info["preferred"] = is_preferred_port(info)
        entries.append(info)
    entries.sort(key=lambda item: (not item["preferred"], item["device"] or ""))
    return entries


def select_auto_port(entries: Sequence[dict[str, Any]]) -> Optional[str]:
    """Pick the first preferred port, otherwise the first entry."""
    for entry in entries:
        if entry.get("preferred"):
            return entry.get("device")
    return entries[0].get("device") if entries else None


def handle_list_ports() -> int:
    entries = enumerate_serial_ports()
    auto_port = select_auto_port(entries)
    payload = {"ports": entries, "auto": auto_port}
    print(json.dumps(payload, indent=2))
    return 0


class SerialPayloadBridge:
    """State machine that hunts for the JSON payload blocks in the serial log."""

    def __init__(self, output_path: Path, mirror: bool = False) -> None:
        self.output_path = output_path
        self.mirror = mirror
        self._topic: Optional[str] = None
        self._capturing = False

    def process_line(self, line: str) -> None:
        stripped = line.strip()
        if not stripped:
            return

        if stripped.startswith(START_MARKER):
            logging.debug("Inicio de payload detectado")
            self._capturing = True
            self._topic = None
            return

        if not self._capturing:
            return

        if stripped.startswith("Topic:"):
            self._topic = stripped.split(":", 1)[1].strip() or None
            logging.debug("Topic detectado: %s", self._topic)
            return

        if stripped.startswith("{") or stripped.startswith("["):
            self._handle_payload_line(stripped)
            return

        if stripped.startswith(END_MARKER):
            logging.debug("Fin de payload")
            self._capturing = False
            self._topic = None

    def _handle_payload_line(self, text: str) -> None:
        try:
            payload = json.loads(text)
            if isinstance(payload, dict):
                payload.setdefault("_meta", {})
                meta = payload["_meta"]
                if isinstance(meta, dict):
                    meta.setdefault("topic", self._topic or "sensors")
                    meta["received_at"] = utc_now_iso()
            logging.info("Payload válido recibido (%s)", self._topic or "sensors")
            write_payload(payload, self.output_path)
            if self.mirror:
                print(json.dumps(payload, indent=2))
        except json.JSONDecodeError:
            logging.warning("No se pudo parsear el payload como JSON:\n%s", text)


def main() -> int:
    args, _ = parse_args()

    if args.list_ports:
        return handle_list_ports()

    logging.basicConfig(
        level=getattr(logging, args.log_level.upper()),
        format="[%(asctime)s] %(levelname)s %(message)s",
    )

    bridge = SerialPayloadBridge(args.output, mirror=args.mirror)
    stop = False

    selected_port = args.port
    if args.auto:
        entries = enumerate_serial_ports()
        selected_port = select_auto_port(entries)
        if not selected_port:
            logging.error("No se detectó ningún puerto serial disponible.")
            return 2
        logging.info("Puerto auto-detectado: %s", selected_port)

    assert selected_port  # guarded arriba

    def _handle_stop(signum: int, _: Any) -> None:
        nonlocal stop
        logging.info("Señal %s recibida, cerrando puente…", signum)
        stop = True

    signal.signal(signal.SIGINT, _handle_stop)
    signal.signal(signal.SIGTERM, _handle_stop)

    logging.info("Escuchando %s @ %d baudios", selected_port, args.baud)
    try:
        with serial.Serial(selected_port, args.baud, timeout=args.timeout) as ser:
            while not stop:
                try:
                    raw = ser.readline()
                except SerialException as exc:
                    logging.error("Error al leer del puerto serial: %s", exc)
                    break

                if not raw:
                    continue

                try:
                    line = raw.decode("utf-8", errors="ignore")
                except UnicodeDecodeError:
                    logging.debug("Bytes no UTF-8 ignorados")
                    continue

                bridge.process_line(line)

    except SerialException as exc:
        logging.error("No se pudo abrir el puerto %s: %s", selected_port, exc)
        return 1

    logging.info("Bridge detenido")
    return 0


if __name__ == "__main__":
    sys.exit(main())

