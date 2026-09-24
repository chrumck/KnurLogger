# Box-2 Pi — headless setup and boot-time reduction

**Scope:** bring the box-2 Raspberry Pi 4B from a stock Raspberry Pi OS Lite install to a headless
logging host running only the services a logger needs, with the I2C and 1-Wire buses the perfboard
expects already configured.

**Subordinate to `../../ndLouvers/CFD-Learning-Plan.md`.** That plan owns requirements and
acceptance criteria; this file owns the host configuration that satisfies them, and owns no
measurement decision. Where the two disagree, the plan wins.
`logger-perfboard-wiring.md` owns pinouts, addresses and bring-up
order; this file configures the SoC side of the same pins and does not restate them.

---

## PREAMBLE — READ THIS FIRST

**After completing each step, update the `Work Progress` section at the bottom of this file**:
set the step's status and write a short note on what was actually done, what deviated, and any
values measured. Re-reading this file must always reveal where we left off.

Each numbered step is self-contained and states its own background, commands and acceptance.

**The two rules that matter here:**

1. **Every destructive script defaults to pre-flight.** Run it with no arguments first, read what
   it says it will do, then re-run with `--execute`. This is the iSitePiLogger `prepare-image.sh`
   idiom and it is deliberate.
2. **Bluetooth is never disabled on this box.** iSitePiLogger's `setupNotes.txt` sets
   `dtoverlay=disable-bt`; copying that removes the RaceChrono BLE link, which is the reason box 2
   exists. The same file sets `dtoverlay=disable-wifi`; box 2 gates Wi-Fi per session instead,
   because a wheel-well cavity has no Ethernet and Wi-Fi is the only way back in.

---

## 0. Baseline and current state

