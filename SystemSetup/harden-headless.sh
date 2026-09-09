#!/bin/bash
# harden-headless.sh — strip a Raspberry Pi OS Lite install down to what
# KnurLogger actually needs, and configure the buses it uses.
#
# Target: Raspberry Pi 4B, Raspberry Pi OS Lite 64-bit (Trixie), NetworkManager.
# Run as the normal login user; it uses sudo internally.
#
# Usage: ./harden-headless.sh [--execute] [--drop-mdns] [--no-hdmi]
#   default      pre-flight: prints every command, executes none
#   --execute    perform the changes
#   --drop-mdns  also disable avahi-daemon (breaks <hostname>.local resolution)
#   --no-hdmi    also drop the KMS driver (breaks the emergency HDMI console)
#
# Bluetooth is never disabled here — BLE is the RaceChrono link and the reason
# box 2 exists. Phase 7 goes further and UN-blocks it: this image ships with the
# BT radio soft-blocked at the rfkill layer, persistently, which BlueZ reports as
# PowerState: off-blocked and which bluetoothctl cannot clear. Wi-Fi is gated per
# session rather than removed, because the cavity has no Ethernet. This is where
# this script diverges from iSitePiLogger's setupNotes.txt, which disables both
# radios outright.
#
# Reboot afterwards, then re-run audit-boot.sh and diff against the before run.

set -euo pipefail

# /usr/sbin is absent from PATH both for a non-interactive SSH session and for a
# non-root login shell on Debian. sysctl, rfkill, swapon and i2cdetect all live
# there, so without this line every check for them silently reports "missing" —
# including right after install-dependencies.sh has installed them.
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH"

DRY_RUN=true
DROP_MDNS=false
NO_HDMI=false

for arg in "$@"; do
    case "$arg" in
        --execute)   DRY_RUN=false ;;
        --drop-mdns) DROP_MDNS=true ;;
        --no-hdmi)   NO_HDMI=true ;;
        *) echo "unknown argument: $arg" >&2; exit 2 ;;
    esac
done

if [[ "$DRY_RUN" == true ]]; then
    echo "*** PRE-FLIGHT MODE — nothing will be executed ***"
    echo "*** Run with --execute to apply.                ***"
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

# list-unit-files does not list an uninstantiated template instance such as
# serial-getty@ttyS0.service, so LoadState is the fallback: a unit systemd can
# load is a unit that exists, however it got there.
unit_exists() {
    systemctl list-unit-files --no-legend --no-pager "$1" 2>/dev/null | grep -q . && return 0
    [[ "$(systemctl show -p LoadState --value "$1" 2>/dev/null)" == "loaded" ]]
}

# Masking is the operative step; disabling is a courtesy. Two reasons.
#
# Masking, because apt's own timers re-enable themselves whenever their package
# is upgraded, and a mask survives that.
#
# `disable` tolerated, because systemd refuses to disable a unit with no
# [Install] section and the refusal is a non-zero exit. This box has both cases:
# rpi-zram-writeback.service is `static` and rpi-zram-writeback.timer is
# `generated`. Under `set -e` an untolerated failure there would abort the whole
# script mid-run, silently skipping phases 6, 7 and 8 — including every
# config.txt bus setting, which is the part that actually matters. So the exit
# status that gets checked is `mask`'s.
retire_unit() {
    local unit="$1" reason="$2"
    if ! unit_exists "$unit"; then
        echo "  $unit — not installed, skipping"
        return 0
    fi
    echo "  $unit — $reason"
    # disable and stop are issued separately and both tolerated. `disable --now`
    # refuses outright on a unit with no [Install] section, and when it refuses
    # the --now half never runs either — so a generated, ACTIVE unit like
    # rpi-zram-writeback.timer would have been masked while still armed for the
    # rest of the uptime. Masking does not stop a running unit.
    echo "  + sudo systemctl disable $unit   (failure tolerated: static/generated units)"
    if [[ "$DRY_RUN" == false ]]; then
        sudo systemctl disable "$unit" 2>/dev/null || true
    fi
    if [[ "$(systemctl is-active "$unit" 2>/dev/null)" != "inactive" ]]; then
        echo "  + sudo systemctl stop $unit   (it is currently active)"
        if [[ "$DRY_RUN" == false ]]; then
            sudo systemctl stop "$unit" 2>/dev/null || true
        fi
    fi
    echo "  + sudo systemctl mask $unit"
    if [[ "$DRY_RUN" == false ]]; then
        if ! sudo systemctl mask "$unit"; then
            echo "  !! FAILED to mask $unit — it is still active. Investigate before trusting this run." >&2
            return 1
        fi
    fi
}

