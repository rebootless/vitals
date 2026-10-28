<h1 align="center">Vitals</h1>
<h3 align="center">A terminal resource monitor for Linux built with notcurses.</h3>

<div align="center">

[![Bash](https://img.shields.io/badge/Bash-5.0%2B-4EAA25?logo=gnubash&logoColor=white)](https://www.gnu.org/software/bash/)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey?logo=linux&logoColor=white)](https://kernel.org)
[![CMake](https://img.shields.io/badge/CMake-064F8C?logo=cmake)](https://cmake.org/)
[![Notcurses](https://img.shields.io/badge/Notcurses-555?logo=data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAMAAADXqc3KAAAAP1BMVEUAAAD///////////////////////////////8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAwlEnmAAAAEHRSTlMABvstUY9uss4AAAAAAAAAa/L5DwAAAJNJREFUeNptUVsOwzAIizEm3P/EI800rQl8tbH8wIzxG4x2AIgtCFh0wGLoBjAQOecF1L+bWTYy09bUB96E/W68lNwfSORlH5tj+c4McBGUcVK2TaDZj1FqfsT6BnBrdi+Oakk0rQBZWl0rdKVOj9WKHNRNiGRF88Nk1Q7WBXleEYhSqswXMFwMsktF6SE11/0v8ANdPQImejX+FwAAAABJRU5ErkJggg==)](https://github.com/dankamongmen/notcurses)

### Supported Distros

[![Debian](https://img.shields.io/badge/Debian-12%20%7C%2013-A81D33?logo=debian&logoColor=white)](https://www.debian.org)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-22.04%20%7C%2024.04%20%7C%2026.04-E95420?logo=ubuntu&logoColor=white)](https://ubuntu.com)
[![Fedora](https://img.shields.io/badge/Fedora-43%20%7C%2044-51A2DA?logo=fedora&logoColor=white)](https://fedoraproject.org)
[![AlmaLinux](https://img.shields.io/badge/AlmaLinux-8%20%7C%209%20%7C%2010-0095D5?logo=almalinux&logoColor=white)](https://almalinux.org)
[![openSUSE](https://img.shields.io/badge/openSUSE-Tumbleweed%20%7C%20Leap%2016.0-73BA25?logo=opensuse&logoColor=white)](https://www.opensuse.org)
[![Arch Linux](https://img.shields.io/badge/Arch%20Linux-Rolling-1793D1?logo=archlinux&logoColor=white)](https://archlinux.org)

### Build Status

[![Debian 12](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Debian%2012%20%28Bookworm%29&label=Debian%2012)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Debian 13](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Debian%2013%20%28Trixie%29&label=Debian%2013)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Ubuntu 22.04](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Ubuntu%2022.04%20%28Jammy%29&label=Ubuntu%2022.04)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Ubuntu 24.04](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Ubuntu%2024.04%20%28Noble%29&label=Ubuntu%2024.04)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Ubuntu 26.04](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Ubuntu%2026.04%20%28Resolute%29&label=Ubuntu%2026.04)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Fedora 43](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Fedora%2043&label=Fedora%2043)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Fedora 44](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Fedora%2044&label=Fedora%2044)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![AlmaLinux 8](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=AlmaLinux%208%20%28RHEL%208%29&label=AlmaLinux%208)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![AlmaLinux 9](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=AlmaLinux%209%20%28RHEL%209%29&label=AlmaLinux%209)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![AlmaLinux 10](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=AlmaLinux%2010%20%28RHEL%2010%29&label=AlmaLinux%2010)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![openSUSE Tumbleweed](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=openSUSE%20Tumbleweed&label=openSUSE%20Tumbleweed)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![openSUSE Leap 16.0](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=openSUSE%20Leap%2016.0&label=openSUSE%20Leap%2016.0)](https://github.com/rebootless/vitals/actions/workflows/build.yml)
[![Arch Linux](https://img.shields.io/github/check-runs/rebootless/vitals/main?nameFilter=Arch%20Linux&label=Arch%20Linux)](https://github.com/rebootless/vitals/actions/workflows/build.yml)

</div>

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

- Installs the required build dependencies
- Clones the Notcurses source at the release pinned by `NOTCURSES_VERSION` in `setup.sh`
- Builds Vitals
- Installs Vitals and its bundled themes under `/usr/local`

Safe to run multiple times.

### Quick Uninstall

Remove Vitals and its bundled themes with a single command:

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/rebootless/vitals/main/uninstall.sh)
```

The uninstaller:

- Removes the Vitals binary from `/usr/local/bin`
- Removes the bundled themes from `/usr/local/share/vitals/themes`
- Removes `/usr/local/share/vitals` if it is empty
- Asks before deleting your saved configuration and themes at `~/.config/vitals` (or `$XDG_CONFIG_HOME/vitals` when configured)

<details>
<summary align="center"><b>Build from Source</b> <i>(click to expand)</i></summary>

Clone the repository:

```bash
git clone https://github.com/rebootless/vitals.git
cd vitals
```

Prepare the build environment:

```bash
chmod +x setup.sh
./setup.sh
```

Build:

```bash
chmod +x build.sh
./build.sh
```

Install:

```bash
sudo cmake --install build
```

</details>

<details>
<summary align="center"><b>Run Without Installing</b> <i>(click to expand)</i></summary>

```bash
git clone https://github.com/rebootless/vitals.git
cd vitals

chmod +x setup.sh build.sh
./setup.sh
./build.sh

./build/vitals
```

</details>

> [!NOTE]
> Symbol Mirroring is experimental and may render incorrectly depending on your environment.
> Mirrored glyphs require font and Unicode 13+ `wcwidth` support.
> Terminal emulators may handle these glyphs differently.
> The Linux TTY (`TERM=linux`) does not support them.

## 📊 Panels

| Panel | Data source | Metrics |
| ---- | ---- | ---- |
| **CPU** | `/proc/stat`, `/sys/devices/system/cpu/cpu*/cpufreq/` | CPU usage, per-core activity, frequency, history |
| **GPU** | `/sys/class/drm`, `/sys/class/hwmon`, `nvidia-smi` | GPU utilization, VRAM, temperature, power |
| **Memory** | `/proc/meminfo` | RAM and swap usage |
| **Network** | `/proc/net/dev` | Per-interface RX/TX throughput |
| **Storage** | `/proc/diskstats`, `statvfs()` | Filesystem usage and disk I/O |
| **Thermal** | `/sys/class/thermal`, `/sys/class/hwmon` | CPU, GPU, and motherboard temperatures |

> [!NOTE]
> The GPU panel is displayed only when a supported device is detected. Otherwise, the CPU panel expands to use the available space.

## ⌨️ Controls

| Key | Action |
| :--- | :--- |
| `q` | Quit |
| `f` | Freeze |
| `Esc` | Open the settings menu |
| `Tab` | Select the next option |
| `↑` / `↓` | Change the selected value |
| `Enter` | Save changes and close |
| `Esc` *(in settings)* | Discard changes and close |

## 🧩 Self-test

```bash
vitals --self-test
```

Checks that every data source Vitals reads (CPU, memory, GPU, storage, network, thermal sensors, and system info) is available and returns sane values.

> [!NOTE]
> Linux buffers disk writes in RAM, then flushes them in batches.
> So downloads may show steady Network traffic while Storage stays idle.

## ♻️ Clean Up Legacy Installations

Remove files left by older pre-release versions:

```bash
sudo rm -f /usr/local/bin/vitals

for d in /usr/local/lib /usr/local/lib64; do
    sudo rm -f "$d"/libnotcurses*.so* "$d"/libnotcurses*.a
    sudo rm -rf "$d"/cmake/Notcurses*
    sudo rm -f "$d"/pkgconfig/notcurses*.pc
done

sudo rm -rf /usr/local/include/notcurses /usr/local/include/ncpp
sudo rm -f /etc/ld.so.conf.d/usr_local_lib.conf
sudo ldconfig
```

> [!CAUTION]
> This removes all matching Notcurses files from `/usr/local`, including installations used by other software. Run these commands only if you intend to remove those files.

## 📋 Requirements

- Linux (kernel ≥ 4.x)
- Bash 5.0+
- GCC or Clang with C++20 support
- CMake ≥ 3.21
- Internet access (required to download Notcurses during setup)

## 📄 License

<p align="center">
  Licensed under the <strong>GNU General Public License v3.0</strong>. See the <a href="LICENSE">LICENSE</a> file for details.
</p>
