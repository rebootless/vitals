#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

SUDO=""
[[ $EUID -eq 0 ]] || SUDO="sudo"

FAMILY=""
if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    for id in ${ID:-} ${ID_LIKE:-}; do
        case "$id" in
            debian|ubuntu)        FAMILY=apt;    break ;;
            fedora|rhel|centos)   FAMILY=dnf;    break ;;
            suse|opensuse*|sles*) FAMILY=zypper; break ;;
            arch)                 FAMILY=pacman; break ;;
        esac
    done
fi

case "$FAMILY" in
    apt)
        $SUDO apt-get update -q
        $SUDO apt-get install -y --no-install-recommends \
            git build-essential cmake pkg-config \
            libncurses-dev libunistring-dev zlib1g-dev
        ;;
    dnf)
        # RHEL-compatible -devel packages live in CRB (PowerTools on 8)
        if [[ "${ID:-}" != "fedora" ]]; then
            $SUDO dnf install -y dnf-plugins-core
            for repo in crb powertools; do
                $SUDO dnf config-manager --set-enabled "$repo" 2>/dev/null || true
            done
        fi
        $SUDO dnf install -y \
            git gcc-c++ make cmake pkgconf-pkg-config \
            ncurses-devel libunistring-devel zlib-devel
        ;;
    zypper)
        $SUDO zypper --non-interactive install --no-recommends \
            git gcc-c++ make cmake pkg-config \
            ncurses-devel libunistring-devel zlib-devel
        ;;
    pacman)
        # A fresh container has no package databases: sync (and upgrade) only in that case.
        if ! compgen -G "/var/lib/pacman/sync/*.db" > /dev/null; then
            $SUDO pacman -Syu --noconfirm
        fi
        $SUDO pacman -S --needed --noconfirm \
            git gcc make cmake pkgconf ncurses libunistring zlib
        ;;
    *)
        echo "Unsupported distribution (ID=${ID:-unknown}, ID_LIKE=${ID_LIKE:-none})." >&2
        echo "Install manually: a C++20 compiler, make, cmake, pkg-config, git and the" >&2
        echo "development files for ncurses (terminfo), libunistring and zlib." >&2
        exit 1
        ;;
esac

echo "Dependencies installed ($FAMILY)."
