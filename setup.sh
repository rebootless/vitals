#!/bin/bash

# Prepares the vitals build environment.
# Workflow: installs build deps (deps.sh), clones notcurses into the repo as a subdirectory.
# Requirements: run from the vitals repo root, internet access, sudo privileges.

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $EUID -eq 0 ]]; then
    echo "Do not run this script as root."
    exit 1
fi

bash "$SCRIPT_DIR/deps.sh"

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
