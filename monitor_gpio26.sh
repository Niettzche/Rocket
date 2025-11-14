#!/usr/bin/env bash

set -euo pipefail

INTERVAL="${1:-1}"

if ! command -v gpio >/dev/null 2>&1; then
    echo "El comando 'gpio' no está disponible en el sistema." >&2
    exit 1
fi

trim() {
    local trimmed="${1#"${1%%[![:space:]]*}"}"
    trimmed="${trimmed%"${trimmed##*[![:space:]]}"}"
    printf '%s' "$trimmed"
}

read_gpio26_state() {
    local voltage
    voltage=$(gpio readall | awk -F'|' '
        function trim(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
        {
            left_bcm = trim($2)
            right_bcm = trim($13)
            if (left_bcm == "26") {
                print trim($6)
                exit
            }
            if (right_bcm == "26") {
                print trim($9)
                exit
            }
        }
    ')

    if [[ -z "${voltage:-}" ]]; then
        echo "UNKNOWN"
    elif [[ "$voltage" == "1" || "$voltage" == "HIGH" ]]; then
        echo "HIGH"
    elif [[ "$voltage" == "0" || "$voltage" == "LOW" ]]; then
        echo "LOW"
    else
        echo "UNKNOWN"
    fi
}

state_changed=0
initial_state=""
current_state=""

print_summary() {
    echo
    if [[ -z "$initial_state" ]]; then
        echo "No fue posible leer el estado inicial del GPIO26."
    else
        echo "Estado inicial: $initial_state."
    fi

    if (( state_changed )); then
        echo "El GPIO26 cambió de voltaje durante la monitorización."
    else
        echo "El GPIO26 no presentó cambios de voltaje."
    fi
}

trap 'print_summary; exit 0' INT TERM

echo "Monitoreando GPIO26 cada ${INTERVAL}s. Presiona Ctrl+C para terminar."

while true; do
    new_state=$(read_gpio26_state)

    if [[ -z "$initial_state" ]]; then
        initial_state="$new_state"
        if [[ "$initial_state" == "HIGH" ]]; then
            echo "El GPIO26 ya estaba entregando voltaje (HIGH)."
        elif [[ "$initial_state" == "LOW" ]]; then
            echo "El GPIO26 inició en LOW (sin voltaje)."
        else
            echo "No se pudo determinar el estado inicial del GPIO26."
        fi
    fi

    if [[ "$current_state" != "$new_state" && -n "$current_state" ]]; then
        if [[ "$new_state" == "UNKNOWN" ]]; then
            echo "El estado del GPIO26 no se pudo leer."
        else
            state_changed=1
            echo "Cambio detectado: GPIO26 ahora está en $new_state."
        fi
    fi

    current_state="$new_state"
    sleep "$INTERVAL"
done
