#!/bin/bash

set -euo pipefail
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

INSTALL_BIN="/usr/local/bin/vitals"
MANIFEST_DIR="/usr/local/share/vitals"
MANIFEST="$MANIFEST_DIR/install_manifest.txt"
CONFIG_DIR="$HOME/.config/vitals"

if [[ $EUID -eq 0 ]]; then
    echo "Do not run this script as root."
    exit 1
fi

if ! sudo -n true 2>/dev/null && ! sudo -v 2>/dev/null; then
    echo "sudo access is required. Aborting."
    exit 1
fi

if ! command -v vitals &>/dev/null && [[ ! -f "$MANIFEST" ]]; then
    echo "vitals does not appear to be installed."
    exit 0
fi

echo ""
echo "  Binary:   $INSTALL_BIN"
echo "  Manifest: $MANIFEST"
echo ""

if [[ -f "$MANIFEST" ]]; then
    MAPFILE_PATHS=()
    while IFS= read -r line; do
        [[ -n "$line" ]] && MAPFILE_PATHS+=("$line")
    done < "$MANIFEST"

    echo "This will remove ${#MAPFILE_PATHS[@]} files installed by install.sh:"
    echo ""
    printf '  %s\n' "${MAPFILE_PATHS[@]}"
    echo ""
    read -rp "Proceed? [y/N]: " CHOICE
    case "${CHOICE,,}" in
        y|yes) ;;
        *)     echo "Aborted."; exit 0 ;;
    esac
    echo ""

    for f in "${MAPFILE_PATHS[@]}"; do
        if [[ -e "$f" || -L "$f" ]]; then
            sudo rm -f -- "$f"
        fi
    done
    echo "Files removed."

    # rmdir only removes empty dirs, so shared system dirs stay untouched
    printf '%s\n' "${MAPFILE_PATHS[@]}" \
        | xargs -r -n1 dirname \
        | sort -u -r \
        | while IFS= read -r d; do
            [[ "$d" == /usr/local || "$d" == /usr/local/* ]] || continue
            sudo rmdir --ignore-fail-on-non-empty "$d" 2>/dev/null || true
        done

    sudo rm -rf "$MANIFEST_DIR"
else
    # no manifest: fall back to the known install paths
    echo "No install manifest found — falling back to a best-effort removal."
    echo "This can only remove the vitals binary and the notcurses libraries"
    echo "matching this project; it will not clean up notcurses' extra"
    echo "binaries, man pages, or docs from a manifest-less install."
    echo ""
    read -rp "Proceed? [y/N]: " CHOICE
    case "${CHOICE,,}" in
        y|yes) ;;
        *)     echo "Aborted."; exit 0 ;;
    esac
    echo ""

    sudo rm -f "$INSTALL_BIN"
    sudo rm -f /usr/local/lib/libnotcurses*.so*
    sudo rm -f /usr/local/lib/libnotcurses*.a
    sudo rm -rf /usr/local/lib/cmake/Notcurses* /usr/local/lib/cmake/NotcursesCore
    sudo rm -f /usr/local/lib/pkgconfig/notcurses*.pc
    sudo rm -rf /usr/local/include/notcurses /usr/local/include/ncpp
    echo "Best-effort removal done."
fi

if [[ -f /etc/ld.so.conf.d/usr_local_lib.conf ]]; then
    sudo rm -f /etc/ld.so.conf.d/usr_local_lib.conf
fi
sudo ldconfig

echo ""
if [[ -d "$CONFIG_DIR" ]]; then
    read -rp "Also remove user config at $CONFIG_DIR? [y/N]: " CHOICE
    case "${CHOICE,,}" in
        y|yes) rm -rf "$CONFIG_DIR"; echo "Removed $CONFIG_DIR." ;;
        *)     echo "Keeping $CONFIG_DIR." ;;
    esac
fi

echo ""
if command -v vitals &>/dev/null; then
    echo "Warning: 'vitals' is still on PATH at $(command -v vitals)."
    echo "It may have been installed from a different location."
else
    echo "vitals has been uninstalled."
fi