#!/bin/bash
# Installs the udev rule that lets the logger trigger a 1-Wire bulk temperature conversion.
#
# Without it a four-probe cycle costs ~3.2 s instead of ~1 s, because each probe's read pays its
# own ~800 ms conversion. It is NOT the whole story: the logger must also write the trigger as
# eight bytes (`"trigger\n"`), or the attribute is writable and still converts nothing.
# Measured on this box 2026-09-10: the write to `therm_bulk_read` is
# refused with EACCES, the attribute being 0644 root:root while the logger runs as `chrum`.
# `60-knurlogger-w1-bulk-read.rules` carries the reasoning; read it before changing anything here.
#
# Pre-flight by default, like every script in this directory. Run it, read the + lines, run it
# again with --execute.
#
# --verify makes the attribute appear WITHOUT a probe, by asking the 1-Wire master to register a
# fake family-0x28 slave, and reports the resulting owner and mode. That is the only way to test
# this on a bench box: `therm_bulk_read` does not exist until a w1_therm slave attaches, and the
# four probes live on the car. The fake slave is removed again on the way out, and nothing else
# on the box is touched.

set -u

PATH="$PATH:/usr/sbin:/sbin"

RULES_NAME="60-knurlogger-w1-bulk-read.rules"
RULES_SOURCE="$(dirname "$(readlink -f "$0")")/$RULES_NAME"
RULES_TARGET="/etc/udev/rules.d/$RULES_NAME"
MASTER_DIR="/sys/bus/w1/devices/w1_bus_master1"
BULK_READ="$MASTER_DIR/therm_bulk_read"
# The kernel parses this with `sscanf(buf, "%02x-%012llx", ...)`, so the hyphen is load-bearing:
# a space is rejected with EINVAL. `w1_master_add` prints the format itself when read.
FAKE_SLAVE_ID="28-0000000001ff"
FAKE_SLAVE_DIR="/sys/bus/w1/devices/28-0000000001ff"
LOGGER_GROUP="gpio"

IS_EXECUTE=0
IS_VERIFY=0

for argument in "$@"; do
    case "$argument" in
        --execute) IS_EXECUTE=1 ;;
        --verify) IS_VERIFY=1 ;;
        *) echo "Unknown argument: $argument"; echo "Usage: $0 [--execute] [--verify]"; exit 2 ;;
    esac
done

# The logger's account, i.e. whoever invoked this rather than root. sudo needs a password on this
# box, so anything mutating has to come from a login shell (`ssh -t`), not a piped one.
LOGGER_USER="${SUDO_USER:-$(id -un)}"

requireInteractiveSudo() {
    if [ "$(id -u)" -eq 0 ] || sudo -n true 2>/dev/null; then return 0; fi
    if [ -t 0 ]; then return 0; fi
    echo "FAIL: this needs sudo, sudo needs a password here, and there is no terminal to type it."
    echo "      Run it from a login shell:"
    echo "        ssh -t KnurLogger 'bash ~/KnurLogger/SystemSetup/grant-w1-bulk-read.sh --execute'"
    exit 1
}

if [ ! -f "$RULES_SOURCE" ]; then
    echo "FAIL: no rules file at $RULES_SOURCE"
    exit 1
fi

if [ "$IS_EXECUTE" -eq 0 ] && [ "$IS_VERIFY" -eq 0 ]; then
    echo "PRE-FLIGHT ONLY. Re-run with --execute to apply, --verify to test it against a fake probe."
    echo
fi

echo "Current state:"
if [ -e "$RULES_TARGET" ]; then
    if cmp -s "$RULES_SOURCE" "$RULES_TARGET"; then
        echo "  rule installed and up to date: $RULES_TARGET"
    else
        echo "  rule installed but DIFFERENT from this repository's copy: $RULES_TARGET"
    fi
else
    echo "  rule NOT installed"
fi
if [ -e "$BULK_READ" ]; then
    echo "  $BULK_READ exists: $(stat -c '%U:%G %a' "$BULK_READ")"
else
    echo "  $BULK_READ does not exist - no w1_therm slave is attached, which is normal on the bench"
fi
echo

if [ "$IS_EXECUTE" -eq 1 ]; then
    requireInteractiveSudo
    echo "+ install $RULES_TARGET"
    sudo cp "$RULES_SOURCE" "$RULES_TARGET" || exit 1
    sudo chmod 644 "$RULES_TARGET"
    echo "+ udevadm control --reload"
    sudo udevadm control --reload || exit 1
    # Only fixes the attribute if a probe happens to be attached right now; the rule is what covers
    # every future attach, including the ones at boot in the car.
    if [ -e "$BULK_READ" ]; then
        echo "+ a probe is attached, so applying to the live attribute too"
        sudo chgrp "$LOGGER_GROUP" "$BULK_READ" && sudo chmod g+w "$BULK_READ"
        echo "  now: $(stat -c '%U:%G %a' "$BULK_READ")"
    fi
    echo "  Done. The rule fires on every DS18B20 attach from now on."
    echo
fi

if [ "$IS_VERIFY" -eq 1 ]; then
    requireInteractiveSudo
    if [ -e "$FAKE_SLAVE_DIR" ]; then
        echo "FAIL: $FAKE_SLAVE_DIR already exists. Remove it before verifying:"
        echo "        echo '$FAKE_SLAVE_ID' | sudo tee $MASTER_DIR/w1_master_remove"
        exit 1
    fi

    echo "+ registering a fake family-0x28 slave so therm_bulk_read appears"
    if ! echo "$FAKE_SLAVE_ID" | sudo tee "$MASTER_DIR/w1_master_add" >/dev/null; then
        echo "  FAIL: the 1-Wire master would not accept '$FAKE_SLAVE_ID'."
        echo "        It parses the id as %02x-%012llx, so the hyphen matters. Nothing was added,"
        echo "        so there is nothing to clean up. The rule itself is unaffected."
        exit 1
    fi
    sleep 2

    RESULT=1
    if [ ! -e "$BULK_READ" ]; then
        echo "  UNEXPECTED: $BULK_READ still does not exist after attaching a family-0x28 slave."
    else
        OWNERSHIP="$(stat -c '%U:%G %a' "$BULK_READ")"
        echo "  $BULK_READ is $OWNERSHIP"
        # The real test is not the mode string but whether the logger's account can write it.
        if sudo -u "$LOGGER_USER" sh -c "echo trigger > $BULK_READ" 2>/dev/null; then
            echo "  PASS: $LOGGER_USER can trigger a bulk conversion."
            RESULT=0
        else
            echo "  FAIL: $LOGGER_USER still cannot write it."
            echo "        If the rule was only just installed, udev may not have seen this attach."
            echo "        Re-run --verify once; if it fails again the rule is not matching."
        fi
    fi

    echo "+ removing the fake slave"
    echo "$FAKE_SLAVE_ID" | sudo tee "$MASTER_DIR/w1_master_remove" >/dev/null
    sleep 1
    [ -e "$FAKE_SLAVE_DIR" ] && echo "  WARNING: $FAKE_SLAVE_DIR is still present - remove it by hand."
    exit "$RESULT"
fi

if [ "$IS_EXECUTE" -eq 0 ] && [ "$IS_VERIFY" -eq 0 ]; then
    echo "Nothing was changed. Re-run with --execute."
fi
