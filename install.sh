#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

REPO_URL="https://github.com/rebootless/vitals.git"
INSTALL_BIN="/usr/local/bin/vitals"
BUILD_DIR="$(mktemp -d /tmp/vitals-build.XXXXXX)"

# only for summary - not used actually
SETTINGS_DIR="${HOME}/.config/vitals/config"
USER_THEMES_DIR="${HOME}/.config/vitals/themes"
BUNDLED_THEMES_DIR="/usr/local/share/vitals/themes"

trap 'rm -rf "$BUILD_DIR"' EXIT

if [[ $EUID -eq 0 ]]; then
    echo "Do not run this script as root."
    exit 1
fi

if ! sudo -n true 2>/dev/null && ! sudo -v 2>/dev/null; then
    echo "sudo access is required. Aborting."
    exit 1
fi

echo ""
echo "  Build directory: $BUILD_DIR"
echo "  Install target:  $INSTALL_BIN"
echo ""

if command -v vitals &>/dev/null; then
    EXISTING="$(command -v vitals)"
    echo "vitals is already installed at: ${EXISTING}"
    read -rp "Reinstall? [y/N]: " CHOICE
    case "${CHOICE,,}" in
        y|yes) echo "Proceeding with reinstall..." ;;
        *)     echo "Aborted."; exit 0 ;;
    esac
    echo ""
fi

git clone --depth 1 "$REPO_URL" "$BUILD_DIR/vitals"
echo "vitals cloned."
echo ""

bash "$BUILD_DIR/vitals/setup.sh"

cd "$BUILD_DIR/vitals"

bash build.sh

if [[ ! -f build/vitals ]]; then
    echo "Build failed — binary not found."
    exit 1
fi

echo ""
echo "Build successful."

# the binary and themes only: notcurses is linked statically
sudo cmake --install build

echo ""
echo "  Binary: $INSTALL_BIN"
echo "  Commit: $(git -C "$BUILD_DIR/vitals" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

echo "  Settings:       $SETTINGS_DIR"
echo "  User themes:    $USER_THEMES_DIR"
echo "  Bundled Themes: $BUNDLED_THEMES_DIR"

echo ""
echo "Run:"
echo "  vitals"
echo "  vitals --help"
