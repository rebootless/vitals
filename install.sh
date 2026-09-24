#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

REPO_URL="https://github.com/rebootless/vitals.git"
INSTALL_BIN="/usr/local/bin/vitals"
BUILD_DIR="$(mktemp -d /tmp/vitals-build.XXXXXX)"

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

cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_PANDOC=OFF

cmake --build build --parallel "$(nproc)"

if [[ ! -f build/vitals ]]; then
    echo "Build failed — binary not found."
    exit 1
fi

echo ""
echo "Build successful."

# cmake --install strips the build-tree RPATH
sudo cmake --install build

# register the notcurses lib dir (lib or lib64); musl has no ldconfig
LDCONF_WRITTEN=0
if command -v ldconfig &>/dev/null && [[ -d /etc/ld.so.conf.d ]]; then
    LIBDIR="$(dirname "$(grep -m1 'libnotcurses\.so' build/install_manifest.txt)")"
    echo "$LIBDIR" | sudo tee /etc/ld.so.conf.d/usr_local_lib.conf > /dev/null
    sudo ldconfig
    LDCONF_WRITTEN=1
fi

# manifest lets uninstall.sh remove exactly what was installed
MANIFEST_DIR="/usr/local/share/vitals"
sudo mkdir -p "$MANIFEST_DIR"
sudo cp "build/install_manifest.txt" "$MANIFEST_DIR/install_manifest.txt"
if [[ $LDCONF_WRITTEN -eq 1 ]]; then
    echo "/etc/ld.so.conf.d/usr_local_lib.conf" | sudo tee -a "$MANIFEST_DIR/install_manifest.txt" > /dev/null
fi

echo ""
echo "Binary installed to $INSTALL_BIN"

echo ""
echo "  Binary: $INSTALL_BIN"
echo "  Commit: $(git -C "$BUILD_DIR/vitals" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

echo ""
echo "Run:"
echo "  vitals"