#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

if [[ -d build ]]; then
    echo "Removing old build directory..."
    rm -rf build
fi

echo "Configuring..."
cmake -B build

echo "Building..."
cmake --build build -j"$(nproc)"

echo "Done."
