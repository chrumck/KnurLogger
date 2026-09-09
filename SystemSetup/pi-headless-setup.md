# Box-2 Pi — headless setup and boot-time reduction

**Scope:** bring the box-2 Raspberry Pi 4B from a stock Raspberry Pi OS Lite install to a headless
logging host running only the services a logger needs, with the I2C and 1-Wire buses the perfboard
expects already configured.

**Subordinate to `../../ndLouvers/CFD-Learning-Plan.md`.** That plan owns requirements and
acceptance criteria; this file owns the host configuration that satisfies them, and owns no
measurement decision. Where the two disagree, the plan wins.
`../../ndLouvers/step0b-rig/logger-perfboard-wiring.md` owns pinouts, addresses and bring-up
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

## 0. Measured state of the box — **as it shipped, before any change**

Read off the machine on 2026-09-09 by `audit-boot.sh`. These are measurements, not assumptions;
the full output is the artefact this section summarises.

> **⚠ This section is the BEFORE state and is deliberately frozen.** Steps 3 and 5 have since
> been run (2026-09-09). It is kept as written because it is the baseline the after-audit is
> diffed against, and rewriting it would destroy the only record of what the box shipped as.
> **Do not read it as current.** Four items in it are now false by design — item 5 (buses not
> configured), item 6 (missing tools), item 7 (Bluetooth soft-blocked) and item 8a (volatile
> journal) are exactly what steps 3 and 5 changed. **Item 9 (sensor zone not built) is now false
> too**, but by the owner assembling the board rather than by anything in this runbook — see
> correction 32, which retires the empty-bus expectations that item 9 justified. **The `Work Progress` table at the bottom of
> this file is the authority on current state**, and the corrections sections after it record
> what running the scripts actually found.

1. **Hardware:** Raspberry Pi 4 Model B Rev 1.5, 4 GB. Powered from the HW-384 buck module through
   the USB-C pigtail (`J1`); the GPIO 5 V pins are not in the power path.
2. **OS:** Raspberry Pi OS Lite 64-bit on Debian 13 (trixie), kernel `6.18.34+rpt-rpi-v8`.
   `/boot/firmware/config.txt` is the boot config path. NetworkManager is the network stack.
   Root filesystem 118 GB, 4% used.
3. **Access:** hostname `KnurLogger`, SSH alias `KnurLogger` → `192.168.118.52`, user `chrum`,
   key-only.
4. **Boot: 19.468 s total** (2.004 s kernel + 17.463 s userspace), `multi-user.target` at
   11.305 s. The slowest units were `NetworkManager-wait-online` at **5.983 s**, NetworkManager
   itself at 5.565 s, and cloud-init at **2.53 s across five units**.
5. **Buses are not configured.** No `/dev/i2c-*`, no `w1` bus, and `config.txt` is stock.
5a. **I2C needs two things, 1-Wire needs one.** `dtparam=i2c_arm=on` registers the adapter but
   does **not** create `/dev/i2c-1`; that needs the `i2c-dev` module, and nothing on this image
   loads it — `/etc/modules` holds only comments and there is no modalias path for it.
   `raspi-config`'s `do_i2c` does both steps. 1-Wire has no equivalent gap: `w1_therm` carries the
   alias `w1-family-0x28`, so the w1 core loads it on discovering a DS18B20, and `do_onewire`
   accordingly only writes the overlay line.
5b. **`sudo` requires a password.** Pre-flight runs pipe fine over `ssh host 'bash -s'`; any
   `--execute` run must be done from a login shell on the box, because a piped stdin has no
   terminal to type a password into.
6. **Missing tools:** `i2c-tools`, `cmake`, `git`, and the GLib development package. `gcc`,
   `g++`, `pkg-config`, `nmcli`, `bluetoothctl` and **`rfkill`** are present — `rfkill` lives in
   `/usr/sbin`, which is not on `PATH` for a non-interactive SSH session or a non-root login
   shell on Debian, so a naive `command -v` reports it missing when it is not. Every script here
   prepends the sbin directories for that reason. `i2cdetect` will land in `/usr/sbin` too.
7. **Bluetooth is SOFT-BLOCKED, and this is the most consequential thing on the box.** `hci0`
   exists on the UART bus, `D8:3A:DD:3C:50:6E`, but `rfkill list` reports `Soft blocked: yes` and
   BlueZ reports `PowerState: off-blocked`. **BLE is the product, and in this state there is
   none.** Two traps: `bluetoothctl power on` cannot clear a soft block, because rfkill sits below
   BlueZ; and the block is **persistent** — `systemd-rfkill` saves per-device state under
   `/var/lib/systemd/rfkill/` and restores it at boot, so it survives reboots. `sudo rfkill
   unblock bluetooth` clears it and is saved back the same way. Step 5 phase 7 does this.
   There is no `hciuart.service` on this image — the controller is attached by udev.
