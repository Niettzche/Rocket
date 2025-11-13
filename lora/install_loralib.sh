#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly TARGET_DIR="${SCRIPT_DIR}/LoRa-RaspberryPi"
readonly LORALIB_REPO="${LORALIB_REPO:-https://github.com/wdomski/LoRa-RaspberryPi.git}"
readonly LORALIB_REF="${LORALIB_REF:-master}"

log() {
    printf '[install-lora] %s\n' "$*" >&2
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        log "Error: se requiere el comando '$1' para continuar."
        exit 1
    fi
}

require_cmd git
require_cmd make

mkdir -p "${SCRIPT_DIR}"

if [[ -d "${TARGET_DIR}/.git" ]]; then
    log "Repositorio detectado en ${TARGET_DIR}, sincronizando cambios..."
    git -C "${TARGET_DIR}" fetch origin
    git -C "${TARGET_DIR}" pull --ff-only || {
        log "No se pudo aplicar fast-forward, intente limpiar manualmente ${TARGET_DIR}."
        exit 1
    }
else
    log "Clonando LoRa-RaspberryPi (${LORALIB_REF}) en ${TARGET_DIR}..."
    git clone --depth 1 --branch "${LORALIB_REF}" "${LORALIB_REPO}" "${TARGET_DIR}"
fi

log "Compilando librería (make all)..."
make -C "${TARGET_DIR}" all

log "Instalación finalizada. Agrega '${TARGET_DIR}' a tu PYTHONPATH para usar loralib."
