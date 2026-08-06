<h1 align="center">Vitals</h1>
<h3 align="center">A terminal resource monitor for Linux built with notcurses.</h3>

<div align="center">

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey?logo=linux&logoColor=white)](https://kernel.org)
[![CMake](https://img.shields.io/badge/CMake-build-064F8C?logo=cmake)](https://cmake.org/)
[![notcurses](https://img.shields.io/badge/notcurses-lightgrey)](https://github.com/dankamongmen/notcurses)

[![Debian 12 (Bookworm)](https://github.com/rebootless/vitals/actions/workflows/build-debian-12.yml/badge.svg)](https://github.com/rebootless/vitals/actions/workflows/build-debian-12.yml)
[![Debian 13 (Trixie)](https://github.com/rebootless/vitals/actions/workflows/build-debian-13.yml/badge.svg)](https://github.com/rebootless/vitals/actions/workflows/build-debian-13.yml)
[![Ubuntu 22.04 (Jammy)](https://github.com/rebootless/vitals/actions/workflows/build-ubuntu-22.04.yml/badge.svg)](https://github.com/rebootless/vitals/actions/workflows/build-ubuntu-22.04.yml)
[![Ubuntu 24.04 (Noble)](https://github.com/rebootless/vitals/actions/workflows/build-ubuntu-24.04.yml/badge.svg)](https://github.com/rebootless/vitals/actions/workflows/build-ubuntu-24.04.yml)

</div>

<table align="center">
  <tr>
    <td width="50%">
      <img src="screenshots/2026-08-06_06-39.png" width="100%" alt="Overview">
    </td>
    <td width="50%">
      <img src="screenshots/2026-08-06_06-40.png" width="100%" alt="Settings">
    </td>
  </tr>
</table>

## ⚡ Installation

### Quick Install

Install the latest version with a single command:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/rebootless/vitals/main/install.sh)
```

The installer automatically:

* Installs the required build dependencies
* Clones the notcurses source
* Builds the project
* Installs `vitals` to `/usr/local/bin`
* Installs the required libraries to `/usr/local/lib`

Safe to run multiple times.

<details>
<summary align="center"><b>Build from Source</b> <i>(click to expand)</i></summary>

Clone the repository:

```bash
git clone https://github.com/rebootless/vitals.git
cd vitals
````

Prepare the build environment:

```bash
chmod +x setup.sh
./setup.sh
```

Build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF
cmake --build build -j$(nproc)
```

Install:

```bash
sudo cmake --install build
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/usr_local_lib.conf
sudo ldconfig
```

</details>

<details>
<summary align="center"><b>Run Without Installing</b> <i>(click to expand)</i></summary>

```bash
git clone https://github.com/rebootless/vitals.git
cd vitals

chmod +x setup.sh
./setup.sh

cmake -B build -DCMAKE_BUILD_TYPE=Release -DUSE_PANDOC=OFF
cmake --build build -j$(nproc)

LD_LIBRARY_PATH="$(pwd)/build/notcurses" ./build/vitals
```

</details>

## 📊 Panels

| Panel | Data source | What it shows |
| :--- | :--- | :--- |
| **CPU** | `/proc/stat`, cpufreq | CPU usage, per-core activity, frequencies, history |
| **GPU** | DRM sysfs, hwmon, `nvidia-smi` | Utilization, VRAM, temperature, power |
| **Memory** | `/proc/meminfo` | RAM and swap usage |
| **Network** | `/proc/net/dev` | Per-interface RX/TX throughput |
| **Storage** | `/proc/diskstats`, `statvfs` | Filesystem usage and disk I/O |
| **Thermal** | `/sys/class/thermal`, `/sys/class/hwmon` | CPU, GPU, and motherboard sensors |

> [!NOTE]
> The GPU panel is displayed only when a supported device is detected. Otherwise, the CPU panel expands to use the available space.

## ⌨️ Controls

| Key | Action |
| :--- | :--- |
| `q` | Quit |
| `Esc` | Open the settings menu |
| `Tab` | Select the next option |
| `↑` / `↓` | Change the selected value |
| `Enter` | Save changes and close |
| `Esc` *(in settings)* | Discard changes and close |

## 📋 Requirements

- Linux (kernel ≥ 4.x)
- GCC or Clang with C++20 support
- CMake ≥ 3.14
- Internet access (required to download notcurses during setup)
- Debian 12 / 13 or Ubuntu 22.04 / 24.04

## 📄 License

<p align="center">
  Licensed under the <strong>GNU General Public License v3.0</strong>. See the <a href="LICENSE">LICENSE</a> file for details.
</p>