8. **Swap is zram**, `/dev/zram0`, 2 GB, priority 100. There is no `dphys-swapfile`.
8a. **The journal is currently VOLATILE.** Raspberry Pi OS ships
   `/usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf` setting `Storage=volatile`, so
   the journal lives in `/run` (8 MB there now) and never touches the card. Step 5 phase 6
   **deliberately overrides that vendor SD-protection default** — see the trade recorded there.
8b. **`fake-hwclock` is not installed and the clock in the car will be wrong.** A Pi 4B has no
   RTC. `systemd-timesyncd` saves the time to `/var/lib/systemd/timesync/clock` and restores it at
   boot, so nothing is stamped 1970 — but with no NTP in the car the clock resumes from the last
   bench sync and is wrong by however long ago that was, while looking plausible. **The logger
   must record an offset against an external time source at session start** rather than trust the
   Pi clock for anything that must line up with RaceChrono data. This is a logger requirement, not
   a host one, and it is recorded in `../CLAUDE.md`.
9. **The sensor zone is not built.** Only the perfboard's power zone is soldered, so an empty I2C
   scan and an empty 1-Wire directory are the expected results throughout, not failures.

---

## 1. Step 1 — First login and the outstanding supply check

**Background.** Plan item 5.4 and build sheet §10 step 4: the Pi runs on the HW-384, which is
fixed-output and cannot be trimmed, so the undervoltage flag is the only available evidence that
the 4.8 V bottom of the vendor band plus pigtail drop clears the Pi's 4.63 V threshold. Build
sheet §10 steps 2 and 3 were bypassed rather than passed; this is the cheap half of what they
would have caught.

**Do.**

```bash
ssh KnurLogger 'vcgencmd get_throttled; vcgencmd measure_volts core; dmesg | grep -i -E "voltage|throttl"'
```

**Acceptance.** `throttled=0x0`. Bit 0 set means undervoltage now; bit 16 means it has occurred
since boot. Record the value either way.

**This does not retire build sheet §10 step 2.** A meter at `TP2` and at the Pi's USB-C end is a
separate, physical measurement, and it is still owed. Nor does an idle reading settle item 5.7:
the flag has to be re-read under full logging load with every device running, which cannot happen
until there is a logger.

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
   after the image's own `98-rpi.conf`). Plan item 5b: the accessory feed disappears without
   warning at ignition-off, so the last durable write bounds what a hard cut costs.

   **The journal setting in this phase cuts against the rest of it and is a deliberate trade.**
   The image ships `Storage=volatile` to spare the card (§0 item 8a); switching to a capped
   persistent journal *adds* a small continuous writer. It is still right here, because the
   failure this box exists to survive — the feed vanishing at ignition-off — is exactly the event
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
3. `/dev/i2c-1` **exists**. An empty `i2cdetect -y 1` scan is expected — the sensor zone is not
   built (§0 item 9) — but a *missing* `/dev/i2c-1` is a failure, and means either the dtparam or
   the `i2c-dev` module did not take (§0 item 5a).
   **`/dev/i2c-20` and `/dev/i2c-21` also appear, and they are not yours.** They are the VC4
   display DDC buses, created by `i2c-dev` against adapters the KMS driver registers. Their
   presence proves only the module half; the perfboard is on **bus 1** and nothing else.
   Watching them appear is in fact how the two halves were told apart mid-run: loading `i2c-dev`
   before the reboot produced 20 and 21 but no 1, because the `i2c_arm` adapter does not exist
   until the firmware re-reads `config.txt`.
4. `/sys/bus/w1/devices/` exists. **It is not empty, and an empty one would be the failure.**
   A working bus always holds `w1_bus_master1`, and with no probes attached this box also shows
   **phantom `00-*` slaves whose identities and count both churn between the 10 s bus scans** —
   measured across 35 s: `00-800000000000` alone, then `00-dc0000000000` + `00-3c0000000000`,
   then `00-3c0000000000` + `00-bc0000000000`, with `w1_master_slave_count` reading `1`, `2`, `2`.
   Family code `00` is not a valid 1-Wire family and a DS18B20 is family `28`, so these are
   search results off a floating line, not devices. **Match only `28-*`, everywhere** (corrections
   25 and 30). `w1_master_slave_count` is **unstable**, not merely off by one: nothing may branch
   on it.
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

**Background.** Plan item 1b: Wi-Fi and BT share one radio and one antenna on the BCM43455, and
background scanning is a known source of BLE jitter. Item 5a wants connection events, supervision
timeouts and notify-sequence gaps logged during the installed link check — which is the test that
would actually show whether scanning matters on this car.

