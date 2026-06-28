#!/bin/bash
set -e

sudo apt update
sudo apt install -y \
    git build-essential cmake pkg-config pandoc\
    libncurses-dev libunistring-dev libutf8proc-dev \
    libdeflate-dev libqrcodegen-dev libstb-dev \
    libavcodec-dev libavdevice-dev libavformat-dev libswscale-dev \
    libavutil-dev libudev-dev doctest-dev

git clone https://github.com/dankamongmen/notcurses.git
cd notcurses
git submodule update --init --recursive

cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DUSE_PANDOC=OFF

cmake --build build -j$(nproc)

sudo cmake --install build
sudo ldconfig
