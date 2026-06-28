# Vitals

[![Debian 12](https://github.com/VargKernel/vitals/actions/workflows/build-debian12.yml/badge.svg)](https://github.com/VargKernel/vitals/actions/workflows/build-debian-12.yml)
[![Debian 13](https://github.com/VargKernel/vitals/actions/workflows/build-debian13.yml/badge.svg)](https://github.com/VargKernel/vitals/actions/workflows/build-debian-13.yml)
[![Ubuntu 22.04](https://github.com/VargKernel/vitals/actions/workflows/build-ubuntu2204.yml/badge.svg)](https://github.com/VargKernel/vitals/actions/workflows/build-ubuntu-22.04.yml)
[![Ubuntu 24.04](https://github.com/VargKernel/vitals/actions/workflows/build-ubuntu2404.yml/badge.svg)](https://github.com/VargKernel/vitals/actions/workflows/build-ubuntu-24.04.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey?logo=linux&logoColor=white)](https://kernel.org)

A terminal resource monitor for Linux built with [notcurses](https://github.com/dankamongmen/notcurses). Displays CPU, memory, network, storage, and thermal data in a responsive multi-panel TUI with 24-bit color using the [Catppuccin Mocha](https://github.com/catppuccin/catppuccin) palette.

## Panels

| Panel | Data source | What it shows |
|---|---|---|
| **CPU**     | `/proc/stat` | Overall % + per-core bars + sparkline history + frequencies |
| **Memory**  | `/proc/meminfo` | RAM and swap usage with gradient bars |
| **Network** | `/proc/net/dev` | Per-interface RX/TX throughput with peak tracking |
| **Storage** | `/proc/diskstats`, `statvfs` | Root filesystem bar + per-disk I/O |
| **Thermal** | `/sys/class/thermal`, `/sys/class/hwmon` | Thermal zones + hwmon sensors (coretemp, k10temp) |

The layout adapts automatically to the terminal width: three-column wide view (≥ 130 cols), two-column medium view (≥ 80 cols), and a compact stacked view for narrow terminals.

## Installation

### Quick install (recommended)

Downloads, builds, and installs the binary to `/usr/local/bin` in one step.

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/VargKernel/vitals/main/install.sh)
```

Or clone first and run locally:

```bash
git clone https://github.com/VargKernel/vitals.git
bash vitals/install.sh
```

The script handles everything: apt dependencies, building notcurses from source, compiling vitals, and placing the binary in PATH. It is idempotent — safe to re-run.

### Manual install

Follow these steps if you prefer full control or are integrating into an existing workflow.

**1. Install build dependencies**

```bash
sudo apt-get update
sudo apt-get install -y \
    git build-essential cmake pkg-config pandoc wget file \
    libncurses-dev libncursesw5-dev libtinfo-dev \
    libunistring-dev libutf8proc-dev libdeflate-dev \
    libavcodec-dev libavdevice-dev libavformat-dev \
    libswscale-dev libavutil-dev libudev-dev \
    libjpeg-dev libpng-dev libx11-dev libxkbcommon-dev \
    libcap-dev zlib1g-dev libqrcodegen-dev \
    libstb-dev doctest-dev
```

> `libstb-dev` and `doctest-dev` may not be available on older distributions. The installer will download the headers directly as a fallback.

**2. Build and install notcurses**

```bash
git clone --depth 1 https://github.com/dankamongmen/notcurses.git
cd notcurses
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF
cmake --build build -j$(nproc)
sudo cmake --install build
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/usr_local_lib.conf
sudo ldconfig
cd ..
```

**3. Clone and build vitals**

```bash
git clone https://github.com/VargKernel/vitals.git
cd vitals
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF
cmake --build build -j$(nproc)
```

**4. Install the binary**

```bash
sudo install -m 755 build/vitals /usr/local/bin/vitals
```

Verify:

```bash
vitals --version 2>/dev/null || vitals
```

### Manual build

Build and run in place without installing anything system-wide.

```bash
git clone --depth 1 https://github.com/dankamongmen/notcurses.git
cd notcurses && git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF
cmake --build build -j$(nproc) && sudo cmake --install build && sudo ldconfig
cd ..

git clone https://github.com/VargKernel/vitals.git && cd vitals
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF
cmake --build build -j$(nproc)

./build/vitals
```

## Usage

```bash
vitals
```

| Key | Action |
|-----|--------|
| `q` | Quit   |

## Requirements

- Linux (kernel ≥ 4.x)
- GCC or Clang with C++20 support
- CMake ≥ 3.14
- notcurses (built from source — see above)
- Debian 12 / 13 or Ubuntu 22.04 / 24.04 (other distros untested)

## License

[GNU General Public License v3.0](LICENSE)
