#!/bin/bash
# =============================================================
#  V2X — Install & Enable All systemd Services
#  Run this script once on the Raspberry Pi.
#  After that, every process starts automatically on every boot.
#
#  Usage:
#      chmod +x install_services.sh
#      sudo ./install_services.sh
# =============================================================

set -e   # stop immediately if any command fails

# ── 1. Make sure the script is running as root ──────────────
if [ "$EUID" -ne 0 ]; then
    echo "❌  Run with sudo:  sudo ./install_services.sh"
    exit 1
fi

# ── 2. Code directory (change if your code lives elsewhere) ─
V2X_DIR="/home/rpi/V2X-Collision-Avoidance/RPI"

echo "=================================================="
echo "  V2X systemd Services Installer"
echo "  Code directory: $V2X_DIR"
echo "=================================================="

# ── 3. Make sure the directory exists ────────────────────────
if [ ! -d "$V2X_DIR" ]; then
    echo "❌  Directory $V2X_DIR not found."
    echo "    Create it first:  mkdir -p $V2X_DIR"
    echo "    Then copy all .py files and the html/js folder into it."
    exit 1
fi

# ── 4. Make sure all required files are present ─────────────
REQUIRED_FILES=(
    "hub.py"
    "dashboard_bridge.py"
    "Car_client.py"
    "V2P.py"
    "server.py"
    "ipc_node.py"
)

MISSING=0
for f in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$V2X_DIR/$f" ]; then
        echo "⚠️   Missing: $V2X_DIR/$f"
        MISSING=1
    fi
done

if [ "$MISSING" -eq 1 ]; then
    echo "❌  Copy the missing files to $V2X_DIR first, then re-run."
    exit 1
fi

echo "✅  All required Python files found."

# ── 5. Copy the service files to systemd ─────────────────────
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SERVICE_FILES=(
    "v2x-hub.service"
    "v2x-dashboard-bridge.service"
    "v2x-car-client.service"
    "v2x-v2p.service"
    "v2x-server.service"
)

echo ""
echo "📋  Copying service files to /etc/systemd/system/ ..."
for svc in "${SERVICE_FILES[@]}"; do
    src="$SCRIPT_DIR/$svc"
    if [ ! -f "$src" ]; then
        echo "❌  Service file not found: $src"
        exit 1
    fi
    cp "$src" "/etc/systemd/system/$svc"
    echo "    ✅  $svc"
done

# ── 6. Reload systemd ─────────────────────────────────────────
echo ""
echo "🔄  Reloading systemd daemon ..."
systemctl daemon-reload

# ── 7. Enable services (auto-start on every boot) ────────────
echo ""
echo "🔗  Enabling services (auto-start on boot) ..."
for svc in "${SERVICE_FILES[@]}"; do
    systemctl enable "$svc"
    echo "    ✅  enabled: $svc"
done

# ── 8. Start them now, in the correct order ──────────────────
echo ""
echo "🚀  Starting services in order ..."

start_and_check() {
    local svc="$1"
    local wait_sec="${2:-2}"
    systemctl start "$svc"
    sleep "$wait_sec"
    if systemctl is-active --quiet "$svc"; then
        echo "    ✅  running: $svc"
    else
        echo "    ❌  FAILED: $svc"
        echo "        → Check logs: sudo journalctl -u $svc -n 30"
    fi
}

start_and_check "v2x-hub.service"               3
start_and_check "v2x-dashboard-bridge.service"  3
start_and_check "v2x-car-client.service"        4
start_and_check "v2x-v2p.service"               4
start_and_check "v2x-server.service"            2

# ── 9. Status summary ─────────────────────────────────────────
echo ""
echo "=================================================="
echo "  STATUS SUMMARY"
echo "=================================================="
for svc in "${SERVICE_FILES[@]}"; do
    status=$(systemctl is-active "$svc" 2>/dev/null || echo "unknown")
    if [ "$status" = "active" ]; then
        icon="✅"
    else
        icon="❌"
    fi
    printf "  %s  %-35s  %s\n" "$icon" "$svc" "$status"
done

echo ""
echo "=================================================="
echo "  Dashboard URL:  http://$(hostname -I | awk '{print $1}'):8000"
echo "=================================================="
echo ""
echo "📌  Useful commands:"
echo "    sudo systemctl status v2x-hub              # check hub"
echo "    sudo journalctl -u v2x-car-client -f       # live logs"
echo "    sudo systemctl restart v2x-v2p             # restart V2P"
echo "    sudo systemctl stop v2x-hub                # stop all (hub stop = cascade)"
echo ""