**Decision, open.** Whether Wi-Fi is blocked for a session, and whether the logger owns that or you
do, is not settled. Until it is, by hand:

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
   long-term shape for a box whose supply vanishes at ignition-off. Not done now because it
   complicates development on the same machine, and because plan item 5b's `fsync` cadence plus
   step 5 phase 6's dirty-page ratios already bound the loss to about a second. Revisit before the
   first event, not after.
2. **Installing the logger systemd unit.** The unit is **written** — `SystemSetup/KnurLogger.service`
   — but deliberately **not installed**, because installing it needs `sudo` and the logger should
   earn a boot-time start on the bench first. It does not copy `iSitePiLogger.service`'s
   `ExecStartPre=/usr/bin/sleep 12` readiness hack, and it must never name
   `network-online.target`, which cannot be reached on this box.
3. **Powering the Bluetooth controller.** Clearing the rfkill *block* is host configuration and
   step 5 phase 7 does it. Bringing the controller up and advertising afterwards is the logger's
   job, not this runbook's.
4. **Any measurement.** Nothing here calibrates, qualifies or commissions anything. Build sheet
   §10 steps 2, 3 and 5–10 and the plan's commissioning items are untouched.

---

## Work Progress

| Step | Status | Notes |
|---|---|---|
| 0. Measured state | **done** 2026-09-09 | Full `audit-boot.sh` run over SSH. Pi 4B Rev 1.5 4 GB, RPi OS Lite 64-bit Trixie, kernel 6.18.34. Boot 19.468 s, `multi-user.target` 11.305 s. No failed units. |
| 1. `get_throttled` | **done (idle only)** 2026-09-09 | **`throttled=0x0`**, `volt=0.9060V` core, SoC 43.8 °C, no voltage/throttling lines in `dmesg`. Plan item 5.4 passes **at idle with no sensors attached**. Does not retire build sheet §10 step 2's `TP2` meter measurement, and item 5.7's under-load re-read is impossible until a logger exists. |
| 2. Baseline audit | **done** 2026-09-09 | Piped over stdin, so nothing was written to the box. Findings folded into §0 and into the scripts — see the three corrections below. |
| 3. Install dependencies | **done** 2026-09-09 | `--execute` run from a login shell. `git 2.47.3`, `cmake 3.31.6`, `i2cdetect 4.4` and `glib-2.0 2.84.4` all answer; `build-essential` and `rfkill` were already present and were skipped. **It also upgraded 14 packages it never named** — the util-linux family, `rfkill` included — see correction 27. The box can now build. |
| 4. Read pre-flight output | **done** 2026-09-09 | All four scripts pre-flight against the box with exit 0 and no suspicious output, repeatedly, including the `--drop-mdns`, `--no-hdmi` and `--port` paths and every bad-argument case. Pre-flight changes nothing, so this does not advance step 5. |
| 5. Apply boot-time reduction | **done** 2026-09-09 | `--execute` from a login shell, all eight phases, both optional flags left off (`--drop-mdns` costs `.local` resolution, `--no-hdmi` costs the emergency console). No `FAILED to mask`, no `modprobe i2c-dev` warning. **Boot 19.468 s → 11.105 s**, userspace 17.463 → 9.108 s, `multi-user.target` 11.305 → 9.108 s. 17 units newly masked, enabled timers 9 → 2, cloud-init disabled, no failed units. Backups at `/boot/firmware/{config,cmdline}.txt.bak-20260909-123716`. Rebooted; `before`/`after` audits diffed. `NetworkManager.service` at 5.236 s is now the whole critical chain and cutting it costs the way back in. |
| 6. Confirm nothing broke | **done** 2026-09-09 | All five criteria pass. **`rfkill list bluetooth` → `Soft blocked: no`, and it survived the reboot**; `hciconfig` → `UP RUNNING`; BlueZ → `Powered: yes` / `PowerState: on`, which is better than the criterion asked for. Wi-Fi enabled and this SSH session never dropped. **`/dev/i2c-1` exists**, scanning empty as expected *at the time* — the sensor zone has since been assembled, so an empty scan is no longer the correct result; see correction 32. 1-Wire master registered — but the devices directory was **not** empty, see correction 25, itself now superseded by correction 32. `System clock synchronized: yes` after ~1 min, see correction 28. `throttled=0x0` at 56.0 °C. |
| 7. SSH hardening | **done** 2026-09-09 | `--execute` from a login shell, no `--port`. `sshd -t` reported **configuration is valid** before the reload. Effective now: `passwordauthentication no`, `kbdinteractiveauthentication no`, `permitrootlogin no`, `pubkeyauthentication yes`, `usepam yes` (deliberately kept), `port 22` listening on both stacks. `~/.ssh` 700 and `authorized_keys` 600, one key (`chrum@WielkiRig`). **Verified two ways**: the owner logged in from a second terminal while the first was open, and a forced password-only attempt from the workstation is refused with `Permission denied (publickey)` — the positive check, not just "keys still work". `ssh.service` owns the listener, so the socket-activation path was not needed. No failed units. **See correction 31: re-enabling cloud-init would undo this.** Its blocker had been cleared first: step 5 phase 1 disabled cloud-init, so `50-cloud-init.conf` is no longer rewritten at every boot. |
| 9. Full system upgrade | **done** 2026-09-09 | `apt-get full-upgrade` — **94 upgraded, 10 newly installed, 0 removed**, pre-flighted with `-s` first. Run to fix a BLE advertising failure that turned out to be a BlueZ/kernel structure mismatch (see `../CLAUDE.md`). `bluez 5.82-1.1+rpt1` → `+rpt2`, kernel `6.18.34` → `6.18.39`, `firmware-brcm80211` `1:20250410` → `1:20260519` — all three layers of the mismatch moved together, so **which one fixed it is unknown and deliberately not chased**. Verified after reboot: advertising works (`ActiveInstances: 1` with the logger running), `config.txt` untouched (`gpio=17=op,dh`, both bus lines intact), `pinctrl get 17` still `op -- pd | hi`, I2C shows `70` and `77`, w1 master present, `rfkill` still `Soft blocked: no`, **no failed units**, enabled timers still 2, masks survived. Audits at `boot-audit-preupgrade.txt` / `boot-audit-postupgrade.txt`. |
| 8. Wi-Fi gating | **decision open** | Manual `nmcli`/`rfkill block wifi` for now. Still waiting on plan item 5a's installed link check to have been *run* — but 5a itself is **no longer blocked**, since clearing the Bluetooth soft block was its precondition and step 5 phase 7 did that. What 5a now waits on is a logger binary, not this file. |

