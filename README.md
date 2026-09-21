<h1 align="center">Vitals</h1>
<h3 align="center">A terminal resource monitor for Linux built with notcurses.</h3>

<div align="center">

[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?logo=linux&logoColor=white)](https://kernel.org)
[![CMake](https://img.shields.io/badge/CMake-064F8C?logo=cmake)](https://cmake.org/)
[![Notcurses](https://img.shields.io/badge/Notcurses-555?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAMAAADXqc3KAAAAP1BMVEUAAAD///////////////////////////////8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwlEnmAAAAEHRSTlMABvstUY9uss4AAAAAAAAAa/L5DwAAAJNJREFUeNptUVsOwzAIizEm3P/EI800rQl8tbH8wIzxG4x2AIgtCFh0wGLoBjAQOecF1L+bWTYy09bUB96E/W68lNwfSORlH5tj+c4McBGUcVK2TaDZj1FqfsT6BnBrdi+Oakk0rQBZWl0rdKVOj9WKHNRNiGRF88Nk1Q7WBXleEYhSqswXMFwMsktF6SE11/0v8ANdPQImejX+FwAAAABJRU5ErkJggg==)](https://github.com/dankamongmen/notcurses)

</div>

## Build Status

[![Debian 12](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Debian%2012%20%28Bookworm%29&label=Debian%2012)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Debian 13](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Debian%2013%20%28Trixie%29&label=Debian%2013)](https://github.com/rebootless/vitals/actions/workflows/build.yml)\
[![Ubuntu 22.04](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Ubuntu%2022.04%20%28Jammy%29&label=Ubuntu%2022.04)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Ubuntu 24.04](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Ubuntu%2024.04%20%28Noble%29&label=Ubuntu%2024.04)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Ubuntu 26.04](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Ubuntu%2026.04%20%28Resolute%29&label=Ubuntu%2026.04)](https://github.com/rebootless/vitals/actions/workflows/build.yml)\
[![Fedora 43](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Fedora%2043&label=Fedora%2043)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Fedora 44](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Fedora%2044&label=Fedora%2044)](https://github.com/rebootless/vitals/actions/workflows/build.yml)\
[![AlmaLinux 8](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=AlmaLinux%208%20%28RHEL%208%29&label=AlmaLinux%208)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![AlmaLinux 9](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=AlmaLinux%209%20%28RHEL%209%29&label=AlmaLinux%209)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![AlmaLinux 10](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=AlmaLinux%2010%20%28RHEL%2010%29&label=AlmaLinux%2010)](https://github.com/rebootless/vitals/actions/workflows/build.yml)\
[![openSUSE Tumbleweed](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=openSUSE%20Tumbleweed&label=openSUSE%20Tumbleweed)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![openSUSE Leap 16.0](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=openSUSE%20Leap%2016.0&label=openSUSE%20Leap%2016.0)](https://github.com/rebootless/vitals/actions/workflows/build.yml)\
[![Arch Linux](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Arch%20Linux&label=Arch%20Linux)](https://github.com/rebootless/vitals/actions/workflows/build.yml)

<p align="center">
  <img src="screenshots/2026-08-06_06-39.png" width="100%" alt="Overview">
</p>

<p align="center">
  <img src="screenshots/2026-08-06_06-40.png" width="100%" alt="Settings">
</p>

## ⚡ Installation

### Quick Install

Install the latest version with a single command:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/rebootless/vitals/main/install.sh)
```

The installer automatically:

* Installs the required build dependencies
* Clones the notcurses source (release pinned by `NOTCURSES_VERSION` in `setup.sh`)
* Builds the project
* Installs `vitals` to `/usr/local/bin`
* Installs the required libraries under `/usr/local` and registers them with the dynamic linker

Safe to run multiple times.

### Quick Uninstall

Remove `vitals` and the notcurses files installed alongside it with a single command:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/rebootless/vitals/main/uninstall.sh)
```

The uninstaller:

* Reads the manifest left by `install.sh` and removes every file it installed (binary, notcurses libraries/headers/pkgconfig files)
* Prunes any directories left empty
* Removes the `/etc/ld.so.conf.d` entry added for the notcurses libraries and refreshes the linker cache
* Asks before deleting your saved config at `~/.config/vitals`

If `vitals` was installed a different way (no manifest present), it falls back to a best-effort removal of the vitals binary and matching notcurses libraries.

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
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Install:

```bash
sudo cmake --install build
dirname "$(grep -m1 'libnotcurses\.so' build/install_manifest.txt)" | sudo tee /etc/ld.so.conf.d/usr_local_lib.conf
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

cmake -B build -DCMAKE_BUILD_TYPE=Release
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

## 🧪 Self-test

```bash
vitals --self-test
```

Checks, without a terminal, that every data source vitals reads (CPU, memory, GPU, storage, network, thermal sensors, system info) is available and returns sane values. It prints one line per component — `OK`, `SKIP` (optional hardware that isn't present, e.g. no GPU) or `FAIL` — and exits with `0` unless something failed, so it can be used in CI.

## 📋 Requirements

- Linux (kernel ≥ 4.x)
- GCC or Clang with C++20 support
- CMake ≥ 3.21
- Internet access (required to download notcurses during setup)

## 📄 License

<p align="center">
  Licensed under the <strong>GNU General Public License v3.0</strong>. See the <a href="LICENSE">LICENSE</a> file for details.
</p>
