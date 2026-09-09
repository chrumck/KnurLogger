#!/bin/bash
# audit-boot.sh — read-only survey of what this Pi runs at boot.
# Changes nothing. Run it before and after harden-headless.sh and keep both outputs.
#
# Usage: ./audit-boot.sh > ~/boot-audit-$(date +%Y%m%d-%H%M%S).txt

set -uo pipefail

# /usr/sbin is absent from PATH both for a non-interactive SSH session and for a
# non-root login shell on Debian. sysctl, rfkill, swapon and i2cdetect all live
# there, so without this line every check for them silently reports "missing" —
# including right after install-dependencies.sh has installed them.
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH"

section() {
    echo ""
    echo "════════════════════════════════════════════════════════"
    echo "  $*"
    echo "════════════════════════════════════════════════════════"
}

section "Identity"
echo "hostname : $(hostname)"
echo "date     : $(date -Is)"
echo "uptime   : $(uptime -p)"
echo "kernel   : $(uname -srm)"
echo "os       : $(. /etc/os-release && echo "$PRETTY_NAME")"
echo "model    : $(tr -d '\0' < /proc/device-tree/model 2>/dev/null)"
echo "memory   : $(free -h | awk '/^Mem:/ {print $2 " total, " $7 " available"}')"

section "Power and thermal — plan item 5.4 / build sheet §10 step 4"
# 0x0 is clean. bit 0 = undervoltage now, bit 16 = undervoltage has occurred since boot.
echo "get_throttled : $(vcgencmd get_throttled 2>/dev/null || echo 'vcgencmd unavailable')"
echo "core volts    : $(vcgencmd measure_volts core 2>/dev/null || echo n/a)"
echo "SoC temp      : $(vcgencmd measure_temp 2>/dev/null || echo n/a)"
echo ""
echo "-- dmesg, voltage/throttling --"
# Distinguish "nothing to report" from "could not read the log". They look
# identical once stderr is discarded, and plan item 5.4 treats this as pass/fail,
# so a false "good" here is worse than no answer at all.
if ! dmesg >/dev/null 2>&1; then
    echo "(dmesg unreadable — kernel.dmesg_restrict=$(cat /proc/sys/kernel/dmesg_restrict 2>/dev/null); re-run with sudo. THIS IS NOT A PASS.)"
elif dmesg 2>/dev/null | grep -i -E 'voltage|throttl'; then
    :
else
    echo "(no matches — good)"
fi

section "Boot time"
systemd-analyze time 2>/dev/null || echo "(unavailable)"
echo ""
echo "-- critical chain --"
systemd-analyze critical-chain 2>/dev/null | head -40
echo ""
echo "-- slowest 25 units --"
systemd-analyze blame 2>/dev/null | head -25

section "Enabled services (these are what boot costs you)"
systemctl list-unit-files --type=service --state=enabled --no-legend --no-pager | sort

section "Enabled timers (these are what interrupts you mid-session)"
systemctl list-timers --all --no-legend --no-pager

section "Running services"
systemctl list-units --type=service --state=running --no-legend --no-pager | sort

section "Failed units"
systemctl list-units --state=failed --no-legend --no-pager || true

section "Masked units — after hardening these are what it retired"
systemctl list-unit-files --state=masked --no-legend --no-pager | sort || echo "(none)"

section "cloud-init"
if [[ -f /etc/cloud/cloud-init.disabled ]]; then
    echo "disabled (/etc/cloud/cloud-init.disabled present)"
elif [[ -d /etc/cloud ]]; then
    # cloud-init status exits 2 while printing a perfectly good "status: done",
    # so the exit code is not usable as a proxy for whether it answered.
    ci_out="$(cloud-init status 2>/dev/null)"
    echo "ACTIVE — ${ci_out:-status unavailable}"
else
    echo "not installed"
fi

section "Radio state — Bluetooth must stay up, Wi-Fi is the one we gate"
# A soft-blocked Bluetooth radio means no BLE and therefore no product. It sits
# below BlueZ, so bluetoothctl cannot clear it and reports PowerState:
# off-blocked instead. This image ships with it blocked.
rfkill list 2>/dev/null || nmcli radio 2>/dev/null || echo "(neither rfkill nor nmcli available)"
if rfkill list bluetooth 2>/dev/null | grep -q 'Soft blocked: yes'; then
    echo ">>> BLUETOOTH IS SOFT-BLOCKED — there is no BLE in this state."
    echo ">>> Clear it with: sudo rfkill unblock bluetooth"
fi
echo ""
echo "-- bluetooth controller --"
hciconfig -a 2>/dev/null || bluetoothctl list 2>/dev/null || echo "(no controller reported)"
echo ""
echo "-- NetworkManager wifi powersave --"
grep -rs 'powersave' /etc/NetworkManager/ || echo "(not configured — defaults to 3/enabled)"

section "Buses the logger needs"
echo "-- i2c devices --"
ls -l /dev/i2c-* 2>/dev/null || echo "(no /dev/i2c-* — I2C not enabled)"
echo ""
echo "-- i2cdetect on bus 1 (expect 0x70 PCA9548A mux and 0x77 BME280; 0x77 not 0x76 as built) --"
i2cdetect -y 1 2>/dev/null || echo "(i2cdetect unavailable or bus absent)"
echo ""
echo "-- 1-Wire (expect the master alone at the bench; four 28-* only with probes attached) --"
ls -l /sys/bus/w1/devices/ 2>/dev/null || echo "(no w1 bus — w1-gpio overlay not loaded)"

section "Boot configuration"
CONFIG_TXT=/boot/firmware/config.txt
[[ -f $CONFIG_TXT ]] || CONFIG_TXT=/boot/config.txt
echo "-- $CONFIG_TXT, non-comment lines --"
grep -vE '^\s*(#|$)' "$CONFIG_TXT" 2>/dev/null
echo ""
CMDLINE=/boot/firmware/cmdline.txt
[[ -f $CMDLINE ]] || CMDLINE=/boot/cmdline.txt
echo "-- $CMDLINE --"
cat "$CMDLINE" 2>/dev/null

section "Storage and write pressure"
df -h / /boot/firmware 2>/dev/null
echo ""
echo "-- swap --"
cat /proc/swaps 2>/dev/null
echo ""
echo "-- dirty-page sysctls (target 5 / 10, per iSitePiLogger setupNotes) --"
sysctl vm.dirty_background_ratio vm.dirty_ratio 2>/dev/null
echo ""
echo "-- journal size --"
journalctl --disk-usage 2>/dev/null

section "Done — this script changed nothing"