### Corrections the first real audit forced

1. **`wpa_supplicant.service` must be left alone.** It is enabled and running, and NetworkManager
   drives it over D-Bus. The script's original rule — retire it if enabled — would have taken
   Wi-Fi off a box whose only access route is Wi-Fi.
2. **Swap is zram, not a file.** `dphys-swapfile` does not exist here, and `swapoff -a` would have
   disabled a RAM-backed buffer that costs no card wear. Only `rpi-zram-writeback.timer` is worth
   retiring.
3. **cloud-init is present and was not in the original list.** 2.53 s across five units, every
   boot, re-deriving a first-boot answer that is already on disk.
4. **I2C was set to 400 kHz, contradicting build sheet §6, which specifies 100 kHz** and gives its
   reasoning. Copied from iSitePiLogger, whose bus carries different parts. Corrected to 100 kHz.
   The build sheet's throughput argument is the weakest of the three that support it: the binding
   one is rise time, since the mandatory 4.7 kΩ per-channel pull-ups leave only ~75 pF of budget
   against fast mode's 300 ns limit, against ~250 pF at 100 kHz.

### Corrections from the second review pass (2026-09-09)

Found by re-reading the scripts against the box rather than against intent.

5. **`/usr/sbin` is not on `PATH`** for a non-interactive SSH session or a non-root Debian login
   shell. `sysctl`, `rfkill`, `swapon` and `i2cdetect` all live there, so every check for them
   reported "unavailable" whether or not they existed — the first audit's dirty-page section
   printed **empty** and nobody noticed. All four scripts now prepend the sbin directories.
   This also retires the claim that `rfkill` was missing: it was installed all along.
6. **`retire_unit` would have aborted the run.** `rpi-zram-writeback.service` is `static` and
   `rpi-zram-writeback.timer` is `generated`; systemd refuses to `disable` a unit with no
   `[Install]` section. Under `set -e` that failure in phase 5 would have skipped phases 6, 7 and
   8 — every `config.txt` bus setting — while looking like a clean stop. `disable` is now
   tolerated and `mask`, which is the step that actually works, is the one whose status is checked.
7. **`unit_exists` was blind to template instances.** `list-unit-files serial-getty@ttyS0.service`
   returns nothing for an uninstantiated instance, so it was silently skipped. Now falls back to
   `LoadState`.
8. **The `dmesg` check could report a false pass.** `dmesg 2>/dev/null | grep … || echo "(no
   matches — good)"` prints the reassuring branch both when there is nothing to report and when
   `dmesg` cannot be read at all. Plan item 5.4 treats this as pass/fail, so the two are now
   distinguished. (`kernel.dmesg_restrict=0` here, so it does read — but that is luck, not design.)
9. **`cloud-init status` exits 2 while printing `status: done`,** so the exit code cannot stand in
   for whether it answered. The audit printed both the answer and "status unavailable".
10. **`ssh-harden.sh` would have consumed its own text as the confirmation** if piped in over
   `ssh host 'bash -s'`, the idiom the runbook uses for `audit-boot.sh`. It now refuses to run
   without a terminal.

