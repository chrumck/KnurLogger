#!/bin/bash
# ssh-harden.sh — key-only SSH on the box-2 logger, without locking yourself out.
#
# Target: Raspberry Pi OS Lite 64-bit (Trixie). Run as the login user; uses sudo.
#
# Usage: ./ssh-harden.sh [--execute] [--port N]
#   default      pre-flight: prints everything, changes nothing
#   --execute    apply
#   --port N     also listen on N (iSitePiLogger uses 60022). Port 22 is KEPT.
#                Drop 22 by hand once N is proven, per the note printed at the end.
#
# This follows iSitePiLogger's setupNotes.txt with two deliberate differences,
# both explained where they occur: UsePAM is left alone, and port 22 survives
# the run that adds the new one.
#
# Trixie trap this script handles: Debian 13 may run sshd under socket
# activation, in which case `Port` in sshd_config is ignored entirely and the
# listening port comes from ssh.socket instead.

set -euo pipefail

# /usr/sbin is absent from PATH both for a non-interactive SSH session and for a
# non-root login shell on Debian. sysctl, rfkill, swapon and i2cdetect all live
# there, so without this line every check for them silently reports "missing" —
# including right after install-dependencies.sh has installed them.
PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH"

DRY_RUN=true
NEW_PORT=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --execute) DRY_RUN=false; shift ;;
        --port)
            # Check for the value before shifting past it. `shift 2` with only
            # one argument left fails, and under `set -e` that killed the script
            # with no output at all on either stream — a silent exit 1 from a
            # typo, on the script that can lock you out of the box.
            if [[ $# -lt 2 ]]; then
                echo "--port requires a port number, e.g. --port 60022" >&2
                exit 2
            fi
            NEW_PORT="$2"
            shift 2
            ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [[ -n "$NEW_PORT" ]]; then
    if [[ ! "$NEW_PORT" =~ ^[0-9]+$ ]]; then
        echo "--port needs a number, got: $NEW_PORT" >&2; exit 2
    fi
    if (( NEW_PORT < 1 || NEW_PORT > 65535 )); then
        echo "--port must be 1-65535, got: $NEW_PORT" >&2; exit 2
    fi
fi

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

# ─── Guard — refuse to disable passwords with no working key ─────────────────

section "Pre-check — is there a key to fall back on?"

# Refuse to run as root. Everything below is relative to $HOME/.ssh, so under
# `sudo ./ssh-harden.sh` this would inspect and chmod ROOT's keys while claiming
# to have verified the login user's — and then disable password authentication
# on that basis.
if [[ "$(id -u)" -eq 0 ]]; then
    echo "Run this as your normal login user, not as root or under sudo." >&2
    echo "It reads \$HOME/.ssh/authorized_keys, and as root that is the wrong file." >&2
    exit 1
fi

AUTH_KEYS="$HOME/.ssh/authorized_keys"

if [[ ! -s "$AUTH_KEYS" ]]; then
    cat >&2 <<'ABORT'
  ~/.ssh/authorized_keys is missing or empty.

  Disabling password authentication now would lock you out of a headless box.
  Install your key first, from the workstation you intend to use:

      ssh-copy-id -i ~/.ssh/id_rsa.pub <user>@<host>

  then run this script again.
ABORT
    exit 1
fi

echo "  $AUTH_KEYS holds $(grep -cvE '^\s*(#|$)' "$AUTH_KEYS") key(s):"
while read -r _type _key comment; do
    [[ -n "${_type:-}" ]] && echo "    - ${comment:-(no comment)}"
done < <(grep -vE '^\s*(#|$)' "$AUTH_KEYS")

echo ""
echo "  Confirm you have logged in with one of these keys IN THIS SESSION,"
echo "  not with a password. If you are unsure, exit and reconnect with"
echo "      ssh -o PreferredAuthentications=publickey <user>@<host>"
if [[ "$DRY_RUN" == false ]]; then
    # Refuse to proceed without a terminal. Piped in over `ssh host 'bash -s'`,
    # this read would consume the script's own remaining text as the answer.
    if [[ ! -t 0 ]]; then
        echo "" >&2
        echo "  stdin is not a terminal, so the confirmation cannot be asked." >&2
        echo "  This script locks out password authentication and must not run" >&2
        echo "  unattended. Copy it to the box and run it from a login shell:" >&2
        echo "" >&2
        echo "      scp SystemSetup/ssh-harden.sh KnurLogger:~/ && ssh -t KnurLogger './ssh-harden.sh --execute'" >&2
        exit 1
    fi
    read -rp "  Key authentication confirmed working? [y/N] " yn
    [[ "$yn" =~ ^[Yy]$ ]] || { echo "  Aborted."; exit 1; }
fi

if [[ "$DRY_RUN" == false ]]; then
    require_sudo
fi

run chmod 700 "$HOME/.ssh"
run chmod 600 "$AUTH_KEYS"

# ─── Socket activation ───────────────────────────────────────────────────────

section "Detecting how sshd listens"

SOCKET_ACTIVATED=false
if systemctl is-enabled ssh.socket >/dev/null 2>&1; then
    SOCKET_ACTIVATED=true
    echo "  ssh.socket is enabled — sshd is SOCKET ACTIVATED."
    echo "  The Port directive in sshd_config is ignored in this mode; the"
    echo "  listening port comes from ssh.socket's ListenStream."
else
    echo "  ssh.service owns the listener; sshd_config's Port applies."
fi

# ─── Drop-in configuration ───────────────────────────────────────────────────

section "Writing /etc/ssh/sshd_config.d/90-knurlogger.conf"

# A drop-in rather than an edit to sshd_config: Trixie's stock file starts with
# `Include /etc/ssh/sshd_config.d/*.conf`, and OpenSSH takes the FIRST value it
# obtains for a keyword, so an included file wins over the defaults below it.
#
# UsePAM is deliberately left at the distribution default. iSitePiLogger sets
# `UsePAM no`; on a systemd host that also stops SSH logins being registered
# with logind, which costs XDG_RUNTIME_DIR and `systemctl --user`. The security
# gain over PasswordAuthentication=no alone is not worth that here.

SSHD_DROPIN=/etc/ssh/sshd_config.d/90-knurlogger.conf

# Re-running without --port would otherwise rewrite the drop-in without the Port
# lines a previous run added, silently dropping the box back to 22 only.
if [[ -z "$NEW_PORT" && -f "$SSHD_DROPIN" ]]; then
    # sudo -n so a pre-flight run never prompts for a password, and awk rather
    # than grep -P so this does not depend on a PCRE-enabled grep.
    EXISTING_PORT="$(sudo -n awk '$1=="Port" && $2!="22" {print $2; exit}' "$SSHD_DROPIN" 2>/dev/null || true)"
    if [[ -n "${EXISTING_PORT:-}" ]]; then
        echo "  $SSHD_DROPIN already listens on port $EXISTING_PORT — preserving it."
        echo "  (Pass --port $EXISTING_PORT explicitly to be sure, or edit the file to drop it.)"
        NEW_PORT="$EXISTING_PORT"
    fi
fi

echo "  + writing $SSHD_DROPIN"
if [[ "$DRY_RUN" == false ]]; then
    sudo mkdir -p /etc/ssh/sshd_config.d
    {
        echo "# KnurLogger. Key-only access; see SystemSetup/pi-headless-setup.md."
        echo "PasswordAuthentication no"
        echo "KbdInteractiveAuthentication no"
        echo "PermitRootLogin no"
        echo "PubkeyAuthentication yes"
        if [[ -n "$NEW_PORT" && "$SOCKET_ACTIVATED" == false ]]; then
            echo "Port 22"
            echo "Port $NEW_PORT"
        fi
    } | sudo tee "$SSHD_DROPIN" >/dev/null
fi

# Raspberry Pi OS ships a cloud-init drop-in that re-enables passwords. It is
# included from the same directory and sorts BEFORE 90-, so it would win.
CLOUD_INIT=/etc/ssh/sshd_config.d/50-cloud-init.conf
if [[ -f "$CLOUD_INIT" ]]; then
    echo "  $CLOUD_INIT exists and sorts BEFORE ours, so it wins on any keyword it sets."
    if [[ "$DRY_RUN" == false ]]; then
        echo "  its current contents:"
        sudo sed 's/^/      /' "$CLOUD_INIT"
    fi
    echo "  + sudo sed -i 's/^PasswordAuthentication.*/PasswordAuthentication no/' $CLOUD_INIT"
    if [[ "$DRY_RUN" == false ]]; then
        if sudo grep -q '^PasswordAuthentication' "$CLOUD_INIT"; then
            sudo sed -i 's/^PasswordAuthentication.*/PasswordAuthentication no/' "$CLOUD_INIT"
            echo "  neutralised."
        else
            echo "  it sets no PasswordAuthentication line; ours applies unopposed."
        fi
    fi
    # Ordering note: cloud-init rewrites this file at boot while it is active.
    # harden-headless.sh phase 1 disables cloud-init, so run that FIRST or this
    # edit can be undone at the next boot.
    echo "  NOTE: run harden-headless.sh first — it disables cloud-init, which would"
    echo "        otherwise rewrite this file at the next boot and undo the edit."
fi

if [[ -n "$NEW_PORT" && "$SOCKET_ACTIVATED" == true ]]; then
    echo "  + adding port $NEW_PORT to ssh.socket (Port in sshd_config would be ignored)"
    if [[ "$DRY_RUN" == false ]]; then
        sudo mkdir -p /etc/systemd/system/ssh.socket.d
        sudo tee /etc/systemd/system/ssh.socket.d/90-knurlogger-port.conf >/dev/null <<SOCKETCONF
[Socket]
ListenStream=
ListenStream=22
ListenStream=$NEW_PORT
SOCKETCONF
        sudo systemctl daemon-reload
    fi
fi

# ─── Validate before reloading ───────────────────────────────────────────────

section "Validating the configuration BEFORE it takes effect"

echo "  + sudo sshd -t"
if [[ "$DRY_RUN" == false ]]; then
    if ! sudo sshd -t; then
        echo "" >&2
        echo "  sshd rejected the configuration. Nothing has been reloaded, so your" >&2
        echo "  current session and the running daemon are unaffected." >&2
        echo "  Remove $SSHD_DROPIN and investigate." >&2
        exit 1
    fi
    echo "  configuration is valid"
fi

if [[ "$SOCKET_ACTIVATED" == true ]]; then
    run sudo systemctl restart ssh.socket
else
    run sudo systemctl reload ssh
fi

# ─── Report ──────────────────────────────────────────────────────────────────

section "Effective settings"

echo "  + sudo sshd -T | grep -E 'passwordauthentication|permitrootlogin|kbdinteractive|usepam|^port'"
if [[ "$DRY_RUN" == false ]]; then
    sudo sshd -T 2>/dev/null | grep -E 'passwordauthentication|permitrootlogin|kbdinteractive|usepam|^port' || true
    echo ""
    echo "  -- actually listening --"
    # Filter on the ports, not on a process name: under socket activation the
    # listener belongs to systemd and "ssh" does not appear in the row at all.
    sudo ss -lntp 2>/dev/null | grep -E "LISTEN|:22|:${NEW_PORT:-22}" || true
fi

section "Done — do not close this session yet"
cat <<'DONE'
Open a SECOND terminal and confirm you can still get in before you disconnect
this one. That is the whole safety net:

    ssh <user>@<host>

If a port was added, prove it too:

    ssh -p <port> <user>@<host>

Only once the new port is proven, drop 22 by editing
/etc/ssh/sshd_config.d/90-knurlogger.conf (or the ssh.socket drop-in under
/etc/systemd/system/ssh.socket.d/ when socket-activated), then reload.

Not done here, and deliberately: ufw. iSitePiLogger's setupNotes.txt installs it
because those boxes sit on ~1000 customer networks. This one sits on your home
LAN and then in a car with no network at all, where a firewall guards nothing
and is one more thing that can strand a headless box. Add it if the Pi ever gets
a route to the internet.
DONE