# ─── Guard ───────────────────────────────────────────────────────────────────

if ! grep -qi raspberry /proc/device-tree/model 2>/dev/null; then
    echo "This does not look like a Raspberry Pi. Refusing to run." >&2
    exit 1
fi

CONFIG_TXT=/boot/firmware/config.txt
CMDLINE=/boot/firmware/cmdline.txt
if [[ ! -f $CONFIG_TXT ]]; then
    CONFIG_TXT=/boot/config.txt
    CMDLINE=/boot/cmdline.txt
fi
[[ -f $CONFIG_TXT ]] || { echo "config.txt not found" >&2; exit 1; }

STAMP="$(date +%Y%m%d-%H%M%S)"

if [[ "$DRY_RUN" == false ]]; then
    require_sudo
fi

# ─── What stays, and why ─────────────────────────────────────────────────────

section "Retained on purpose — do not optimise these away later"
cat <<'KEEP'
  bluetooth.service      BLE is the RaceChrono DIY link. The whole point of box 2.
                         Phase 7 also clears the rfkill soft block this image
                         ships with, without which the service runs and the
                         controller still refuses to power.
                         (There is no hciuart.service on this image — the BCM43455
                         is attached by udev, so bluetooth.service is the only
                         unit involved. Older recipes name hciuart; do not go
                         looking for it here.)
  NetworkManager         Wi-Fi is the only headless access route into a wheel-well
                         cavity with no Ethernet. Gated per session, not removed.
                         See plan item 1b.
  wpa_supplicant         NetworkManager drives it over D-Bus on this image, and it
                         is enabled and running. Retiring it loses Wi-Fi.
  systemd-timesyncd      A Pi 4B has no RTC, and fake-hwclock is NOT installed on
                         this image, so timesyncd is the only thing standing
                         between a session file and a 1970 timestamp. It saves the
                         time to /var/lib/systemd/timesync/clock and restores that
                         at boot when no network is reachable.
                         CONSEQUENCE, and it is a measurement one: in the car there
                         is no NTP, so the clock resumes from whatever it was when
                         the Pi last synced on the bench. It will be monotonic and
                         plausible but WRONG by however long ago that was. Anything
                         correlating the SD session file against RaceChrono/GPS
                         time must record an offset at session start rather than
                         trust the Pi clock.
  getty@tty1             Emergency console on HDMI. Costs nothing, and it is the
                         only way back in when Wi-Fi will not associate.
  cron, polkit           polkit is a NetworkManager dependency; cron is inert
                         until something is scheduled.
  systemd-journald       Kept, but capped below. The previous boot journal is how
                         an ignition-off cut gets diagnosed.
KEEP

# ─── Phase 1 — Boot-blocking network wait ────────────────────────────────────

section "Phase 1 — Stop waiting for the network, and stop re-provisioning"

# Measured on this box, 2026-09-09: 5.983 s, the slowest unit on the system, on
# a boot that had an access point in range. In the car there is none, so it runs
# to its timeout instead.
retire_unit NetworkManager-wait-online.service "5.98 s measured here; runs to timeout with no AP"
retire_unit systemd-networkd-wait-online.service "same, if networkd is installed"

