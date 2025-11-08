#!/usr/bin/env bash
set -euo pipefail

SERVICE_NAME="read_sensors.service"
SYSTEMD_DIR="/etc/systemd/system"
ENABLE_AND_START=true

print_usage() {
    cat <<'EOF'
Usage: sudo ./setup_read_sensors_service.sh [options]

Options:
  --user <name>        Run the service as this user (auto-detected when omitted)
  --workdir <path>     Directory that hosts read_sensors.py (defaults to repo root)
  --service-name <id>  Override the systemd service name (default: read_sensors.service)
  --no-start           Install and enable the unit but do not start it yet
  -h, --help           Show this help and exit

Run this script with sudo/root because it writes into /etc/systemd/system and invokes systemctl.
EOF
}

require_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "This script must be run as root (use sudo)." >&2
        exit 1
    fi
}

detect_repo_root() {
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    echo "$script_dir"
}

detect_default_user() {
    local target_dir="$1"
    if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
        echo "${SUDO_USER}"
        return
    fi
    stat -c '%U' "$target_dir"
}

WORKING_DIR_OVERRIDE=""
RUN_USER_OVERRIDE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --user)
            RUN_USER_OVERRIDE="$2"
            shift 2
            ;;
        --workdir)
            WORKING_DIR_OVERRIDE="$2"
            shift 2
            ;;
        --service-name)
            SERVICE_NAME="$2"
            shift 2
            ;;
        --no-start)
            ENABLE_AND_START=false
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            print_usage
            exit 1
            ;;
    esac
done

require_root

REPO_ROOT="$(detect_repo_root)"
WORKING_DIR="${WORKING_DIR_OVERRIDE:-$REPO_ROOT}"

if [[ ! -d "$WORKING_DIR" ]]; then
    echo "Working directory '$WORKING_DIR' does not exist." >&2
    exit 1
fi

RUN_USER="${RUN_USER_OVERRIDE:-$(detect_default_user "$WORKING_DIR")}"

if ! id "$RUN_USER" >/dev/null 2>&1; then
    echo "User '$RUN_USER' was not found on this system." >&2
    exit 1
fi

RUN_GROUP="$(id -gn "$RUN_USER")"
READ_SENSORS_PATH="$WORKING_DIR/read_sensors.py"
LORA_PATH="$WORKING_DIR/external/LoRa-RaspberryPi"

if [[ ! -f "$READ_SENSORS_PATH" ]]; then
    echo "Could not find read_sensors.py inside '$WORKING_DIR'." >&2
    exit 1
fi

if [[ ! -d "$LORA_PATH" ]]; then
    echo "Warning: Expected LoRa path '$LORA_PATH' not found. The service will still be created." >&2
fi

SERVICE_TARGET_PATH="$SYSTEMD_DIR/$SERVICE_NAME"
TMP_SERVICE="$(mktemp)"

cat <<EOF >"$TMP_SERVICE"
[Unit]
Description=Read sensor data and forward payloads over LoRa
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=$RUN_USER
Group=$RUN_GROUP
WorkingDirectory=$WORKING_DIR
Environment="PYTHONUNBUFFERED=1"
Environment="PYTHONPATH=$LORA_PATH:\$PYTHONPATH"
ExecStart=/usr/bin/env python3 -u $READ_SENSORS_PATH
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

install -m 0644 "$TMP_SERVICE" "$SERVICE_TARGET_PATH"
rm -f "$TMP_SERVICE"

systemctl daemon-reload
systemctl enable "$SERVICE_NAME"

if $ENABLE_AND_START; then
    systemctl restart "$SERVICE_NAME"
    systemctl status "$SERVICE_NAME" --no-pager
else
    echo "Service installed and enabled. Start it later with: sudo systemctl start $SERVICE_NAME"
fi

echo
echo "Installed $SERVICE_NAME for user '$RUN_USER' with working directory '$WORKING_DIR'."
