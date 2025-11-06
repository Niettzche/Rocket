#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="${SCRIPT_DIR}"
readonly VENV_DIR="${REPO_ROOT}/.venv"
readonly LORALIB_REPO="${LORALIB_REPO:-https://github.com/wdomski/LoRa-RaspberryPi.git}"
readonly LORALIB_REF="${LORALIB_REF:-master}"
readonly LORALIB_DIR="${REPO_ROOT}/external/LoRa-RaspberryPi"
readonly REQUIREMENTS_FILE="${REPO_ROOT}/requirements.txt"
readonly WIRINGPI_REPO="${WIRINGPI_REPO:-https://github.com/WiringPi/WiringPi.git}"
readonly WIRINGPI_REF="${WIRINGPI_REF:-master}"
readonly WIRINGPI_DIR="${REPO_ROOT}/external/WiringPi"
readonly COLOR_RESET=$'\033[0m'
readonly COLOR_INFO=$'\033[1;36m'
readonly COLOR_WARN=$'\033[1;33m'
readonly COLOR_ERROR=$'\033[1;31m'
readonly APT_PACKAGES=(
  python3
  python3-venv
  python3-pip
  python3-dev
  python3-smbus
  python3-rpi.gpio
  wiringpi
  build-essential
  make
  git
)

log_info() {
  printf '%s[setup]%s %s\n' "${COLOR_INFO}" "${COLOR_RESET}" "$*"
}

log_warn() {
  printf '%s[setup]%s %s\n' "${COLOR_WARN}" "${COLOR_RESET}" "$*"
}

log_error() {
  printf '%s[setup]%s %s\n' "${COLOR_ERROR}" "${COLOR_RESET}" "$*" 1>&2
}

require_command() {
  local cmd="$1"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    log_error "El comando requerido '${cmd}' no está disponible. Instálalo manualmente e intenta de nuevo."
    exit 1
  fi
}

install_apt_dependencies() {
  if ! command -v apt-get >/dev/null 2>&1; then
    log_warn "Sistema sin apt-get; omitiendo instalación de paquetes de sistema."
    return
  fi

  local apt_cmd=(apt-get)
  if [[ "$(id -u)" -ne 0 ]]; then
    if command -v sudo >/dev/null 2>&1; then
      apt_cmd=(sudo apt-get)
    else
      log_error "Necesitas privilegios de administrador para instalar dependencias del sistema."
      exit 1
    fi
  fi

  log_info "Instalando paquetes de sistema: ${APT_PACKAGES[*]}"
  "${apt_cmd[@]}" update
  "${apt_cmd[@]}" install -y --no-install-recommends "${APT_PACKAGES[@]}"
}

ensure_virtualenv() {
  require_command python3

  if [[ ! -d "${VENV_DIR}" ]]; then
    log_info "Creando entorno virtual en ${VENV_DIR}"
    python3 -m venv "${VENV_DIR}"
  fi
}

install_python_packages() {
  ensure_virtualenv

  local pip_bin="${VENV_DIR}/bin/pip"
  log_info "Actualizando pip en el entorno virtual"
  "${pip_bin}" install --upgrade pip

  if [[ -f "${REQUIREMENTS_FILE}" ]]; then
    log_info "Instalando dependencias de Python desde ${REQUIREMENTS_FILE}"
    "${pip_bin}" install -r "${REQUIREMENTS_FILE}"
  else
    log_warn "No se encontró ${REQUIREMENTS_FILE}; omitiendo instalación de paquetes de Python."
  fi
}

setup_wiringpi() {
  require_command git

  mkdir -p "$(dirname "${WIRINGPI_DIR}")"

  if [[ ! -d "${WIRINGPI_DIR}/.git" ]]; then
    log_info "Clonando WiringPi (${WIRINGPI_REF}) en ${WIRINGPI_DIR}"
    git clone --depth 1 --branch "${WIRINGPI_REF}" "${WIRINGPI_REPO}" "${WIRINGPI_DIR}"
  else
    log_info "Actualizando WiringPi"
    git -C "${WIRINGPI_DIR}" fetch --depth 1 origin "${WIRINGPI_REF}"
    git -C "${WIRINGPI_DIR}" checkout "${WIRINGPI_REF}"
    git -C "${WIRINGPI_DIR}" pull --ff-only
  fi

  local build_script="${WIRINGPI_DIR}/build"
  if [[ -x "${build_script}" ]]; then
    if [[ "$(id -u)" -eq 0 ]]; then
      log_info "Instalando WiringPi ejecutando ${build_script}"
      (cd "${WIRINGPI_DIR}" && "${build_script}")
    elif command -v sudo >/dev/null 2>&1; then
      log_info "Instalando WiringPi con sudo ${build_script}"
      (cd "${WIRINGPI_DIR}" && sudo "${build_script}")
    else
      log_warn "Se requiere privilegio de administrador para instalar WiringPi; ejecuta ${build_script} manualmente."
    fi
  else
    log_warn "No se encontró script de build en WiringPi; revisa ${WIRINGPI_DIR} para instrucciones manuales."
  fi
}

setup_loralib() {
  require_command git
  require_command make
  require_command python3

  mkdir -p "$(dirname "${LORALIB_DIR}")"

  if [[ ! -d "${LORALIB_DIR}/.git" ]]; then
    log_info "Clonando LoRa-RaspberryPi (${LORALIB_REF}) en ${LORALIB_DIR}"
    git clone --depth 1 --branch "${LORALIB_REF}" "${LORALIB_REPO}" "${LORALIB_DIR}"
  else
    log_info "Actualizando LoRa-RaspberryPi"
    git -C "${LORALIB_DIR}" fetch --depth 1 origin "${LORALIB_REF}"
    git -C "${LORALIB_DIR}" checkout "${LORALIB_REF}"
    git -C "${LORALIB_DIR}" pull --ff-only
  fi

  local py_version
  py_version="$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')"
  log_info "Compilando loralib para Python ${py_version}"
  make -C "${LORALIB_DIR}" all PYTHON_VERSION="${py_version}"

  log_info "Para usar loralib, exporta PYTHONPATH añadiendo:"
  printf 'export PYTHONPATH="%s:${PYTHONPATH}"\n' "${LORALIB_DIR}"
}

install_web_dependencies() {
  local web_dir="${REPO_ROOT}/receptor_arudino/webpage"
  if [[ ! -f "${web_dir}/package.json" ]]; then
    log_warn "No se encontró interfaz web React; omitiendo dependencias de npm."
    return
  fi

  if ! command -v npm >/dev/null 2>&1; then
    log_warn "npm no está instalado; omitiendo dependencias de la interfaz web."
    return
  fi

  log_info "Instalando dependencias de la interfaz web (npm install)"
  (cd "${web_dir}" && npm install)
}

main() {
  install_apt_dependencies
  install_python_packages
  setup_wiringpi
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