# Raspberry Pi Imager provisions the user, SSH keys and Wi-Fi through cloud-init
# on Trixie. That work is done once, at first boot, and persists in ordinary
# files afterwards — /etc/passwd, ~/.ssh/authorized_keys and a NetworkManager
# connection profile. Everything after that is a datasource probe that finds the
# same answer. Measured cost here: 2.53 s across five units.
#
# Disabling it does NOT undo what it provisioned. It does mean a future change
# to Imager-style settings has to be made by hand, which on a box you already
# have a shell on is not a loss.
if [[ -d /etc/cloud ]] && [[ ! -f /etc/cloud/cloud-init.disabled ]]; then
    echo "  cloud-init — first-boot provisioning already applied (status: done)"
    run sudo touch /etc/cloud/cloud-init.disabled
else
    echo "  cloud-init — absent or already disabled"
fi

# ─── Phase 2 — Unattended background work ────────────────────────────────────

section "Phase 2 — Timers that interrupt a session"

retire_unit apt-daily.timer           "apt metadata refresh — multi-second I/O stall at random"
retire_unit apt-daily-upgrade.timer   "unattended upgrades — can replace the running kernel mid-session"
retire_unit man-db.timer              "man page index rebuild, pure CPU and SD writes"
retire_unit dpkg-db-backup.timer      "dpkg database copy, pure SD writes"
retire_unit e2scrub_all.timer         "LVM metadata scrub, no LVM here"
retire_unit e2scrub_reap.service      "companion to the above"
retire_unit fstrim.timer              "weekly TRIM; run it by hand between sessions instead"
retire_unit logrotate.timer           "journald is capped below; nothing else writes /var/log"

# ─── Phase 3 — Services with no consumer on this box ─────────────────────────

section "Phase 3 — Services with nothing to serve"

retire_unit ModemManager.service      "no cellular modem; probes serial ports at boot"
retire_unit rpi-eeprom-update.service "bootloader update check; do it deliberately, not every boot"
retire_unit rsyslog.service           "duplicates journald into /var/log, doubling SD writes"
retire_unit triggerhappy.service      "GPIO/keyboard hotkey daemon, no input devices"
retire_unit triggerhappy.socket       "companion socket"
retire_unit bluealsa.service          "BlueZ audio; box 2 uses GATT only"
retire_unit udisks2.service           "USB automount; mount deliberately via fstab if a stick is used"
retire_unit rpcbind.service           "NFS portmapper"
retire_unit rpcbind.socket            "companion socket"
retire_unit nfs-client.target         "no NFS"
retire_unit cups.service              "no printing"
retire_unit cups-browsed.service      "no printing"
retire_unit keyboard-setup.service    "no keyboard; 280 ms measured here"
retire_unit console-setup.service     "console font and keymap for a console nobody reads"

# wpa_supplicant.service is LEFT ALONE whenever NetworkManager is installed.
# On this image it is enabled and running, and NetworkManager talks to it over
# D-Bus rather than spawning its own — so retiring it is a plausible-looking way
# to lose Wi-Fi on a box whose only access route is Wi-Fi. An earlier draft of
# this script retired it on the strength of it being enabled; the first real
# audit is what caught that.
if unit_exists NetworkManager.service; then
    echo "  wpa_supplicant.service — KEPT. NetworkManager drives it over D-Bus on this image."
elif unit_exists wpa_supplicant.service && \
     systemctl is-enabled wpa_supplicant.service >/dev/null 2>&1; then
    retire_unit wpa_supplicant.service "no NetworkManager present, so nothing consumes it"
fi

if [[ "$DROP_MDNS" == true ]]; then
    retire_unit avahi-daemon.service "mDNS — --drop-mdns given; <hostname>.local stops resolving"
    retire_unit avahi-daemon.socket  "companion socket"
else
    echo "  avahi-daemon.service — KEPT. It is what makes <hostname>.local resolve, and that"
    echo "                         is your way back in. Pass --drop-mdns to retire it, but"
    echo "                         reserve a static DHCP lease for the Pi first."
fi

# ─── Phase 4 — Serial console ────────────────────────────────────────────────

section "Phase 4 — Serial console off, UART left clean"

