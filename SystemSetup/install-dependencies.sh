#!/bin/bash
# install-dependencies.sh — packages KnurLogger needs on the box, and nothing else.
#
# Target: Raspberry Pi OS Lite 64-bit (Trixie). Run as the login user; uses sudo.
#
# Usage: ./install-dependencies.sh [--execute]
#   default    pre-flight: prints what would be installed and what is already there
#   --execute  install
#
# Deliberately smaller than iSitePiLogger's list: no libcurlpp-dev, because
# KnurLogger has nothing to upload. Data leaves the box over BLE and on the SD
# card, so there is no HTTP path at all.

set -euo pipefail

# /usr/sbin is absent from PATH both for a non-interactive SSH session and for a
# non-root login shell on Debian. sysctl, rfkill, swapon and i2cdetect all live
# there, so without this line every check for them silently reports "missing" —
# including right after install-dependencies.sh has installed them.
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH"

DRY_RUN=true
[[ "${1:-}" == "--execute" ]] && DRY_RUN=false

if [[ "$DRY_RUN" == true ]]; then
    echo "*** PRE-FLIGHT MODE — nothing will be installed ***"
    echo "*** Run with --execute to install.              ***"
fi

run() {
    echo "  + $*"
    if [[ "$DRY_RUN" == false ]]; then
        "$@"
    fi
}

section() {
    echo ""
    echo "════════════════════════════════════════════════════════"
    echo "  $*"
    echo "════════════════════════════════════════════════════════"
}

# Everything below needs root. Fail here, clearly, rather than at the first sudo
# in the middle of a phase — especially since some failures further down are
# deliberately tolerated and would disguise a missing password as a unit error.
require_sudo() {
    if sudo -n true 2>/dev/null; then
        echo "  sudo: passwordless, proceeding."
        return 0
    fi
    if [[ -t 0 ]]; then
        echo "  sudo needs a password; you will be prompted once now."
        sudo -v || { echo "  sudo authentication failed." >&2; exit 1; }
        return 0
    fi
    cat >&2 <<'NOSUDO'

  sudo requires a password on this box and stdin is not a terminal, so it cannot
  be entered. Piping this script over `ssh host 'bash -s'` works for a pre-flight
  run but cannot work for --execute.

  Copy it over and run it from a login shell:

      scp SystemSetup/<script>.sh KnurLogger:~/
      ssh -t KnurLogger './<script>.sh --execute'
NOSUDO
    exit 1
}

# package:reason — reason is printed, so the list stays self-documenting.
PACKAGES=(
    "git:clone and update this repo on the box"
    "cmake:build system"
    "build-essential:gcc, g++ and make; gcc and g++ alone are already present"
    "libglib2.0-dev:GLib main loop, GThread workers and the GDBus that bluez_inc needs"
    "i2c-tools:i2cdetect, for bring-up diagnostics on the mux and BME280"
    "rfkill:radio gating, and clearing the Bluetooth soft block; present on this image already"
)

section "Current state"

MISSING=()
for entry in "${PACKAGES[@]}"; do
    pkg="${entry%%:*}"
    reason="${entry#*:}"
    if dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
        printf "  %-20s present\n" "$pkg"
    else
        printf "  %-20s MISSING — %s\n" "$pkg" "$reason"
        MISSING+=("$pkg")
    fi
done

if [[ ${#MISSING[@]} -eq 0 ]]; then
    section "Nothing to do"
    exit 0
fi

section "Installing ${#MISSING[@]} package(s)"

if [[ "$DRY_RUN" == false ]]; then
    require_sudo
fi

# apt-get rather than apt: apt prints "does not have a stable CLI interface" when
# stdout is not a terminal, and its output format is explicitly not guaranteed.
# DEBIAN_FRONTEND=noninteractive so a package that wants to ask something fails
# visibly instead of blocking forever on a box reached over SSH.
run sudo apt-get update
run sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y "${MISSING[@]}"

section "Verifying"

echo "  + tool check"
if [[ "$DRY_RUN" == false ]]; then
    for t in git cmake gcc g++ i2cdetect rfkill pkg-config; do
        printf "    %-12s %s\n" "$t" "$(command -v "$t" || echo MISSING)"
    done
    printf "    %-12s %s\n" "glib-2.0" "$(pkg-config --modversion glib-2.0 2>/dev/null || echo MISSING)"
fi

section "Done"
cat <<'DONE'
Not installed here, and deliberately:

  libcurlpp-dev   iSitePiLogger uploads readings over HTTP. KnurLogger writes to
                  the SD card and notifies over BLE. There is no upload path.
  libbluetooth-dev
                  bluez_inc talks to BlueZ over GDBus, not libbluetooth. The
                  bluez package itself is already present on this image.
  pigpio          iSitePiLogger needs it for GPIO. KnurLogger's only GPIO use is
                  the mux ~RESET, which config.txt sets at firmware time, and
                  1-Wire, which is a kernel driver. Add it only if a real need
                  appears.
DONE
