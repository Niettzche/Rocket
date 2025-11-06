#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="${SCRIPT_DIR}"
readonly VENV_DIR="${REPO_ROOT}/.venv"
readonly LORALIB_REPO="${LORALIB_REPO:-https://github.com/HelTecAutomation/LoRa-RaspberryPi.git}"
readonly LORALIB_REF="${LORALIB_REF:-master}"
readonly LORALIB_DIR="${REPO_ROOT}/external/LoRa-RaspberryPi"
readonly APT_PACKAGES=(
  python3
  python3-venv
  python3-pip
  python3-smbus
  python3-rpi.gpio
  wiringpi
  build-essential
  make
  git
)
readonly PIP_PACKAGES=(
  pyserial
  pynmea2
)

log() {
  printf '[setup] %s\n' "$*"
}

require_command() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    log "El comando requerido '${cmd}' no está disponible. Instálalo manualmente e intenta de nuevo."
    exit 1
  fi
}

install_apt_dependencies() {
  if ! command -v apt-get >/dev/null 2>&1; then
    log "Sistema sin apt-get; omitiendo instalación de paquetes de sistema."
    return
  fi

  local apt_cmd=(apt-get)
  if [[ "$(id -u)" -ne 0 ]]; then
    if command -v sudo >/dev/null 2>&1; then
      apt_cmd=(sudo apt-get)
    else
      log "Necesitas privilegios de administrador para instalar dependencias del sistema."
      exit 1
    fi
  fi

  log "Instalando paquetes de sistema: ${APT_PACKAGES[*]}"
  "${apt_cmd[@]}" update
  "${apt_cmd[@]}" install -y --no-install-recommends "${APT_PACKAGES[@]}"
}

ensure_virtualenv() {
  require_command python3

  if [[ ! -d "${VENV_DIR}" ]]; then
    log "Creando entorno virtual en ${VENV_DIR}"
    python3 -m venv "${VENV_DIR}"
  fi
}

install_python_packages() {
  ensure_virtualenv

  local pip_bin="${VENV_DIR}/bin/pip"
  log "Actualizando pip en el entorno virtual"
  "${pip_bin}" install --upgrade pip

  log "Instalando paquetes de Python: ${PIP_PACKAGES[*]}"
  "${pip_bin}" install "${PIP_PACKAGES[@]}"
}

setup_loralib() {
  require_command git
  require_command make

  mkdir -p "$(dirname "${LORALIB_DIR}")"

  if [[ ! -d "${LORALIB_DIR}/.git" ]]; then
    log "Clonando HelTec LoRa-RaspberryPi (${LORALIB_REF}) en ${LORALIB_DIR}"
    git clone --depth 1 --branch "${LORALIB_REF}" "${LORALIB_REPO}" "${LORALIB_DIR}"
  else
    log "Actualizando HelTec LoRa-RaspberryPi"
    git -C "${LORALIB_DIR}" fetch --depth 1 origin "${LORALIB_REF}"
    git -C "${LORALIB_DIR}" checkout "${LORALIB_REF}"
    git -C "${LORALIB_DIR}" pull --ff-only
  fi

  log "Compilando loralib"
  make -C "${LORALIB_DIR}" all

  log "Para usar loralib, exporta PYTHONPATH añadiendo:"
  printf 'export PYTHONPATH="%s:${PYTHONPATH}"\n' "${LORALIB_DIR}"
}

install_web_dependencies() {
  local web_dir="${REPO_ROOT}/receptor_arudino/webpage"
  if [[ ! -f "${web_dir}/package.json" ]]; then
    log "No se encontró interfaz web React; omitiendo dependencias de npm."
    return
  fi

  if ! command -v npm >/dev/null 2>&1; then
    log "npm no está instalado; omitiendo dependencias de la interfaz web."
    return
  fi

  log "Instalando dependencias de la interfaz web (npm install)"
  (cd "${web_dir}" && npm install)
}

main() {
  install_apt_dependencies
  install_python_packages
  setup_loralib
  install_web_dependencies

  cat <<'EOF'

=== Instalación completada ===
- Activa el entorno virtual con: source .venv/bin/activate
- Exporta PYTHONPATH para loralib (ver mensaje anterior) o añade la línea a tu shell rc.
- Ejecuta python3 read_sensors.py desde la raíz del proyecto cuando los sensores estén conectados.
EOF
}

main "$@"