# ttyAMA0 is the Bluetooth UART on a Pi 4B. The login console is the mini-UART,
# ttyS0. Disabling this getty frees it without touching the BT attachment.
retire_unit serial-getty@ttyS0.service "login shell on serial; the header pins are for sensors"

if grep -q 'console=serial0' "$CMDLINE" 2>/dev/null; then
    run sudo cp "$CMDLINE" "${CMDLINE}.bak-${STAMP}"
    echo "  + editing $CMDLINE: removing console=serial0,115200"
    if [[ "$DRY_RUN" == false ]]; then
        sudo sed -i 's/console=serial0,[0-9]* //' "$CMDLINE"
    fi
else
    echo "  $CMDLINE — no console=serial0 entry, nothing to do"
fi

# ─── Phase 5 — Swap ──────────────────────────────────────────────────────────

section "Phase 5 — Swap: keep zram, stop it writing back to the card"

# Raspberry Pi OS Trixie swaps to zram (/dev/zram0), not to a file on the SD
# card. That is RAM-backed, so it costs no card wear and there is nothing to
# gain by turning it off — with 3.5 GB free and a logger measured in megabytes
# it will never be touched anyway. dphys-swapfile does not exist on this image.
#
# The writeback timer is the exception: its whole job is to push idle zram pages
# out to a backing device, which is the SD write this phase exists to avoid.
retire_unit rpi-zram-writeback.timer   "pushes idle zram pages to the SD card"
retire_unit rpi-zram-writeback.service "companion service"
retire_unit dphys-swapfile.service     "file-backed swap, if this image has it instead"

# ─── Phase 6 — Write pressure ────────────────────────────────────────────────

section "Phase 6 — Write pressure, and evidence that survives a power cut"

# Plan item 5b: the accessory feed disappears without warning at ignition-off,
# so the last durable write bounds what a hard cut costs. These are the values
# iSitePiLogger's setupNotes.txt sets, for the same reason.
# 99-, not 90-: /etc/sysctl.d/98-rpi.conf ships with this image and would sort
# after a 90- file. It sets no dirty ratios today, so 90- happened to work, but
# the ordering was luck rather than design.
SYSCTL_FILE=/etc/sysctl.d/99-knurlogger.conf
echo "  + writing $SYSCTL_FILE"
if [[ "$DRY_RUN" == false ]]; then
    sudo tee "$SYSCTL_FILE" >/dev/null <<'SYSCTL'
# KnurLogger. Small dirty-page ratios keep the writeback window
# short, so an unannounced power cut at ignition-off costs about a second of
# samples rather than a session. See ../ndLouvers/CFD-Learning-Plan.md Step 0b item 5b.
vm.dirty_background_ratio = 5
vm.dirty_ratio = 10
SYSCTL
fi
run sudo sysctl --system

# This one is a DELIBERATE OVERRIDE OF A VENDOR DEFAULT, and it cuts against the
# rest of this phase, so it is stated rather than buried.
#
# Raspberry Pi OS ships /usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf
# setting Storage=volatile: the journal lives in /run, in RAM, and touches the SD
# card never. That is a considered SD-protection choice by the vendor and this
# phase otherwise exists to reduce card writes. Switching to persistent ADDS a
# continuous, if small, writer.
#
# It is still the right trade here, for one reason: the failure this box is built
# to survive is the accessory feed vanishing at ignition-off, and a volatile
# journal is destroyed by exactly that event. The session file records the data;
# only the journal records an undervoltage flag, a thermal event or an oops in
# the seconds before the cut. Losing it means the one class of failure that
# matters is undiagnosable by construction.
#
# The magnitude is acceptable: journald on an idle headless box writes a few
# hundred kB a day, against a session file being written at 10 Hz.
#
# Names sort before directories in systemd drop-in resolution, so 90- here beats
# the vendor's 40-. To go back to the vendor behaviour, delete this file.
JOURNAL_FILE=/etc/systemd/journald.conf.d/90-knurlogger.conf
echo "  + writing $JOURNAL_FILE  (OVERRIDES the vendor Storage=volatile — see comment)"
if [[ "$DRY_RUN" == false ]]; then
    sudo mkdir -p /etc/systemd/journald.conf.d
    sudo tee "$JOURNAL_FILE" >/dev/null <<'JOURNAL'
