# syntax=docker/dockerfile:1
FROM python:3.11-slim

ARG BUILD_LORALIB=false
ARG LORALIB_REPO=https://github.com/HelTecAutomation/LoRa-RaspberryPi.git
ARG LORALIB_REF=master

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    APP_HOME=/app

WORKDIR ${APP_HOME}

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        git \
    && if [ "${BUILD_LORALIB}" = "true" ]; then \
        apt-get install -y --no-install-recommends \
            python3-smbus \
            python3-rpi.gpio \
            wiringpi \
        ; \
    fi \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir \
        pyserial \
        pynmea2

RUN if [ "${BUILD_LORALIB}" = "true" ]; then \
        git clone --depth 1 --branch "${LORALIB_REF}" "${LORALIB_REPO}" /opt/LoRa-RaspberryPi \
        && make -C /opt/LoRa-RaspberryPi all \
    ; else \
        mkdir -p /usr/local/lib/python3.11/site-packages \
        && cat <<'PY' > /usr/local/lib/python3.11/site-packages/loralib.py \
"""Fallback stub for loralib so the app can run without LoRa hardware."""\nimport sys\nimport time\n\n_READY = False\n\n\ndef init(spi_channel: int, freq_hz: int, spread_factor: int) -> None:\n    global _READY\n    _READY = True\n    sys.stdout.write(f"[loralib-stub] init(channel={spi_channel}, freq={freq_hz}, sf={spread_factor})\\n")\n    sys.stdout.flush()\n\n\ndef send(payload: bytes) -> None:\n    if not isinstance(payload, (bytes, bytearray)):\n        raise TypeError("payload must be bytes")\n    if not _READY:\n        raise RuntimeError("loralib not initialised")\n    sys.stdout.write(f"[loralib-stub] send {len(payload)} bytes\\n")\n    sys.stdout.flush()\n\n\ndef recv():\n    if not _READY:\n        raise RuntimeError("loralib not initialised")\n    time.sleep(0.5)\n    return b"", 0, -120, -120, -20, 0\n\n\ndef close() -> None:\n    global _READY\n    _READY = False\n    sys.stdout.write("[loralib-stub] close\\n")\n    sys.stdout.flush()\nPY\n    ; fi

ENV PYTHONPATH=/opt/LoRa-RaspberryPi:${PYTHONPATH}

COPY . ${APP_HOME}

CMD ["python", "read_sensors.py"]