### Corrections from the third review pass (2026-09-09)

11. **`/dev/i2c-1` would never have appeared.** `dtparam=i2c_arm=on` registers the adapter; the
   `i2c-dev` module is what exposes it to userspace, and nothing on this image loads it.
   `raspi-config`'s `do_i2c` does both; the script did only the first. Bring-up — build sheet §10
   steps 5, 7 and 8, all of which are `i2cdetect` work — would have been blocked with no obvious
   cause. Now writes `/etc/modules-load.d/knurlogger.conf`. **1-Wire needed no such fix**, and
   checking why is what surfaced the asymmetry: `w1_therm` is loaded by modalias.
12. **`dtoverlay=w1-gpio,gpiopin=4,pullup=0` — the `pullup` parameter is ignored.** The overlays
   README on this image states "Now enabled by default (ignored)". The comment justifying it was
   wrong on its own terms: the strong-pullup form is a *separate overlay*, `w1-gpio-pullup`, which
   this build must not use because `R11` is a plain resistor. Parameter dropped, comment corrected.
13. **`sudo` needs a password, and the tolerated `disable` failures would have hidden that.** An
   `--execute` run piped over SSH would have failed at the first `sudo` and surfaced as
   "FAILED to mask", pointing at the wrong thing entirely. All three mutating scripts now check
   sudo up front and refuse clearly when there is no terminal to authenticate from.
14. **Appending to a `config.txt` with no trailing newline** would have glued the managed block's
   first marker onto the last stock line, breaking both that line and the idempotent removal on
   the next run. This file does end with a newline today, so it was latent. Guarded.
15. **Re-running `ssh-harden.sh` without `--port` silently reverted the box to port 22 only**,
   because it rewrites the drop-in from scratch. It now detects and preserves an existing port.
16. **`ss -lntp | grep -i ssh` finds nothing under socket activation**, where the listener belongs
   to systemd. Filters on the port now.
17. **The rollback list was incomplete** — it named only `config.txt`/`cmdline.txt` and the masked
   units, omitting cloud-init and the four files the script creates. All now listed.

### Corrections from the fourth review pass (2026-09-09)

This pass tested rather than re-read: the real `config.txt` was pulled off the box and the script's
own Phase 8 block was extracted verbatim and run against it three times in a sandbox, and every
command-line path was exercised.

18. **A masked unit that `disable` refused was never stopped.** `disable --now` refuses outright on
   a unit with no `[Install]` section, and when it refuses the `--now` half does not run either.
   `rpi-zram-writeback.timer` is `generated` **and active**, so it would have been masked while
   still armed for the rest of the uptime — and masking does not stop a running unit. `disable`
   and `stop` are now issued separately, both tolerated, and `stop` only when the unit is active.
   Twelve units on this box turn out to need it.
19. **`./ssh-harden.sh --port` with no value died silently.** `shift 2` with one argument left
   fails, and under `set -e` the script exited 1 with **nothing on stdout and nothing on stderr**.
   A typo produced no diagnosis at all, on the one script that can lock you out. The parser now
   checks for the value before shifting, and range-checks 1–65535 (`--port 99999` was previously
   accepted and left for `sshd -t` to reject).
20. **`ssh-harden.sh` run under `sudo` would have inspected the wrong keys.** Everything in it is
   relative to `$HOME/.ssh`; as root that is `/root/.ssh`, so it would have verified root's
   `authorized_keys`, chmod'd root's `.ssh`, and disabled password authentication on that basis.
   It now refuses to run as root.
21. **Two comments written into `config.txt` itself were wrong for their reader** — one said
   "unlike I2C below" about a line that is above it in the emitted file, and one cited a
   `../ndLouvers/...` repo path that means nothing to somebody reading `/boot/firmware/config.txt`.

**What the sandbox test confirmed** (not defects — evidence the logic holds):

1. Phase 8 is **idempotent**: three consecutive runs against the real `config.txt` produce a
   byte-identical file.
2. The trailing-newline guard **fires correctly** on a `config.txt` stripped of its final newline;
   the marker lands on its own line rather than being glued to `[all]`.
3. `sed` preserves `cmdline.txt`'s absent final newline, as that file requires.
4. A hand static-analysis pass for the usual shell defect classes — unquoted expansions in command
   position, `local x=$(...)` masking exit status, bare `cd`, `$?` testing, unused variables,
   unset-variable use under `set -u` — found nothing. The only unquoted expansions are inside
   `[[ ]]`, where no word splitting occurs.

### Corrections from the fifth review pass (2026-09-09)

Two new methods: reverse-dependency analysis on every unit the script masks, and checking whether
each config file it writes actually wins its ordering contest. Plus real `shellcheck`.