The shipped-state audit is preserved in
[CLAUDE.history.md](../CLAUDE.history.md#host-shipped-state); it is not the current machine.
[Work Progress](#work-progress) below owns setup status. Capture a fresh baseline with step 2
when applying this runbook to a new image; do not treat the old audit as a fresh pre-flight.

1. I2C requires both `dtparam=i2c_arm=on` and the `i2c-dev` module to expose `/dev/i2c-1`.
   The headless setup script installs the module-load file. 1-Wire's family alias loads `w1_therm`.
2. Run mutating scripts in a login shell with a terminal for the sudo password. Include
   `/usr/sbin` in PATH before declaring tools such as `rfkill` or `i2cdetect` missing.
3. Bluetooth soft-block state persists across reboot. Clear it with `sudo rfkill unblock bluetooth`;
   `bluetoothctl power on` cannot clear an rfkill block. There is no `hciuart.service` on this image.
4. The clock can resume from the last saved time without NTP. Use the logger's boot/session
   clock for elapsed time — do not restore GPS-time fetching; see history §2.5.

## 1. Step 1 — First login and the outstanding supply check

**Background.** Plan item 5 and build sheet §10 step 4: the Pi runs on the HW-384, which is
fixed-output and cannot be trimmed, so the undervoltage flag is the only available evidence that
the 4.8 V bottom of the vendor band plus pigtail drop clears the Pi's 4.63 V threshold. Build
sheet §10 steps 2 and 3 were retired unperformed at rev 105 (2026-09-20), so this flag and the
logger's supply telemetry are the only supply evidence there will be.

**Do.**

```bash
ssh KnurLogger 'vcgencmd get_throttled; vcgencmd measure_volts core; dmesg | grep -i -E "voltage|throttl"'
```

**Acceptance.** `throttled=0x0`. Bit 0 set means undervoltage now; bit 16 means it has occurred
since boot. Record the value either way.

**The `TP2` meter reading is retired unperformed (rev 105), not owed.** An idle reading does not
settle item 5.7: the flag has to be read under full logging load with every device running, which
is the logger's supply telemetry (`0x604` and the `supply` records) under a run.

---

## 2. Step 2 — Baseline audit, before changing anything

**Background.** Two audits that can be diffed are worth more than any opinion about what a Pi runs
at boot. The disable list in step 4 is written with existence guards rather than assumptions, but
the guards only tell you what happened if you can compare.

**Do.** The script writes nothing to the box; piping it over stdin avoids even creating a file.

```bash
ssh KnurLogger 'bash -s' < SystemSetup/audit-boot.sh > boot-audit-before.txt
```

**Acceptance.** `systemd-analyze time` reports a figure and the file is kept off the box. It is
the only record of the stock state.

---

## 3. Step 3 — Install what the build needs

**Background — and this step has been RUN; see the Work Progress table.** As the box shipped it
had `gcc` and `g++` but no `cmake`, no `git` and no GLib development package, so nothing could be
built on it. `i2c-tools` is needed for `i2cdetect` during bring-up. `rfkill` was already
installed; the script keeps it in the list as a guard and reports it present. **Re-running is
safe** — the script installs only what `dpkg-query` says is missing and exits early with
"Nothing to do" when the list is satisfied, which is what it does now.

**Do.**

```bash
./install-dependencies.sh              # pre-flight
./install-dependencies.sh --execute
```

**Acceptance.** `cmake --version`, `git --version`, `i2cdetect -V` and
`pkg-config --modversion glib-2.0` all answer.

---

## 4. Step 4 — Read the pre-flight output

**Do.**

```bash
./harden-headless.sh | less
```

No arguments means nothing executes. Read every `+` line, and in particular the **"Retained on
purpose"** block at the top — that is the answer to "what still runs at boot, and why", and it is
the part worth disagreeing with if you are going to disagree with anything.

**Acceptance.** You can say, for each retained unit, why it is retained.

---

## 5. Step 5 — Apply the boot-time reduction

**Background — and this step has been RUN; see the Work Progress table.** Eight phases, each
justified against *this* box rather than a generic Pi. The figures below are measured, from step 2,
and are therefore the *before* figures: the phases are described in the tense of a box that has
not had them applied, because that is what makes the justifications legible. What they actually
achieved is in the Work Progress table and in the corrections list after it.

1. **`NetworkManager-wait-online` (5.983 s) and cloud-init (2.53 s).** The first blocks
   `multi-user.target` and runs to its timeout in a car with no access point in range. The second
   finished its real work at first boot — the user, the SSH keys and the Wi-Fi profile persist as
   ordinary files — and every boot since has been a datasource probe finding the same answer.
   Together they are the majority of what is recoverable.
2. **apt / man-db / dpkg / e2scrub / fstrim / logrotate timers** — masked, not merely disabled,
   because apt's timers re-enable themselves on package upgrade. These cause multi-second I/O
   stalls at unpredictable moments, which on a 10 Hz logger means dropped samples.
   The before-audit caught three of them mid-stall and they are the strongest evidence in this
   section: **`fstrim.service` 3.467 s**, **`apt-daily-upgrade.service` 1.581 s** and
   **`e2scrub_reap.service` 1.092 s** — 6.14 s of I/O between them, none of it at a moment
   anybody chose. `fstrim` was the third-slowest unit on the box and went unmentioned here until
   the `--execute` run's diff surfaced it.
3. **rpi-eeprom-update (848 ms), udisks2, keyboard-setup (280 ms), console-setup** — nothing on
   this box consumes any of them, and these four are the ones that actually exist here. The script
   also names ModemManager, rsyslog, triggerhappy, bluealsa, rpcbind, nfs-client.target, cups and
   cups-browsed, and the `--execute` run reported every one of them **"not installed, skipping"**
   (correction 26). They stay in the list as guards against a future image, but nothing on *this*
   box is recovered by them — in particular there is no `rsyslog` here duplicating journald into
   `/var/log`, which an earlier draft of this section claimed as a saving.
4. **Serial console** — `serial-getty@ttyS0` off and `console=serial0` removed from
   `cmdline.txt`. `ttyAMA0` is the Bluetooth UART on a Pi 4B and is untouched.
5. **zram writeback off, zram swap kept.** Swap here is RAM-backed and costs no card wear, so
   there is nothing to gain by removing it. `rpi-zram-writeback.timer` is the exception: its job
   is to push idle zram pages onto the SD card.
6. **Write pressure, and evidence that survives a power cut** — `vm.dirty_background_ratio=5`,
   `vm.dirty_ratio=10` (iSitePiLogger's values and its reasoning; written as `99-` so it sorts
   after the image's own `98-rpi.conf`). Plan item 5b: the supply disappears without
   warning — the fuse pulled at end of day on the constant 12 V feed as built, or a cranking dip
   — so the last durable write bounds what a hard cut costs.

   **The journal setting in this phase cuts against the rest of it and is a deliberate trade.**
   The image ships `Storage=volatile` to spare the card (§0 item 8a); switching to a capped
   persistent journal *adds* a small continuous writer. It is still right here, because the
   failure this box exists to survive — the feed vanishing without warning — is exactly the event
   that destroys a volatile journal, and the journal is the only record of an undervoltage flag,
   thermal event or oops in the seconds before the cut. Deleting
   `/etc/systemd/journald.conf.d/90-knurlogger.conf` reverts to the vendor behaviour.
7. **Un-block Bluetooth, and turn Wi-Fi power save off.** The unblock is the single most
   consequential action the script takes — the radio ships soft-blocked (§0 item 7) and without
   clearing it there is no BLE and no product. Wi-Fi power save is currently unconfigured, so
   defaulting to enabled; it is a common cause of SSH stalling for seconds on an idle Pi and does
   not touch the BLE side.
8. **`config.txt` and `/etc/modules-load.d/knurlogger.conf`** — I2C1 at **100 kHz** *plus the
   `i2c-dev` module* (§0 item 5a: without both there is no `/dev/i2c-1`), `w1-gpio` on GPIO4,
   `gpio=17=op,dh`, and audio / camera / display / splash off. **A reboot is required** — the
   adapter does not exist until the firmware re-reads `config.txt`.

**`gpio=17=op,dh` is the one line that closes an open question rather than saving a resource.**
Build sheet §3a.5 item 6 records that GPIO17 boots as an input with the SoC's ~50 kΩ pull-down
fighting `R12`, and that the resulting level at the mux `~RESET` was never measured. Setting the
pin at firmware time, before Linux starts, means the mux is never held in reset by an unmeasured
divider. It does not answer the question, and does not retire that item's advice to drop `R12` to
4.7 kΩ if the level measures marginal.

**Do.**

```bash
./harden-headless.sh --execute
sudo reboot
```

Optional flags, both off by default and both with a real cost:

- `--drop-mdns` also retires `avahi-daemon`, which is what makes `KnurLogger.local` resolve.
  Reserve a static DHCP lease on the router first, or you lose a way in.
- `--no-hdmi` also drops the KMS driver. It saves a little memory and breaks the emergency console
  on a monitor — which, for a box in a wheel well, is worth more than the memory.

**Acceptance.**

```bash
ssh KnurLogger 'bash -s' < SystemSetup/audit-boot.sh > boot-audit-after.txt
diff boot-audit-before.txt boot-audit-after.txt
```

Boot time should fall from the measured 19.468 s, mostly from phase 1. Record the new figure.

**Rollback.** `config.txt` and `cmdline.txt` are backed up alongside the originals with a
timestamp suffix. Units are masked, so `sudo systemctl unmask <unit>` restores any single one.
cloud-init is re-enabled by removing `/etc/cloud/cloud-init.disabled`.

---

## 6. Step 6 — Confirm nothing needed was broken

**Background.** The failure mode of a script like this is not that it fails loudly; it is that it
removes something the logger needs three weeks later, on a day when the car is on a track.

**Do.**

```bash
ssh KnurLogger
rfkill list bluetooth      # MUST read "Soft blocked: no"
bluetoothctl show          # must NOT read PowerState: off-blocked
nmcli radio                # wifi enabled?
ls -l /dev/i2c-1 ; i2cdetect -y 1
ls /sys/bus/w1/devices/
timedatectl
```

**Acceptance.**

1. `rfkill list bluetooth` reports **`Soft blocked: no`**, and `bluetoothctl show` reports a
   controller that is **not** `PowerState: off-blocked`. **If either fails, stop** — there is no
   BLE and therefore no product. `Powered: no` on an unblocked controller is fine and is the
   logger's job to fix; `off-blocked` is not, and means phase 7 did not take.
2. `nmcli radio` shows Wi-Fi enabled, and this SSH session still works.
3. `/dev/i2c-1` **exists**, and `i2cdetect -y 1` shows **`0x70`** (the PCA9548A mux) and
   **`0x77`** (the BME280) on the main bus and nothing else; the five SDP810s answer at `0x25`
   only behind mux channels 0–4 (correction 32 for the empty-scan expectation this replaced;
   `../hardware-interface.md` before probing behind the mux — **never address channel 5**). A *missing*
   `/dev/i2c-1` is a failure, and means either the dtparam or the `i2c-dev` module did not take
   (§0 item 5a).
   **`/dev/i2c-20` and `/dev/i2c-21` also appear, and they are not yours.** They are the VC4
   display DDC buses, created by `i2c-dev` against adapters the KMS driver registers. Their
   presence proves only the module half; the perfboard is on **bus 1** and nothing else.
   Watching them appear is in fact how the two halves were told apart mid-run: loading `i2c-dev`
   before the reboot produced 20 and 21 but no 1, because the `i2c_arm` adapter does not exist
   until the firmware re-reads `config.txt`.
4. `/sys/bus/w1/devices/` exists and holds `w1_bus_master1`, plus one `28-*` entry per DS18B20
   attached — four with the car's probes plugged in, none on the bench without them. **No `00-*`
   entries**: those were phantom search results off the line while `R11` was unfitted, and they
   stopped once the sensor zone was assembled (correction 32; the phantom set is recorded in
   corrections 25 and 30). **Match only `28-*`, everywhere** — the filter stays mandatory though
   nothing on this box now exercises it — and never branch on `w1_master_slave_count`.
5. `timedatectl` shows `System clock synchronized: yes` while on Wi-Fi. **Give it a minute.**
   Run immediately after boot it reads `no` and `NTP service: active`, because timesyncd has not
   yet reached a server; that is the expected transient, not a failure.

---

## 7. Step 7 — SSH hardening (optional, and the one that can lock you out)

**Background.** iSitePiLogger's posture is key-only auth, no root login, no PAM, port 60022 and
ufw. Two of those are copied and three are not, for reasons stated in the script: `UsePAM no`
costs logind session registration for a marginal gain, and ufw guards a box that is on a home LAN
and then on no network at all.

**Run step 5 first.** While cloud-init is active it rewrites
`/etc/ssh/sshd_config.d/50-cloud-init.conf` at every boot, which would undo this script's edit to
that file. Step 5 phase 1 disables cloud-init.

**Do it from a login shell on the box** — it needs a terminal for both the confirmation prompt and
the sudo password, and refuses to run without one.

```bash
scp SystemSetup/ssh-harden.sh KnurLogger:~/
ssh -t KnurLogger './ssh-harden.sh'                          # pre-flight
ssh -t KnurLogger './ssh-harden.sh --execute'                # key-only auth on port 22
ssh -t KnurLogger './ssh-harden.sh --execute --port 60022'   # ...and add the iSitePiLogger port
```

**Password authentication WAS enabled and is now off — this step has been run** (2026-09-09; see
the Work Progress table). As the box shipped, `50-cloud-init.conf` was 27 bytes, the length of
`PasswordAuthentication yes`, and it sorts before our drop-in so it won. The script found exactly
that, printed the file's contents, and `sed`-ed the keyword to `no` in *that* file rather than only
writing its own — which is the whole point, since writing `90-knurlogger.conf` alone would have
looked like it worked and changed nothing. The file is now 26 bytes, one shorter, which is the
`yes`→`no` edit visible in the byte count.
The script reports the file's actual contents before touching it.

The script refuses to run if `~/.ssh/authorized_keys` is empty, validates with `sshd -t` before
reloading anything, keeps port 22 listening when it adds a new one, and handles Trixie's socket
activation, under which `Port` in `sshd_config` is ignored and the listener comes from
`ssh.socket`.

**Acceptance.** From a **second terminal**, while the first is still open:

```bash
ssh KnurLogger
ssh -p 60022 KnurLogger     # if a port was added
```

Only after both succeed, drop port 22 by hand.

---

## 8. Step 8 — Wi-Fi gating for a run

**Background.** Wi-Fi and BT share one radio and one antenna on the BCM43455, and background
scanning is a known source of BLE jitter.

**Decision: gating is manual** (owner, 2026-09-23). The installed link has run clean through the
track sessions with Wi-Fi left up, so nothing gates it routinely and the logger will not own it.
Gate by hand only when diagnosing a BLE link problem:

```bash
sudo rfkill block wifi         # before a run
sudo rfkill unblock wifi       # after
```

`sudo nmcli radio wifi off` does the same through NetworkManager; both are installed. Either is
preferable to `dtoverlay=disable-wifi`, which needs an SD card and a text editor to undo.

**Never use bare `rfkill block all`.** It would take the Bluetooth radio down with Wi-Fi, and
because `systemd-rfkill` persists the state it would stay down across the next reboot — which is
exactly the condition the box shipped in.

---

## 9. Not done, and deliberately

1. **Read-only root / overlayfs.** The real answer to unannounced power cuts and the correct
   long-term shape for a box whose supply vanishes without warning. Not done now because it
   complicates development on the same machine, and because plan item 5b's `fsync` cadence plus
   step 5 phase 6's dirty-page ratios already bound the loss to about a second. Revisit before the
   first event, not after.
2. **The logger systemd unit is no longer in this list** — `SystemSetup/KnurLogger.service` is
   installed, enabled and survives a reboot (Work Progress row 12). It does not copy
   `iSitePiLogger.service`'s `ExecStartPre=/usr/bin/sleep 12` readiness hack, and it must never
   name `network-online.target`, which cannot be reached on this box.
3. **Powering the Bluetooth controller.** Clearing the rfkill *block* is host configuration and
   step 5 phase 7 does it. Bringing the controller up and advertising afterwards is the logger's
   job, not this runbook's.
4. **Any measurement.** Nothing here calibrates, qualifies or commissions anything. Build sheet
   §10 steps 2, 3, 5 and 6 were retired unperformed at rev 105 (2026-09-20), and steps 7–9 are
   done in substance; step 10 and the plan's commissioning items belong to the build sheet and the
   plan, not to this file.

---

## Work Progress

| Step | Status | Notes |
|---|---|---|
| 0. Measured state | **done** 2026-09-09 | Full `audit-boot.sh` run over SSH. Pi 4B Rev 1.5 4 GB, RPi OS Lite 64-bit Trixie, kernel 6.18.34. Boot 19.468 s, `multi-user.target` 11.305 s. No failed units. |
| 1. `get_throttled` | **done (idle only)** 2026-09-09 | **`throttled=0x0`**, `volt=0.9060V` core, SoC 43.8 °C, no voltage/throttling lines in `dmesg`. Plan item 5.4 passes **at idle with no sensors attached**. Build sheet §10 step 2's `TP2` meter measurement was retired unperformed at rev 105 (2026-09-20); item 5.7's under-load reading is the logger's supply telemetry. |
| 2. Baseline audit | **done** 2026-09-09 | Piped over stdin, so nothing was written to the box. Findings folded into §0 and into the scripts — see the three corrections below. |
| 3. Install dependencies | **done** 2026-09-09 | `--execute` run from a login shell. `git 2.47.3`, `cmake 3.31.6`, `i2cdetect 4.4` and `glib-2.0 2.84.4` all answer; `build-essential` and `rfkill` were already present and were skipped. **It also upgraded 14 packages it never named** — the util-linux family, `rfkill` included — see correction 27. The box can now build. |
| 4. Read pre-flight output | **done** 2026-09-09 | All four scripts pre-flight against the box with exit 0 and no suspicious output, repeatedly, including the `--drop-mdns`, `--no-hdmi` and `--port` paths and every bad-argument case. Pre-flight changes nothing, so this does not advance step 5. |
| 5. Apply boot-time reduction | **done** 2026-09-09 | `--execute` from a login shell, all eight phases, both optional flags left off (`--drop-mdns` costs `.local` resolution, `--no-hdmi` costs the emergency console). No `FAILED to mask`, no `modprobe i2c-dev` warning. **Boot 19.468 s → 11.105 s**, userspace 17.463 → 9.108 s, `multi-user.target` 11.305 → 9.108 s. 17 units newly masked, enabled timers 9 → 2, cloud-init disabled, no failed units. Backups at `/boot/firmware/{config,cmdline}.txt.bak-20260909-123716`. Rebooted; `before`/`after` audits diffed. `NetworkManager.service` at 5.236 s is now the whole critical chain and cutting it costs the way back in. |
| 6. Confirm nothing broke | **done** 2026-09-09 | All five criteria pass. **`rfkill list bluetooth` → `Soft blocked: no`, and it survived the reboot**; `hciconfig` → `UP RUNNING`; BlueZ → `Powered: yes` / `PowerState: on`, which is better than the criterion asked for. Wi-Fi enabled and this SSH session never dropped. **`/dev/i2c-1` exists**, scanning empty as expected *at the time* — the sensor zone has since been assembled, so an empty scan is no longer the correct result; see correction 32. 1-Wire master registered — but the devices directory was **not** empty, see correction 25, itself now superseded by correction 32. `System clock synchronized: yes` after ~1 min, see correction 28. `throttled=0x0` at 56.0 °C. |
| 7. SSH hardening | **done** 2026-09-09 | `--execute` from a login shell, no `--port`. `sshd -t` reported **configuration is valid** before the reload. Effective now: `passwordauthentication no`, `kbdinteractiveauthentication no`, `permitrootlogin no`, `pubkeyauthentication yes`, `usepam yes` (deliberately kept), `port 22` listening on both stacks. `~/.ssh` 700 and `authorized_keys` 600, one key (`chrum@WielkiRig`). **Verified two ways**: the owner logged in from a second terminal while the first was open, and a forced password-only attempt from the workstation is refused with `Permission denied (publickey)` — the positive check, not just "keys still work". `ssh.service` owns the listener, so the socket-activation path was not needed. No failed units. **See correction 31: re-enabling cloud-init would undo this.** Its blocker had been cleared first: step 5 phase 1 disabled cloud-init, so `50-cloud-init.conf` is no longer rewritten at every boot. |
| 9. Full system upgrade | **done** 2026-09-09 | `apt-get full-upgrade` — **94 upgraded, 10 newly installed, 0 removed**, pre-flighted with `-s` first. Run to fix a BLE advertising failure that turned out to be a BlueZ/kernel structure mismatch (see `../CLAUDE.history.md` §1.2). `bluez 5.82-1.1+rpt1` → `+rpt2`, kernel `6.18.34` → `6.18.39`, `firmware-brcm80211` `1:20250410` → `1:20260519` — all three layers of the mismatch moved together, so **which one fixed it is unknown and deliberately not chased**. Verified after reboot: advertising works (`ActiveInstances: 1` with the logger running), `config.txt` untouched (`gpio=17=op,dh`, both bus lines intact), `pinctrl get 17` still `op -- pd | hi`, I2C shows `70` and `77`, w1 master present, `rfkill` still `Soft blocked: no`, **no failed units**, enabled timers still 2, masks survived. Audits at `boot-audit-preupgrade.txt` / `boot-audit-postupgrade.txt`. |
| 10. Production install | **done** 2026-09-10 | `deploy-logger.sh --execute`. The logger runs from `~/bin/KnurLogger` reading `~/bin/KnurLogger.ini`, separately from the build tree in `~/KnurLogger/build/`. **The two `.ini` files are not the same file and must not be conflated**: the build-tree one is the git-tracked template, the `~/bin` one carries the real ROM ID bindings and calibration offsets and is never overwritten by a deploy or by the `tar` build sync. `KnurLogger.service` points at `/home/chrum/bin/KnurLogger`; see step 12. `~/bin/KnurLogger.ini` holds the four ROM ID bindings enrolled 2026-09-10 and four zero offsets (the cold-soak calibration's result). |
| 11. 1-Wire bulk-read permission | **done** 2026-09-10 | `grant-w1-bulk-read.sh --execute --verify` from a login shell. The udev rule gives the `gpio` group write access to `w1_bus_master*/therm_bulk_read`, which `w1_therm` registers `0644 root:root`; without it the logger's write is refused with `EACCES` (errno 13) and four probes sample at **0.31 Hz instead of ~1 Hz**. **Verified against a fake probe, not a real one**: `--verify` writes a family-0x28 id to `w1_master_add`, the attribute appears as `root:gpio 664`, `chrum` writes it successfully, and the fake slave is removed again — bus left with the master alone. **The rate did NOT change and the permission was not the whole fault** (measured with four real probes the same day): 331 cycles at 3190–3309 ms, every probe still ~800 ms. **The other half was found 2026-09-11 and is in the logger, not here:** the trigger must be written as eight bytes (`"trigger
"`), because `therm_bulk_read_store` gates on `size == sizeof("trigger")`. With both in place, 123 cycles at a 1023 ms mean interval — **0.977 Hz** — and one 762–790 ms conversion for all four probes. Parasite power was the standing suspect and was wrong. The rule is correct and stays. Plan open item 43 and `../CLAUDE.history.md` §1.15. |
| 12. KnurLogger.service | **done** 2026-09-10 | Installed to `/etc/systemd/system/`, `daemon-reload`, `enable --now`, and **verified across a reboot** — `active` and `enabled`, all four channels bound at startup, BLE advertising. It runs `/home/chrum/bin/KnurLogger` in log mode as `chrum`. **Consequence for anything done by hand from now on: the logger is ALREADY RUNNING whenever the box is powered.** Two instances both poll the 1-Wire bus, each read triggering its own conversion, which roughly doubles cycle time and would corrupt any timing measurement — so `sudo systemctl stop KnurLogger` before running the binary by hand, `--enroll` included. Live view is `journalctl -u KnurLogger -f`. |
| 8. Wi-Fi gating | **done — manual** | Owner decision 2026-09-23: gate by hand with `rfkill block wifi` / `nmcli radio wifi off` only when diagnosing a BLE link problem; the logger does not own it. |

### Setup incident history

The dated audit and execution corrections are in
[CLAUDE.history.md — host setup corrections](../CLAUDE.history.md#host-setup-corrections).
The procedures above retain the applicable technique; this runbook owns current setup and progress.

### Script review status

The four scripts have been through **five review passes** (2026-09-09; history revs 66–66d in the
plan's companion file). What that did and did not establish:

1. **Established.** They pre-flight cleanly against the real box; `shellcheck` 0.11.0 at `-S style`
   reports only an informational SC1091; Phase 8 is idempotent across three runs against the real
   `config.txt`; every unit they mask is `WantedBy` something and `RequiredBy` nothing; every
   config file they write wins its ordering contest; every unit named in the "Retained on purpose"
   block exists.
2. **Now also established, by running them.** `install-dependencies.sh --execute` and
   `harden-headless.sh --execute` have both been run, followed by a reboot and a diffed re-audit
   (2026-09-09). Every phase completed, no unit failed to mask, the box came back, and the five
   step 6 acceptance criteria pass. That run produced corrections 25–29, and every one of them is
   a thing the five static passes could not have reached: what the kernel does with a bare 1-Wire
   bus, which units this image actually ships, what apt drags in behind a four-package list, how
   long timesyncd takes, and which retained units are D-Bus-activated rather than booted. The
   prediction that the remaining unknowns were all on the far side of `--execute` held.
3. **All four scripts have now been run with `--execute`** (2026-09-09), `ssh-harden.sh` last and
   verified from a second terminal plus a forced password-only attempt. **Since then the host has
   carried the logger under load** — BLE to RaceChrono, a 38.09-hour continuous session, two
   track days and, since 2026-09-19, the 10 Hz pressure worker — with `throttled` logged every second by the logger's supply
   worker.

### Open items owned by this file

1. **`avahi-daemon` keep or drop** — kept by default. Dropping needs a static DHCP reservation.
2. **Read-only root** — see §9 item 1. The decision remains open; the original pre-event
   deadline is stale because events have already run. Do not infer acceptance or retirement from that.

**I2C bus speed is not an open item.** Build sheet §6 specifies 100 kHz and owns the decision.
Raising it needs a segment-capacitance measurement or lower pull-ups, and a change to the build
sheet first — not a change here.
