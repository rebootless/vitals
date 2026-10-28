#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

INSTALL_BIN="/usr/local/bin/vitals"
DATA_DIR="/usr/local/share/vitals"

CONFIG_ROOT="$HOME/.config"
if [[ "${XDG_CONFIG_HOME:-}" == /* ]]; then CONFIG_ROOT="$XDG_CONFIG_HOME"; fi
CONFIG_DIR="$CONFIG_ROOT/vitals"

if [[ $EUID -eq 0 ]]; then
    echo "Do not run this script as root."
    exit 1
fi

if ! sudo -n true 2>/dev/null && ! sudo -v 2>/dev/null; then
    echo "sudo access is required. Aborting."
    exit 1
fi

if [[ ! -e "$INSTALL_BIN" && ! -d "$DATA_DIR" ]]; then
    echo "vitals does not appear to be installed."
    exit 0
fi

echo ""
echo "  Binary: $INSTALL_BIN"
echo "  Themes: $DATA_DIR/themes"
echo ""
read -rp "Remove them? [y/N]: " CHOICE
case "${CHOICE,,}" in
    y|yes) ;;
    *)     echo "Aborted."; exit 0 ;;
esac

sudo rm -f "$INSTALL_BIN"
sudo rm -rf "$DATA_DIR/themes"
sudo rmdir --ignore-fail-on-non-empty "$DATA_DIR" 2>/dev/null || true
echo "Removed."

echo ""
if [[ -d "$CONFIG_DIR" ]]; then
    read -rp "Also remove user config and themes at $CONFIG_DIR? [y/N]: " CHOICE
    case "${CHOICE,,}" in
        y|yes) rm -rf "$CONFIG_DIR"; echo "Removed $CONFIG_DIR." ;;
        *)     echo "Keeping $CONFIG_DIR." ;;
    esac
fi

echo ""
if command -v vitals &>/dev/null; then
    echo "Warning: 'vitals' is still on PATH at $(command -v vitals)."
    echo "It may have been installed from a different location."
else
    echo "vitals has been uninstalled."
fi
