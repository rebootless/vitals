#!/bin/bash

# Installs vitals from source to /usr/local/bin.
# Workflow: installs apt deps -> clones vitals + notcurses -> builds -> installs binary + libs.
# Requirements: Debian/Ubuntu, internet access, sudo privileges.

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

REPO_URL="https://github.com/rebootless/vitals.git"
INSTALL_BIN="/usr/local/bin/vitals"
BUILD_DIR="$(mktemp -d /tmp/vitals-build.XXXXXX)"

# Cleanup temp build dir on exit (success or failure).
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

# setup.sh installs apt deps and clones notcurses as a subdirectory.
# Must run from inside the vitals repo root.
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

# cmake --install handles both the vitals binary and notcurses shared libs.
# It strips the build-tree RPATH so the installed binary uses the system linker.
sudo cmake --install build

# Ensure /usr/local/lib is registered so notcurses.so is found at runtime.
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/usr_local_lib.conf > /dev/null
sudo ldconfig

echo ""
echo "Binary installed to $INSTALL_BIN"

echo ""
echo "  Binary: $INSTALL_BIN"
echo "  Commit: $(git -C "$BUILD_DIR/vitals" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"

echo ""
echo "Run:"
echo "  vitals"