# KnurLogger. Deliberately overrides Raspberry Pi OS's
# /usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf, which keeps the
# journal in RAM to spare the SD card.
#
# Why: this box loses power without warning at ignition-off, and a volatile
# journal does not survive that -- which is precisely the event that needs
# diagnosing. Capped tightly because the same card holds the session record.
# Delete this file to return to the vendor's volatile journal.
[Journal]
Storage=persistent
SystemMaxUse=64M
SystemMaxFileSize=8M
JOURNAL
fi
run sudo systemctl restart systemd-journald

# ─── Phase 7 — Radios ────────────────────────────────────────────────────────

section "Phase 7 — Un-block Bluetooth, and stop Wi-Fi power save"

# The Bluetooth radio ships SOFT-BLOCKED on this image, measured 2026-09-09:
# `rfkill list` reports "hci0: Bluetooth  Soft blocked: yes" and BlueZ reports
# "PowerState: off-blocked". BLE is the product, so this is not cosmetic.
#
# Two things follow that are easy to get wrong:
#   - `bluetoothctl power on` CANNOT clear it. A soft block sits below BlueZ; the
#     controller refuses to power while it is set, whatever BlueZ is told.
#   - The block is persistent. systemd-rfkill saves per-device state under
#     /var/lib/systemd/rfkill/ and restores it on boot, so it survives reboots
#     and would survive right up to the first session that recorded nothing.
# Unblocking once is therefore enough, and it is saved back the same way.
if command -v rfkill >/dev/null 2>&1; then
    if rfkill list bluetooth 2>/dev/null | grep -q 'Soft blocked: yes'; then
        echo "  Bluetooth is SOFT-BLOCKED — clearing it. Without this there is no BLE."
        run sudo rfkill unblock bluetooth
    else
        echo "  Bluetooth is not soft-blocked; nothing to clear."
    fi
    echo "  (Wi-Fi is deliberately left unblocked. Gate it per session — see the end of this run.)"
else
    echo "  !! rfkill not found. Check the Bluetooth block by hand before trusting a session:" >&2
    echo "     rfkill list bluetooth" >&2
fi

# Power save is a common cause of SSH sessions stalling for seconds at a time on
# an otherwise idle Pi. It does not affect the BLE side.
NM_FILE=/etc/NetworkManager/conf.d/90-knurlogger-wifi-powersave.conf
echo "  + writing $NM_FILE"
if [[ "$DRY_RUN" == false ]]; then
    sudo mkdir -p /etc/NetworkManager/conf.d
    sudo tee "$NM_FILE" >/dev/null <<'NMCONF'
[connection]
wifi.powersave = 2
NMCONF
fi

# ─── Phase 8 — Buses and boot firmware ───────────────────────────────────────

section "Phase 8 — config.txt: buses on, everything visual off"

run sudo cp "$CONFIG_TXT" "${CONFIG_TXT}.bak-${STAMP}"

# dtparam=i2c_arm=on registers the adapter. It does NOT create /dev/i2c-1 —
# that needs the i2c-dev module, and nothing on this image loads it: /etc/modules
# holds only comments, and unlike 1-Wire there is no modalias path that would
# pull it in when the adapter appears. raspi-config's do_i2c does BOTH steps,
# and an earlier draft of this script did only the first, which would have left
# `i2cdetect -y 1` with no device to open at bring-up.
MODULES_FILE=/etc/modules-load.d/knurlogger.conf
echo "  + writing $MODULES_FILE  (i2c-dev — without it /dev/i2c-1 never appears)"
if [[ "$DRY_RUN" == false ]]; then
    sudo mkdir -p /etc/modules-load.d
    sudo tee "$MODULES_FILE" >/dev/null <<'MODULES'
