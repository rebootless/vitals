#!/bin/bash

# Prepares the vitals build environment.
# Workflow: installs apt deps, clones notcurses into the repo as a subdirectory.
# Requirements: run from the vitals repo root, internet access, sudo privileges.

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $EUID -eq 0 ]]; then
    echo "Do not run this script as root."
    exit 1
fi

sudo apt-get update -q
sudo apt-get install -y --no-install-recommends \
    git \
    build-essential \
    cmake \
    pkg-config \
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
    echo "libstb-dev not found — downloading stb_image.h directly..."
    sudo mkdir -p /usr/local/include/stb
    sudo wget -qO /usr/local/include/stb/stb_image.h \
        https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
fi

if ! sudo apt-get install -y --no-install-recommends doctest-dev 2>/dev/null; then
    echo "doctest-dev not found — downloading doctest.h directly..."
    sudo mkdir -p /usr/local/include/doctest
    sudo wget -qO /usr/local/include/doctest/doctest.h \
        https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
fi

echo ""
echo "Dependencies installed."

if [[ -d "$SCRIPT_DIR/notcurses" ]]; then
    echo "notcurses already present — skipping clone."
else
    git clone --depth 1 https://github.com/dankamongmen/notcurses.git \
        "$SCRIPT_DIR/notcurses"
    echo "notcurses cloned."
fi

echo "Updating notcurses submodules..."
git -C "$SCRIPT_DIR/notcurses" submodule update --init --recursive
echo "Submodules ready."

echo ""
echo "  Repo:      $SCRIPT_DIR"
echo "  notcurses: $SCRIPT_DIR/notcurses"

echo ""
echo "Build vitals:"
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF"
echo "  cmake --build build -j\$(nproc)"