22. **Phase 6 claimed to reduce SD writes while silently overriding a vendor default that exists to
   reduce SD writes.** The image ships `Storage=volatile` in
   `/usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf`; the journal is in RAM today.
   The override is still the right call — see §5 phase 6 — but it was smuggled in under a phase
   title about bounding write pressure, which is close to the opposite. Now stated outright, in
   the script, in the file it writes, and in the rollback list.
23. **The sysctl drop-in sorted before the image's own `98-rpi.conf`, not after.** It happens to
   set no dirty ratios, so `90-` worked by luck. Renamed `99-knurlogger.conf`.
24. **The "Retained on purpose" block named `fake-hwclock`, which is not installed on this image.**
   Worse than a cosmetic error: it credited a non-existent unit with keeping session timestamps
   sane. `systemd-timesyncd` actually does that job — and doing it that way has a **measurement
   consequence** the text now carries (§0 item 8b).

**Confirmed, not defects.**

5. **Masking breaks no dependency chain.** Every unit the script masks is `WantedBy` something and
   `RequiredBy` nothing, so a masked unit is skipped rather than failing its target. Checked for
   all fifteen.
6. **Mask precedence holds.** `rpi-zram-writeback.timer` lives in `/run/systemd/generator`, which
   sits *below* `/etc/systemd/system` in systemd's load path. In `generator.early`, which sits
   above, masking would have done nothing at all.
7. **`shellcheck` 0.11.0 at `-S style`** — the strictest level — reports **one** finding across all
   four scripts, an informational SC1091 about not following `/etc/os-release`, which is on the
   target box rather than here. Nothing actionable.

**Forward constraint, recorded in `../CLAUDE.md`:** masking `NetworkManager-wait-online.service`
means `network-online.target` can no longer be reached. Nothing needs it now, but the KnurLogger
service must never declare `Wants=`/`After=network-online.target`.

### Corrections from the first `--execute` run (2026-09-09)

The far side of `--execute`. Both scripts ran to completion, no phase failed, no unit reported
`FAILED to mask`, and the box came back. Everything below is something no static pass could have
found, which is the point.

25. **`/sys/bus/w1/devices/` is not empty on a bare bus, and this file said it would be.** It
   holds `w1_bus_master1` plus a **phantom slave `00-800000000000`**, and
   `w1_master_slave_count` reads `1` with zero probes attached. Family `00` is not a valid 1-Wire
   family — a DS18B20 is family `28` — so it is a bus with nothing on it. The acceptance criterion
   was wrong in both directions: it called an empty directory the pass, when an empty directory
   would actually mean the overlay had not loaded, and it would have had the next person counting
   a non-device as a probe. Bring-up counts `28-*` and nothing else. Step 6 item 4 corrected.
   **Amended by correction 30: the specific ID and count above are a snapshot of something that
   moves.**
26. **Six of the nine units phase 3 claims to retire are not installed on this image.**
   `ModemManager`, `rsyslog`, `triggerhappy` (service and socket), `bluealsa`, `rpcbind` (service
   and socket), `nfs-client.target`, `cups` and `cups-browsed` all reported "not installed,
   skipping". Only `rpi-eeprom-update`, `udisks2`, `keyboard-setup` and `console-setup` existed.
   §5 item 3's specific claim that `rsyslog` "duplicates journald into `/var/log`, doubling SD
   writes for no reader" was a saving credited to an absent package — the same defect class as
   correction 24's `fake-hwclock`. Section rewritten.
27. **`install-dependencies.sh` changed 14 packages it never named, `rfkill` among them.** The four
   requested packages pulled the whole util-linux family forward from `2.41-5` to
   `2.41.5-0+deb13u1` — `util-linux`, `mount`, `login`, `bsdutils`, `bsdextrautils`, `fdisk`,
   `libfdisk1`, `eject`, `libblkid1`, `libmount1`, `libsmartcols1`, `libuuid1`, `liblastlog2-2`
   and **`rfkill`**. The script had reported `rfkill` "present" and correctly skipped it; apt
   replaced it anyway, one script before the run whose most consequential single action is
   `rfkill unblock bluetooth`. Nothing broke — but "installs four packages" is not what happened,
   and a pre-flight that lists only the four cannot tell you that. `read-edid` also arrived as an
   `i2c-tools` dependency, which is odd on a headless box and harmless.
28. **`System clock synchronized` reads `no` if you check it at once.** Step 6 item 5 treats `yes`
   as the acceptance. At 0 min uptime timesyncd has not reached a server yet; ~1 min later it
   reported `Contacted time server 89.161.47.139:123 (2.debian.pool.ntp.org)` and flipped to
   `yes`. A literal reading of the old text would have failed a passing box. Item 5 corrected.