# KnurLogger. dtparam=i2c_arm=on in config.txt registers the I2C adapter; this
# module is what exposes it as /dev/i2c-1 for userspace. Both are required.
i2c-dev
MODULES
    # Load it now as well, so a `modprobe`-visible failure surfaces here rather
    # than after the reboot. The device node still needs the reboot, because the
    # adapter itself does not exist until config.txt is re-read.
    sudo modprobe i2c-dev || echo "  !! modprobe i2c-dev failed — check before rebooting" >&2
fi

MARK_BEGIN='# ---- KnurLogger (managed) ----'
MARK_END='# ---- end KnurLogger ----'

# Drop any previous managed block before scanning for stock lines, so a re-run
# reads the original file rather than the block this script last appended.
if grep -qF "$MARK_BEGIN" "$CONFIG_TXT"; then
    echo "  + removing the previous managed block from $CONFIG_TXT"
    if [[ "$DRY_RUN" == false ]]; then
        sudo sed -i "\|^${MARK_BEGIN}\$|,\|^${MARK_END}\$|d" "$CONFIG_TXT"
    fi
fi

# dtparam ordering in config.txt is not reliably last-wins, so contradicting
# stock lines are commented out rather than overridden by the block below.
comment_out() {
    local pattern="$1"
    if grep -qE "^[[:space:]]*${pattern}" "$CONFIG_TXT"; then
        echo "  + commenting out '${pattern}' in $CONFIG_TXT"
        if [[ "$DRY_RUN" == false ]]; then
            sudo sed -i -E "s|^([[:space:]]*${pattern}.*)$|# KnurLogger superseded: \1|" "$CONFIG_TXT"
        fi
    fi
}

comment_out 'dtparam=audio=on'
comment_out 'camera_auto_detect=1'
comment_out 'display_auto_detect=1'
comment_out 'dtparam=i2c_arm=on'
if [[ "$NO_HDMI" == true ]]; then
    comment_out 'dtoverlay=vc4-kms-v3d'
    comment_out 'max_framebuffers=2'
fi

# A config.txt with no final newline would glue the first marker line onto the
# last stock line, silently breaking both that line and the idempotent removal
# above. This file has one today; the guard costs nothing and the failure would
# be invisible.
echo "  + ensuring $CONFIG_TXT ends with a newline before appending"
if [[ "$DRY_RUN" == false ]]; then
    # tail -c1 in a command substitution yields "" when the last byte IS a
    # newline, because substitution strips trailing newlines. Non-empty means
    # the file ends mid-line.
    if [[ -s "$CONFIG_TXT" && -n "$(tail -c1 "$CONFIG_TXT")" ]]; then
        echo | sudo tee -a "$CONFIG_TXT" >/dev/null
    fi
fi

echo "  + appending managed block to $CONFIG_TXT"
if [[ "$DRY_RUN" == false ]]; then
    sudo tee -a "$CONFIG_TXT" >/dev/null <<CONFIG
${MARK_BEGIN}
# [all] first: the stock file ends inside a conditional filter section, and an
# appended block would otherwise inherit it.
[all]

# Buses the logger needs. I2C1 carries the TCA9548A at 0x70 and the BME280 at
# 0x76. Written by KnurLogger's SystemSetup/harden-headless.sh; the reasoning
# lives in that repo and in the perfboard build sheet section 4.
#
# 100 kHz is specified by that build sheet's section 6, which owns the decision.
# Three reasons, and the throughput one is not the strongest:
#   - No throughput argument exists. Five sensors at 10 Hz is a few hundred bytes
#     per second; 100 kHz runs that at under 10 percent bus duty.
#   - Rise time is the real constraint. Each mux channel carries its own 4.7 kOhm
#     pull-ups (R1-R10), and t_rise ~= 0.85*R*C against the I2C limit of 300 ns in
#     fast mode leaves only ~75 pF of budget per segment. Standard mode allows
#     1000 ns, so the same pull-ups get ~250 pF. Breakouts and short wiring can
#     eat 75 pF.
#   - Slower edges are better behaved next to a switching regulator on the same
#     board.
# Do not raise this to 400 kHz without measuring the segment capacitance or
# lowering the pull-ups, and not without changing the build sheet first.
dtparam=i2c_arm=on,i2c_arm_baudrate=100000

