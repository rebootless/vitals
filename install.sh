#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

RED='\033[0;31m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

REPO_URL="https://github.com/VargKernel/vitals.git"
INSTALL_BIN="/usr/local/bin/vitals"
BUILD_DIR="$(mktemp -d /tmp/vitals-build.XXXXXX)"

trap 'rm -rf "$BUILD_DIR"' EXIT

if [[ $EUID -eq 0 ]]; then
    echo -e "${RED}[!] Do not run this script as root.${NC}"
    echo "[i] Run it as a regular user — sudo will be called where needed."
    exit 1
fi

if ! sudo -n true 2>/dev/null && ! sudo -v 2>/dev/null; then
    echo -e "${RED}[!] sudo access is required. Aborting.${NC}"
    exit 1
fi

echo ""
echo "[i] Build directory : $BUILD_DIR"
echo "[i] Install target  : $INSTALL_BIN"
echo ""

if command -v vitals &>/dev/null; then
    EXISTING="$(command -v vitals)"
    echo -e "${YELLOW}[!] vitals is already installed at: ${EXISTING}${NC}"
    read -rp "[?] Reinstall? [y/N]: " CHOICE
    case "${CHOICE,,}" in
        y|yes) echo "[i] Proceeding with reinstall." ;;
        *)     echo "[i] Aborted."; exit 0 ;;
    esac
    echo ""
fi

sudo apt-get update -q
sudo apt-get install -y --no-install-recommends \
    git \
    build-essential \
    cmake \
    pkg-config \
    pandoc \
    wget \
    file \
    libncurses-dev \
    libncursesw5-dev \
    libtinfo-dev \
    libunistring-dev \
    libutf8proc-dev \
    libdeflate-dev \
    libavcodec-dev \
    libavdevice-dev \
    libavformat-dev \
    libswscale-dev \
    libavutil-dev \
    libudev-dev \
    libjpeg-dev \
    libpng-dev \
    libx11-dev \
    libxkbcommon-dev \
    libcap-dev \
    zlib1g-dev \
    libqrcodegen-dev

if ! sudo apt-get install -y --no-install-recommends libstb-dev 2>/dev/null; then
    echo "[i] libstb-dev not found — downloading stb_image.h directly..."
    sudo mkdir -p /usr/local/include/stb
    sudo wget -qO /usr/local/include/stb/stb_image.h \
        https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
fi

if ! sudo apt-get install -y --no-install-recommends doctest-dev 2>/dev/null; then
    echo "[i] doctest-dev not found — downloading doctest.h directly..."
    sudo mkdir -p /usr/local/include/doctest
    sudo wget -qO /usr/local/include/doctest/doctest.h \
        https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
fi

echo "[+] Dependencies installed."
echo ""

export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib:${LD_LIBRARY_PATH:-}"

if pkg-config --exists notcurses 2>/dev/null; then
    NC_VER="$(pkg-config --modversion notcurses)"
    echo "[i] notcurses ${NC_VER} already installed — skipping build."
    echo ""
else
    echo "[i] notcurses not found — building from source."
    echo ""

    cd "$BUILD_DIR"
    git clone --depth 1 https://github.com/dankamongmen/notcurses.git
    cd notcurses
    git submodule update --init --recursive

    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DUSE_PANDOC=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local

    cmake --build build --parallel "$(nproc)"
    sudo cmake --install build

    echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/usr_local_lib.conf > /dev/null
    sudo ldconfig

    echo "[+] notcurses installed."
    echo ""
fi

cd "$BUILD_DIR"
git clone --depth 1 "$REPO_URL" vitals
cd vitals

echo "[+] Repository cloned."
echo ""

cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_PANDOC=OFF

cmake --build build --parallel "$(nproc)"

if [[ ! -f build/vitals ]]; then
    echo -e "${RED}[!] Build failed — binary not found.${NC}"
    exit 1
fi

echo "[+] Build successful."
echo ""

sudo install -m 755 build/vitals "$INSTALL_BIN"

echo "[+] Binary installed to $INSTALL_BIN"
echo ""

echo "[SUCCESS]"
echo "Binary  : $INSTALL_BIN"
echo "Version : $(git -C "$BUILD_DIR/vitals" rev-parse --short HEAD 2>/dev/null || echo 'unknown')"
echo ""
echo "[i] Run with: vitals"
echo ""