29. **`polkit.service` is `static` and D-Bus-activated, and is currently inactive.** The "Retained
   on purpose" block says "polkit is a NetworkManager dependency", which reads as though it runs
   at boot. It ran in the before-audit and does not in the after — not because anything retired
   it, but because nothing has asked it for an authorisation this boot. Retained is still the
   right call; the wording overstates what retaining it costs.

### Correction from the assembled sensor zone (2026-09-09)

32. **"An empty I2C scan and an empty 1-Wire slave list are the correct results" is no longer
    true, and neither is the phantom trap that corrections 25 and 30 describe.** The perfboard's
    sensor zone has been assembled (owner, 2026-09-09), minus the pressure-sensor part. Three
    things changed at once, and every acceptance criterion in this file that rests on a silent bus
    is now stale rather than wrong-when-written:
    1. **`i2cdetect -y 1` returns `0x77`.** That is a genuine BME280 — chip-ID register `0xD0`
       reads `0x60` — at the address the build sheet explicitly told us not to use, instead of the
       specified `0x76`. The build sheet's §2 owns the discrepancy and the owner owns the fix
       (pull `SDO` down, or accept `0x77` and amend net list row 9).
    2. **Nothing answers at `0x70`**, so the mux is silent. Expected if the TCA9548A is not
       populated, since it exists only to serve the SDP810s. `gpio=17=op,dh` rules out a held
       reset.
    3. **The 1-Wire phantoms have stopped.** Three scans over 36 s gave `w1_bus_master1` alone,
       `w1_master_slave_count = 0`, no `00-*` entries. Corrections 25 and 30 recorded a churning
       phantom set measured while this zone was **unbuilt**, i.e. with `GPIO4` floating on the
       SoC's internal pull-up; `R11`'s 2.2 kΩ to `+3V3` is in the sensor zone and is now fitted.
       Attributed to the line now being terminated — well-supported by the timing, not proven.
       **The `28-*` family filter remains mandatory** and is now **untestable on this box**, so
       its correctness rests on the parser rather than on an observation.

### Corrections from the full upgrade (2026-09-09)

33. **`/tmp` is cleared on reboot, so nothing staged there survives a step that reboots.** A
    verification script written to `/tmp` before the upgrade was gone when the box came back, which
    is obvious in hindsight and wasted a round trip. **Stage anything that must outlive a reboot in
    `~`**, not `/tmp`.
34. **`sshswitch.service` is enabled and now appears in the boot chain** (152 ms), where it was
    below the reporting cutoff before. It ships with `raspberrypi-sys-mods`, which this upgrade
    bumped, and it enables `ssh.service` when a file named `ssh` exists on the FAT boot partition.
    **Not a fault and not worth retiring** — SSH is wanted on this box and anyone who can write to
    that partition already has the card in their hand, which is a strictly worse position than this
    service represents. Recorded because it is a service that appeared without anyone asking for it.
35. **Boot time regressed 11.317 s → 14.137 s across the upgrade. The regression is REAL and
    PERSISTENT, and the cause was not identified.** A settled reboot measured 14.137 s against
    14.029 s on the first post-upgrade boot, which disposes of the obvious explanation — this is
    not first-boot work. Almost all of it is `NetworkManager.service`, **5.236 s → 7.342 s**, plus
    `rpi-resize-swap-file.service` 562 ms → 1.034 s.
    **Two candidates were tested and both are wrong.** It is not the Wi-Fi firmware, despite the
    upgrade bumping `firmware-brcm80211`: the BCM43455 still loads build `7.45.265` dated
    2023-08-29 and does so about 6 s into boot as before. And it is not the added `Knurfon` hotspot
    profile — NetworkManager auto-activates the bench connection directly, with no scan or attempt
    against the lower-priority profile. Inside NetworkManager there is a **6.7 s window with no log
    output at all**, between loading its device plugins and the first device state change; that is
    where the time goes and what it waits on is unknown.
    **Deliberately not chased further, and that is a judgement rather than an oversight.** Three
    seconds of boot has no operational consequence for a box that logs 40-minute sessions and that
    nobody waits on. `NetworkManager` cannot be removed or delayed — it is the only way back into a
    wheel-well cavity — so the remaining boot time is the part the hardening step already decided
    not to touch, for the same reason. Recorded so the next person does not rediscover it as new.

**Measured outcomes, not defects.**

1. **Boot 19.468 s → 11.105 s**, a 43% cut. Kernel unchanged at ~2.0 s; **userspace 17.463 s →
   9.108 s**; `multi-user.target` 11.305 s → 9.108 s. `NetworkManager.service` at 5.236 s is now
   the entire critical chain past `sysinit`, and the chain no longer runs through cloud-init at
   all. Reducing it further means not waiting on NetworkManager, which costs the way back in.
