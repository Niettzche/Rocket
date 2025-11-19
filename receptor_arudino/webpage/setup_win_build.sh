#!/usr/bin/env bash
set -euo pipefail

# Prepares a 32-bit Wine environment and runs the Windows Electron build.
# Designed for Arch-based systems using pacman.

main() {
  ensure_pacman
  install_wine_stack
  init_wine_prefix
  run_electron_build
}

ensure_pacman() {
  if ! command -v pacman >/dev/null 2>&1; then
    echo "pacman not found. This script is intended for Arch-based systems." >&2
    exit 1
  fi
}

install_wine_stack() {
  local wine_pkg=""
  if pacman -Si wine-staging >/dev/null 2>&1; then
    wine_pkg=wine-staging
  elif pacman -Si wine >/dev/null 2>&1; then
    wine_pkg=wine
  else
    echo "wine/wine-staging not found in repositories. Ensure your pacman mirrors are synced and multilib is enabled." >&2
    exit 1
  fi

  local packages=(
    "$wine_pkg"
    wine-gecko
    wine-mono
    lib32-gcc-libs
    lib32-glibc
    lib32-freetype2
    lib32-libpng
  )

  local available=()
  local missing=()

  for pkg in "${packages[@]}"; do
    if pacman -Si "$pkg" >/dev/null 2>&1; then
      available+=("$pkg")
    else
      missing+=("$pkg")
    fi
  done

  if ((${#missing[@]})); then
    echo "The following packages were not found in your configured repositories: ${missing[*]}" >&2
    echo "Enable the [multilib] repo in /etc/pacman.conf, run 'sudo pacman -Syyu', then rerun this script." >&2
    exit 1
  fi

  echo "Installing Wine and 32-bit runtime dependencies (sudo required)..."
  sudo pacman -S --needed "${available[@]}"
}

init_wine_prefix() {
  : "${WINEPREFIX:=$HOME/.wine32}"
  export WINEPREFIX
  export WINEARCH=win32

  if [[ ! -f "$WINEPREFIX/system.reg" ]]; then
    echo "Initializing 32-bit Wine prefix at $WINEPREFIX ..."
    wineboot --init
  else
    echo "Using existing Wine prefix at $WINEPREFIX"
  fi
}

run_electron_build() {
  echo "Running Electron Windows build..."
  npm run build:electron:win
}

main "$@"