# 1-Wire on GPIO4 for the four DS18B20s. No pullup= parameter: the overlays
# README on this image says of w1-gpio's pullup, "Now enabled by default
# (ignored)", so passing it is noise. The overlay that drives an external strong
# pullup from a second GPIO is a DIFFERENT one, w1-gpio-pullup, and this build
# must not use it — R11 (2.2 kOhm) is a plain resistor to 3V3, not a switched
# parasite-power pullup.
#
# No modules-load entry is needed for 1-Wire, unlike the I2C line above:
# w1_therm carries the alias w1-family-0x28, so the w1 core loads it on
# discovering a DS18B20.
dtoverlay=w1-gpio,gpiopin=4

# Release the TCA9548A ~RESET at firmware time, before Linux starts. Build sheet
# section 3a.5 item 6: GPIO17 boots as an input with the SoC's ~50 kOhm pull-down
# fighting R12, and the resulting level was never measured. Driving it high here
# removes the question instead of answering it.
gpio=17=op,dh

# Nothing on this box makes a sound, has a camera, or has a display.
dtparam=audio=off
camera_auto_detect=0
display_auto_detect=0
disable_splash=1
boot_delay=0
${MARK_END}
CONFIG
fi

echo ""
echo "  NOTE: dtoverlay=disable-bt and dtoverlay=disable-wifi are deliberately"
echo "        absent. iSitePiLogger sets both; box 2 must not."

# ─── Summary ─────────────────────────────────────────────────────────────────

section "Done"
cat <<'DONE'
Next:
  1. sudo reboot        <- required: the buses do not exist until config.txt
                           is re-read, so /dev/i2c-1 will not appear before it
  2. ./audit-boot.sh > ~/boot-audit-after.txt   and diff against the before run
  3. Confirm the buses:              ls /dev/i2c-1 ; ls /sys/bus/w1/devices/
     An empty i2cdetect scan is expected until the sensor zone is built; a
     MISSING /dev/i2c-1 is not, and means i2c-dev or the dtparam did not take.
  4. Plan item 5.4 / build sheet section 10 step 4, still outstanding and now
     one command away:
       vcgencmd get_throttled        0x0 is a pass. Anything else is the HW-384
                                     margin question, not a formality.
       dmesg | grep -i -E 'voltage|throttl'

Confirm the radio that matters actually came up:
  rfkill list bluetooth       # expect "Soft blocked: no"
  bluetoothctl show           # expect Powered: yes, NOT "PowerState: off-blocked"
If it still reads off-blocked, phase 7 did not take and there is no BLE.

To gate Wi-Fi for a session without giving up your way back in:
  sudo rfkill block wifi      # before a run
  sudo rfkill unblock wifi    # after
`nmcli radio wifi off` does the same through NetworkManager. Either is reversible
over a shell; dtoverlay=disable-wifi is not, which is why neither this script nor
the runbook uses it. Note that `rfkill block wifi` targets wlan only and leaves
Bluetooth alone — do not use bare `rfkill block all`, which would take the BLE
link down with it. Leave the gating manual until the logger owns it: plan item 5a
wants the link qualified in the installed position before any of it is trusted.

Rollback — everything this script touched, in one place:
  config.txt, cmdline.txt   restore the .bak-<timestamp> copies beside them
  masked units              sudo systemctl unmask <unit>
  cloud-init                sudo rm /etc/cloud/cloud-init.disabled
  journal back to RAM       sudo rm /etc/systemd/journald.conf.d/90-knurlogger.conf
                            (restores the vendor Storage=volatile)
  Bluetooth block           sudo rfkill block bluetooth   (you will not want this)
  files created, all removable:
      /etc/sysctl.d/99-knurlogger.conf
      /etc/systemd/journald.conf.d/90-knurlogger.conf
      /etc/NetworkManager/conf.d/90-knurlogger-wifi-powersave.conf
      /etc/modules-load.d/knurlogger.conf
DONE