2. **Enabled timers 9 → 2**, and one of the two (`rpi-zram-writeback.timer`) is masked and shows
   no next elapse. **17 units newly masked**, none of them with a failed status.
3. **The Bluetooth unblock survived the reboot** — the whole question this box turned on.
   `rfkill list bluetooth` reads `Soft blocked: no`, `hciconfig` reads **`UP RUNNING`**, and
   BlueZ reports `Powered: yes` / `PowerState: on`. Better than the acceptance asked for: it
   allowed `Powered: no` on an unblocked controller as the logger's problem to fix, and BlueZ
   powered it unprompted. `systemd-rfkill.service` now restores *unblocked* from the same
   `/var/lib/systemd/rfkill/` state that used to restore the block.
4. **`/dev/i2c-1` exists** and scanned empty across all 112 addresses, as §0 item 9 predicted at
   the time. The 1-Wire bus master registered. Both halves of §0 item 5a took. **Superseded as an
   expectation by correction 32** — the sensor zone has since been assembled.
5. **Journal 8 M volatile → 16 M persistent** in `/var/log/journal`, which is phase 6's deliberate
   trade (correction 22) with a real number against it for the first time.
6. **Dirty ratios took**: `vm.dirty_background_ratio` 10 → 5, `vm.dirty_ratio` 20 → 10. Wi-Fi
   power save is now `wifi.powersave = 2`, where it had been unconfigured and defaulting to 3.
7. **`throttled=0x0` still**, at `volt=0.9060V` and 56.0 °C shortly after boot against 43.8 °C at
   idle earlier. Plan item 5.4 remains a pass at idle and item 5.7's under-load re-read remains
   impossible until there is a logger.
8. **No failed units, and the SSH session survived.** `cmdline.txt` kept `console=tty1` while
   `console=serial0,115200` went, and its absent final newline was preserved.

### Correction from re-probing the box the same day (2026-09-09)

Found while gathering facts for the logger's channel-enrollment design, by reading the 1-Wire bus
a second time instead of trusting the first reading.

30. **Correction 25 described a moving target as a fixed one, and the mistake matters.** The
   phantom is not one device with one ID; it is a **churning set**. Across 35 s with nothing
   wired: `00-800000000000` alone, then `00-dc0000000000` + `00-3c0000000000`, then
   `00-3c0000000000` + `00-bc0000000000` — `w1_master_slave_count` reading `1`, `2`, `2`. These
   are bus-search results off a floating line, and `w1_master_attempts` was already past 250 with
   no probes attached. Rescan interval is 10 s (`w1_master_timeout = 10`).
   **Why the difference is not pedantic.** Correction 25's phrasing supported "expect four probes
   plus one" — a stable, correctable offset. The truth is that the count is **unstable**, so no
   code and no procedure may branch on it, and `28-*` matching is not a tidiness preference but
   the only thing separating a probe from noise. It also sets a hard requirement on the channel
   enrollment the owner asked for: anything that binds "the next device to appear" will bind a
   phantom within about ten seconds of being switched on.
   Corrected in step 6 item 4, in `../CLAUDE.md`, and in the plan set's build sheet §10 step 6,
   which is where the probe counting will actually be done.

### Correction from running `ssh-harden.sh` (2026-09-09)

31. **The rollback list presents `sudo rm /etc/cloud/cloud-init.disabled` as an innocuous undo, and
   after step 7 it is not.** Step 7's fix works by editing `PasswordAuthentication` inside
   **cloud-init's own** `50-cloud-init.conf`, because that file sorts ahead of ours and wins. That
   edit only survives because cloud-init is disabled and will not regenerate the file.
   **So re-enabling cloud-init silently restores `PasswordAuthentication yes`** — it does not just
   undo phase 1, it undoes step 7 as well, and it does so at the next boot with nothing in the
   output to say so. Anyone reverting cloud-init for an unrelated reason must re-run
   `ssh-harden.sh --execute` afterwards, or check `sudo sshd -T | grep passwordauthentication`.
   The two rollback entries are not independent and the list did not say so.

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
   verified from a second terminal plus a forced password-only attempt. **Still not established:**
   nothing here has been exercised under logging load, because there is no logger — no bus has
   carried a transaction, no BLE connection has been made, and `throttled` has only ever been read
   at idle.

### Open items owned by this file

1. **Wi-Fi gating policy** — manual `nmcli`, or logger-owned? Blocked on plan item 5a.
2. **`avahi-daemon` keep or drop** — kept by default. Dropping needs a static DHCP reservation.
3. **Read-only root** — see §9 item 1. Decide before the first event.

**I2C bus speed is not an open item.** Build sheet §6 specifies 100 kHz and owns the decision.
Raising it needs a segment-capacitance measurement or lower pull-ups, and a change to the build
sheet first — not a change here.
