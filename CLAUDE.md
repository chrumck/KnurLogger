The role of this file is to describe common mistakes and confusion points that agents might
encounter as they work in this project. If you ever encounter something here that surprises you,
alert the developer and record it in this file so the next agent does not hit it.

# CLAUDE.md — KnurLogger

**`CLAUDE.history.md` is the companion audit trail** — faults that were found and fixed, the
diagnostics that found them, decisions that were reversed, and requirements that were built and
then deliberately removed. Nothing in it is a live instruction. Read it when a statement here
surprises you, when you are about to reinstate something that looks missing, or when a figure you
remember from git history or an older transcript does not appear here. **The guards against
reinstating retired work are here; the reasoning behind them is there.** When a conclusion
changes, put the new one here and the superseded one there — do not leave "this used to say"
narrative in this file.

## Where authority lives

- **This repository owns software and host configuration. It owns no measurement decision.**
  `../ndLouvers/CFD-Learning-Plan.md` Step 0b is the authority on channels, acceptance criteria,
  calibration and commissioning. `../ndLouvers/step0b-rig/logger-perfboard-wiring.md` is the
  authority on wiring, I2C addresses, mux channel numbering and bring-up order — its §3a net list
  specifically, against which §3, §4 and §6 are views.
- **Cross-repo, not cross-directory.** `ndLouvers` is a separate git repository that happens to
  sit alongside this one. Relative links between them work on disk and break on a git host. Do not
  "fix" them by copying content across; a duplicated requirement is a requirement that will drift.
  This repository has a public upstream at `github.com/chrumck/KnurLogger`, so every
  `../ndLouvers/...` link 404s there. That is accepted.
- **This repository is PUBLIC. Weigh that before writing host specifics into it.** It already
  carries the box's LAN IP, its username and its Bluetooth MAC. Nothing here is reachable from the
  internet (RFC1918 address, and the BT MAC is broadcast to anyone in range anyway), and no
  credential, key or Wi-Fi PSK is in the repo. **Git history is not retractable**, so the test for
  anything new is "would I mind this being permanent and public", not "is it useful now".
  **Measured-state notes are the ones that age badly**: accurate and useful when written, then a
  public status page for a host that may not have been hardened yet. History §1.6 is the worked
  example — it took a day to close, and the next might not.

## Naming

- **Channel names are positional and mean nothing.** Pressure channels are `P0`–`P5`, fixed by mux
  position. Thermal channels are `temp0`–`temp3`, fixed by DS18B20 ROM ID at enrollment. The
  mapping to measurement roles is a per-session record, logged at boot, and **a channel is never
  renamed after a role**.
  - **Pressure (`U`/`X`/`C`) is still deliberately undecided.** Do not invent one.
  - **Thermal is decided but not yet applied.** Installing the probes on the car decided it (owner,
    2026-09-09): enrolled in installed order, lowest first, it is **temp0 = `T_ambient`,
    temp1 = `T_core_in`, temp2 = `T_core_out`, temp3 = `T_aft`**. **No channel is bound yet** —
    the enrollment mode exists and works, but the probes are on the car, so binding needs the trip.
    The plan owns the positions and the reasoning; this is a pointer, not a second copy.
- **`P` names a logger channel only.** The two pitot probes are `T1`/`T2`, never `P1`/`P2`.

## Hardware facts that surprise people

- **The sensor zone is ASSEMBLED, minus the pressure-sensor part** (owner, 2026-09-09) — the five
  SDP810s are still being delivered. **An empty I2C scan is therefore no longer the correct
  result**, and neither is zero `28-*` devices (history §2.6 for what the acceptance criteria used
  to say). Three facts from the assembled board matter before writing any bus code:
  1. **The BME280 is at `0x77`, and that is the specified address** (owner decision, 2026-09-09,
     amending the build sheet rather than the board). It is a genuine BME280 — chip-ID register
     `0xD0` reads `0x60`. So **`0x77` is the address to code against** and it is no longer free for
     anything else; **never strap a mux upward on this board.** No collision results, because the
     mux is strapped to `0x70` alone. History §1.7 has the discrepancy this resolved.
  2. **Whenever a bus device is silent, check the Pi side with `pinctrl get <n>` before suspecting
     the device.** It is one command and it splits the search space in half. This is what localised
     the mux's `~RESET` miswiring (history §1.1, since resoldered and answering at `0x70`).
     **Do not try to fix a reset problem in software** — the GPIO was already high, so no
     `pinctrl`/libgpiod write could have helped, and one would only have masked the diagnosis.
  3. **The mux is a PCA9548A, not a TCA9548A** — the board is marked PCA9548A (owner,
     2026-09-09). Functionally equivalent for everything here: same pinout, same `0x70`–`0x77`
     range, same single-control-byte channel register, same active-LOW `~RESET`. Recorded so that
     searching the board for a "TCA9548A" does not suggest the wrong part was fitted.
- **Testing the thermal channels requires a trip to the car, and the car has no network**
  (owner, 2026-09-09). The four DS18B20s are installed on the car; the car is in an underground
  garage with **no cell coverage and no internet**. So the logger is carried there, run, and
  brought back with artefacts to inspect. Five consequences:
  1. **There IS a way in: the owner's phone hotspot.** The owner can raise a local Wi-Fi network
     from the phone and SSH into the box — no internet, but a shell. So enrollment does not have
     to be blind. **The precondition is that the box already knows that SSID and its PSK**, since
     there is no other way to configure it there; a NetworkManager profile for the hotspot must
     exist *before* the trip, and no PSK goes in this public repository.
  2. **It must nonetheless survive unattended from power-on**, because the box is fed from the
     car's supply and *will* be power-cycled in the garage. A hand-started process does not
     survive that, which is why the systemd unit exists.
  3. **BLE is the richest feedback channel at the car**, and the only one that shows the data as
     RaceChrono will see it. A phone watching `temp0`–`temp3` move is what makes the "warm one
     probe by hand and watch which channel moves" identification check performable in situ.
  4. **Do not run the BLE link qualification with Wi-Fi up.** Wi-Fi and BT share one radio and one
     antenna on the BCM43455, and plan item 1b names background Wi-Fi scanning as a known source
     of BLE jitter. The hotspot is a debugging convenience, so **gate Wi-Fi off for any
     measurement of link quality** and bring it back up afterwards to collect artefacts.
     `rfkill block wifi`, never `rfkill block all`.
  5. **The pressure sensors have no such problem** — they sit on the perfboard and bench-test
     directly, once they arrive.
- **All five SDP810s share one fixed I2C address (`0x25`) and cannot be strapped apart.** The mux
  is therefore mandatory, one sensor per channel. The mux does **not** pass pull-ups downstream, so
  every populated channel has its own pair.
- **The BME280's pressure channel is enclosure pressure, never a static reference.** The cavity is
  aerodynamically live; at Cp −1 the offset is ~464 Pa against 45–90 Pa measurands. It is
  tolerable as a density term and disqualifying as a reference. Name the logged field accordingly.
- **A DS18B20 has no positional anchor.** Its 64-bit ROM ID *is* the channel definition, recorded
  once at enrollment. As of 2026-09-10 **no ROM ID has been recorded, so no `temp` channel is yet
  defined** — `oneWireProbes.cxx` and its `channels.ini` store exist, and the store is empty on the
  box because the probes are on the car. Build sheet §5a's table is correspondingly still blank.
- **The `28-*` family filter is mandatory, and it can no longer be exercised on this box.** The
  bare bus used to invent churning phantom `00-*` devices; with the sensor zone assembled and
  `R11` terminating `GPIO4` the scans come back clean (history §1.4). The filter is correct
  regardless of cause — a marginal 4 × 5 m star can produce garbage of its own — but its
  correctness now rests on the parser rather than on an observation. Family code `00` is not a
  valid 1-Wire family; a DS18B20 is family **`28`**.
  1. **Match `28-*` and nothing else, everywhere** — enumeration, binding, and reads.
     `w1_master_slave_count` is not "off by one", it is **unstable**, and no code may branch on
     it.
  2. **A `00-*` ROM ID must never be persisted** as a `temp` channel definition. Anything that
     auto-binds "the next device that appears" will bind noise within about ten seconds.
  3. **An *empty* devices directory is the real failure signal**, because a working bus always
     registers its master.
  4. **Bus rescan is every 10 s** (`w1_master_timeout = 10`), which is the hot-plug detection
     latency for anything that watches for a probe being connected. It cannot be shortened without
     root — the master attributes are root-owned and the logger runs as `chrum`.
  5. **`therm_bulk_read` does not exist on this box, and its absence is not a fault** (measured
     2026-09-10). It is the documented way to convert every probe at once, and it is the only way
     to sample four probes at 1 Hz — without it each `w1_slave` read pays its own ~750 ms
     conversion and a four-probe cycle takes ~3 s, so plan thermal item 2's "start around 1 Hz"
     silently becomes 0.33 Hz. `w1_therm` registers it as a **master** attribute only once a slave
     of its family attaches, so with no probes on the bench there is nothing to test and nothing
     to fix. `oneWireProbes.cxx` triggers it when it exists and falls through when it does not;
     **the bulk path has therefore never run.** Its worst case is the per-probe path, which is why
     it was shipped unexercised, but treat the cycle time in the session record (`cycleMs`,
     `conversionMs`, `bulkConversion`) as the first thing to read after the first real run.
  6. **The whole 1-Wire path IS testable without probes, and this is how** (2026-09-10).
     `unshare -Urm --map-root-user` gives an unprivileged user namespace with a private mount
     namespace, so a fake tree can be bind-mounted over `/sys/bus/w1/devices` — **no root, no sudo,
     no risk to the real box, and nothing to undo** since the namespace dies with the shell.
     Populate it with `w1_bus_master1/`, `28-…/w1_slave` files in the kernel's two-line
     `crc=xx YES` / `t=<millidegrees>` format, and a `00-…` entry to prove the family filter drops
     it. Enrollment order, the ambiguous-step refusal, the store's family and duplicate guards,
     bound-but-absent, CRC failure, the 85.00 °C default, out-of-range rejection and the
     application of a hand-entered offset were all verified this way. **Reach for this before
     concluding a sysfs-driven path is untestable.** What it does not establish: real bus timing,
     real conversion time, or `therm_bulk_read`.

## When BLE will not advertise, start here

The platform has already been the culprit once and the logger looked guilty (history §1.2), so
**do this before reading `raceChronoBle.cxx`:**

1. **`bluetoothctl advertise on`.** If that fails too, the fault is not in this repository and no
   amount of reading `raceChronoBle.cxx` will find it. This single command separates "our code"
   from "the platform" and it should always be step one.
2. **`journalctl -u bluetooth`** for the `bluetoothd`-side reason, which is more specific than the
   D-Bus error the client sees.
3. **`btmgmt add-adv -c -u 1ff8 1`** (root). This uses the *legacy* MGMT path, so success here
   proves the controller and kernel can advertise and narrows the fault to `bluetoothd`.
4. **`btmon` while triggering an attempt** (root) — the only thing that shows an actual malformed
   command. `btmon -w file` then `btmon -r file`.

- **`SupportedInstances` and `ActiveInstances` on `org.bluez.LEAdvertisingManager1`** are the quick
  check that an advertisement actually registered. `ActiveInstances: 1` while the logger runs is
  the acceptance; `0` means it silently did not.
- **Known-good versions: kernel `6.18.39`, `bluez 5.82-1.1+rpt2`, `firmware-brcm80211
  1:20260519`.** A regression to `bluez 5.82-1.1+rpt1` on kernel `6.18.34` cannot advertise at all;
  read history §1.2 before debugging anything else.

## The box

- **SSH alias `KnurLogger`** — `192.168.118.52`, user `chrum`, key-only.
- **Raspberry Pi OS Lite 64-bit, Trixie**, kernel `6.18.39`, Pi 4B Rev 1.5, 4 GB.
  `/boot/firmware/config.txt` is the boot config path. NetworkManager is the network stack.
- **There is no `hciuart.service` on this image.** The BCM43455 is attached by udev and
  `bluetooth.service` is the only unit involved. Recipes that name `hciuart` predate this.
- **`wpa_supplicant.service` is enabled and running, and NetworkManager drives it over D-Bus.**
  Disabling it because "NetworkManager spawns its own" loses Wi-Fi, which is the only way onto a
  box in a wheel-well cavity. An early `harden-headless.sh` draft did exactly that (history §1.8).
- **Swap is zram (`/dev/zram0`), not a file.** It is RAM-backed and costs no SD wear, so there is
  nothing to gain by turning it off. `dphys-swapfile` does not exist here. The one thing worth
  retiring is `rpi-zram-writeback.timer`, whose job is to push zram pages onto the card.
- **I2C needs a module that nothing loads; 1-Wire does not.** `dtparam=i2c_arm=on` registers the
  adapter but does **not** create `/dev/i2c-1` — the `i2c-dev` module does, and `/etc/modules` is
  empty with no modalias path to pull it in. `raspi-config`'s `do_i2c` does both steps and any
  script replacing it must too; `harden-headless.sh` writes
  `/etc/modules-load.d/knurlogger.conf`. 1-Wire has no equivalent gap because `w1_therm` carries
  the alias `w1-family-0x28`. **A missing `/dev/i2c-1` after a reboot means one of the two halves
  did not take** — it is never "the sensors are not built yet", which only ever explained an
  *empty scan*. Both halves took on 2026-09-09 and `/dev/i2c-1` exists.
  **`/dev/i2c-20` and `/dev/i2c-21` exist too and are not yours** — they are the VC4 display DDC
  buses, which `i2c-dev` exposes against adapters the KMS driver registers. The perfboard is on
  **bus 1** and nothing else. History §1.9 is how that distinction diagnosed a missing bus 1.
- **`w1-gpio`'s `pullup` parameter is ignored** on this firmware — the overlays README says so
  outright. The overlay that drives an external strong pullup is a different one,
  `w1-gpio-pullup`, and this build must not use it: `R11` is a plain 2.2 kΩ resistor to 3V3.
- **The box is fed from CONSTANT 12 V, not the accessory circuit** (owner, as built,
  2026-09-10 — history §2.2 for the accessory-feed assumption this replaced). Four things follow.
  1. **The logger runs the whole day; the fuse is the off switch.** Fitted in the morning, pulled
     at the end. So it is powered through engine-off periods and one session file spans the day.
  2. **`SystemSetup/KnurLogger.service` is REQUIRED, not convenient.** There is no ignition
     event to hand-start the logger around, and the paddock has no network but a phone hotspot, so
     without the unit the owner must SSH in every morning to start it by hand.
  3. **The box is powered during cranking**, which it never was before. The HW-384's 6 V floor is
     well below a normal dip so this should be benign, but build sheet §10 step 3's crank watch was
     bypassed rather than passed. A crank brownout shows up as a latched undervoltage bit, or as
     the session file splitting with a fresh `session` record if the Pi rebooted.
  4. **Battery drain is a new failure mode.** Estimated ~275 mA at 12 V — ~3.3 Ah over a 12 h day
     against the ND's ~45 Ah, which is comfortable, but **~46 Ah over a week with the fuse left
     in, i.e. a flat battery.** Estimated, not measured; plan item 5.3 still owes the real figure.
  **The ~1 s `fsync` requirement is unchanged — only its trigger moved.** A hard cut is the fuse
  being pulled, or a cranking dip. Repeated hard cuts are the durability risk worth knowing: one
  costs at most the last second, but doing it daily for a season is the classic route to a corrupt
  SD card, so `sudo poweroff` before pulling the fuse is free insurance.
- **Two logger instances run happily side by side and BOTH advertise — nothing refuses, nothing
  warns** (measured 2026-09-10: `SupportedInstances` is 5, `ActiveInstances` went 1 → 2 with a
  log-mode and an `--enroll` instance up together). **So stop the service before enrolling.** No
  single-instance guard exists — the owner declined one as not worth the code for a one-shot job —
  which means this is a discipline matter and the reason is worth knowing:
  1. **Two advertisements share the name and the service UUID**, so RaceChrono connects to one and
     you cannot tell which. If it picks the log-mode instance, `temp0`–`temp3` stay at −327.68 °C
     however enrollment goes, and the warm-one-probe identification check silently cannot work.
  2. **A log-mode instance never sees new bindings.** It reads `channels.ini` once at worker start
     and never re-reads it; only `--enroll` writes it. So it must be restarted after enrolling
     regardless.
  3. **Both poll the bus**, each read triggering its own conversion, roughly doubling cycle time.
  Nothing corrupts: session files carry a mode tag so they never collide, and the store write is
  temp-file plus rename plus directory fsync.
- **The tar-over-ssh build loop OVERWRITES the deployed `build/KnurLogger.ini`, and that file
  holds the thermal offsets.** `build/KnurLogger.ini` is tracked in git and there is no separate
  untracked deployed copy, so an offset typed in on the box is destroyed by the next sync from the
  workstation — silently, with the logger carrying on using the repo's values. Edit offsets in the
  repo and sync, or add `--exclude=build/KnurLogger.ini` to the `tar` when editing on the box. The
  README's repository section carries the loop and both workarounds.
- **`sudo` requires a password.** Pre-flight runs pipe fine over `ssh host 'bash -s'`; anything
  that changes state must run from a login shell (`ssh -t`), and every mutating script checks this
  up front rather than failing halfway.
- **`/usr/sbin` is not on `PATH`** for a non-interactive SSH session or a non-root Debian login
  shell. `sysctl`, `rfkill`, `swapon` and `i2cdetect` all live there, so `command -v rfkill`
  answers "missing" on a box where rfkill is installed. Every script in `SystemSetup/` prepends
  the sbin directories; do the same in anything new, and distrust any "tool missing" result that
  has not accounted for this.
- **`i2c-tools`, `cmake`, `git` and `libglib2.0-dev` are installed** as of 2026-09-09, by
  `SystemSetup/install-dependencies.sh --execute`. `rfkill`, `build-essential`, `nmcli` and
  `bluetoothctl` were already present. **A pre-flight package list does not bound what apt will
  change** — history §1.10.
- **The Bluetooth soft block is CLEARED and survived a reboot** — `Soft blocked: no`, `hciconfig`
  `UP RUNNING`, BlueZ `Powered: yes` / `PowerState: on`. It *shipped* blocked, and history §1.3 is
  why that state cannot be undone from `bluetoothctl`.
- **Some channels the plan needs will never appear on box 2's SD card.** CAN ambient
  (`0x420` byte 7) and, if it is on the bus, **cooling-fan state** arrive through box 1's CAN
  broadcast into RaceChrono — not through this logger (`../ndLouvers/` §7 open item 35). So the
  record for a session is **split across two devices, and RaceChrono is what reassembles it** —
  which is the whole reason BLE is the primary data path. The consequence: **a box-2 channel that
  never reaches the phone cannot be aligned to the fan state that explains it**, and the fan can
  change state mid-run with no driver input and no speed change. So a BLE gap is not a cosmetic
  loss — it is the loss of the only link between box 2's pressures and the fan behaviour that
  conditions them. (It is *not* reassembled by a session-start time offset; history §2.5.)
- **`fake-hwclock` is not installed, and the clock in the car will be wrong.** A Pi 4B has no RTC.
  `systemd-timesyncd` saves the time to `/var/lib/systemd/timesync/clock` and restores it at boot,
  so a session file is never stamped 1970 — but with no NTP in the car the clock simply resumes
  from the last bench sync and is **wrong by however long ago that was**, while looking perfectly
  plausible. **The car has no network at all**, so there is no NTP to reach even in principle.
  **Do not build a GPS-time fetch to fix this** (history §2.5). What the logger does instead is
  cheap and sufficient: **every local record carries elapsed-since-boot alongside the wall
  clock**, so intra-session timing is exact regardless of what the absolute epoch says.
- **`network-online.target` can no longer be reached** once `harden-headless.sh` masks
  `NetworkManager-wait-online.service`. Nothing needs it today — only cloud-init did, and that is
  disabled too. But the KnurLogger service must **not** use `Wants=`/`After=network-online.target`;
  it would wait on a target that never comes up. `After=multi-user.target` and the box's own
  readiness checks are the right shape. Verified: every unit this script masks is `WantedBy`
  something and `RequiredBy` nothing, so masking breaks no dependency chain.

## Never disable

- **Bluetooth.** BLE is the RaceChrono link and the reason this box exists. `iSitePiLogger`'s
  `setupNotes.txt` sets `dtoverlay=disable-bt` and is otherwise this project's model — that one
  line is not to be copied.
- **Wi-Fi, permanently.** `dtoverlay=disable-wifi` needs an SD card and a text editor to undo.
  Gate it per session with `rfkill block wifi` or `nmcli radio wifi off` instead — **never
  `rfkill block all`**, which takes BLE down with it and persists across reboots, and cannot then
  be cleared from `bluetoothctl` (history §1.3). Whether Wi-Fi needs gating at all is plan item
  5a's installed link check to answer, and that check has not been run.

## Scripts

- **Every script in `SystemSetup/` defaults to pre-flight and needs `--execute`.** This is the
  `iSitePiLogger` `prepare-image.sh` idiom and it is deliberate. Run with no arguments, read the
  `+` lines, then re-run.
- **`harden-headless.sh` masks rather than disables.** apt's timers re-enable themselves on
  package upgrade. `sudo systemctl unmask <unit>` is the rollback.
- **`disable --now` is not a way to stop a unit that has no `[Install]` section.** systemd refuses
  the whole command, so the `--now` half never runs, and masking does not stop a running unit
  either. `retire_unit` therefore issues `disable`, `stop` and `mask` as three separate steps,
  tolerating the first two. Twelve units on this box are active when it runs.
- **`ssh-harden.sh` must not be run under `sudo`** — it reads `$HOME/.ssh/authorized_keys`, and as
  root that is the wrong file entirely. It refuses, but do not work around it.

## Architecture

Follow `iSitePiLogger`, which is the structural model:

- **Single translation unit.** All `.cxx` files are `#include`-d into `main.cxx`. Do not add them
  to `CMakeLists.txt` as independent targets.
- **`initialiseBlePackets()` is called exactly once, from `main`, before any worker starts.**
  Initialise shared packet state in `main`, never in a worker — history §1.5 is the latent bug that
  made this a rule.
- **Procedural workers, no OOP.** Plain `gpointer fn(gpointer)` passed to `g_thread_new()`.
- **`CLOCK_TAI` throughout**, to avoid leap-second discontinuities in sample timestamps and file
  names.
- **The `.ini` sits beside the binary** and its path is resolved from `/proc/self/exe`, not the
  working directory.

Four requirements came from the plan rather than from iSitePiLogger. A fifth — an automatic
session-start thermal sample — was implemented and then **retired whole**; do not rebuild it, and
read history §3.1 before concluding it is missing by accident.

- **BLE is the primary data path; the SD card is the durable raw and diagnostic record**
  (owner decision, 2026-09-09, item 5b — this **reverses** the earlier "SD primary, BLE
  secondary", so do not reinstate that from memory or from git history; history §2.1). The analysis
  record is RaceChrono's consolidated log, because RaceChrono is what collects box 1's CAN
  broadcast, box 2's channels and the phone's GPS and stamps them into one frame set on one
  timebase. Box 2's channels have to reach the phone to be useful.
  **The local file is still mandatory, on three narrower grounds** — and each one is a thing the
  BLE path physically cannot do:
  1. **Link-level loss cannot be signalled over BLE.** *Channel*-level invalidity can: send
     `-32768` (`INT16_MIN`) for any channel with no trustworthy reading and RaceChrono decodes an
     unmistakable −327.68 °C, which is what the ESP32 rig already does. But a dropped connection,
     a phone that stopped recording, or a dead logger leaves nobody to send a sentinel, and
     RaceChrono presents the last value it received indefinitely. **A gap in the local stream is
     the only evidence that a sample was missing rather than held.**
  2. **Item 4's diagnostics are not channel-shaped** — raw counts, the retained scale factor,
     sensor temperature, product/revision/serial, per-sample validity flags.
  3. **Supply telemetry is SD-only by nature**, because the event worth catching is a brownout at
     a brownout — with the constant feed as built, a cranking dip or the fuse being pulled.
- **Append-only session file, `fsync` on a fixed ~1 s cadence** — not per sample, not only at
  close. The supply vanishes without warning — the fuse pulled at the end of the day, or a
  cranking dip — so the last durable write bounds the loss. Flushing per sample at 10 Hz buys a
  shorter window at the price of write amplification without changing the failure mode.
- **The logger binds `temp0`–`temp3` itself, by discovery order, and persists the binding**
  (owner decision, 2026-09-09). The owner plugs the four DS18B20s in one at a time, lowest channel
  first; the logger notices each new `28-*` ROM ID and writes the binding to a store that survives
  restarts. This replaces reading ROM IDs off a bench rig and typing them into a config by hand.
  Seven things make it correct rather than merely convenient, and skipping any of them produces
  silently mislabelled temperature data — which is worse than no data, because ΔT_preheat rests on
  the *differences* between these four probes:
  1. **`28-*` only.** The bare bus invents churning `00-*` phantoms (see the family-filter trap
     above). Binding "the next new device" without the family filter binds noise.
  2. **Binding happens only in an explicit enrollment mode, never during a logging run.** A
     dropout and reconnect mid-session, or a probe replaced after a failure, must not silently
     re-bind a channel. Outside enrollment an unknown ROM ID is **invalid data and a logged
     event**, not a new channel.
  3. **One probe per enrollment step.** If two unbound `28-*` IDs appear in the same 10 s scan the
     arrival order between them is unknowable — sysfs order is not arrival order — so the logger
     must refuse the ambiguous step and say so rather than guess.
  4. **The store carries provenance: the ROM ID, the channel and the bind timestamp.** It carries
     no offset — that key was retired (history §3.2).
  5. **Enrollment must report which ROM ID it just bound**, so the binding can be checked against
     the lead being plugged in. **The probes are already installed on the car** (owner,
     2026-09-09) and the owner can identify each lead at the logger end, so enrollment binds
     channel → location directly and the older "mark the probe body" step is moot. Provide an
     identification fallback anyway: with all four bound, **warming one probe by hand must be
     visible as one channel moving**, which confirms the map in situ and doubles as a liveness
     test.
  6. **A BOUND channel whose ROM ID goes absent must be logged as present-but-invalid, never
     omitted** (owner, 2026-09-10). Iterating the four channels rather than the devices present on
     the bus is what produces it. If the record simply loses the channel, a mid-session dropout
     becomes indistinguishable from the logger not having run — which destroys the one property the
     SD file exists for, namely that a gap in the local stream is the only evidence a sample was
     missing rather than held. The sentinel goes out on `0x602` for it, and `reason` in the session
     record separates `unbound`, `absent`, `crc`, `powerOnDefault`, `outOfRange` and `readFailed`.
     **`absent` must not increment the read-error counter.** A dropped lead and a marginal bus send
     you to different parts of the car, and inflating `0x603` byte 2–3 with absences would bury the
     bus-quality signal it exists to carry.
  7. **Enrollment must keep running after the fourth bind** (owner, 2026-09-10). Requirement 5's
     fallback check *is* "warm one probe and watch which channel moves", and that needs a logger
     still sampling and still notifying. An enroller that exits on the fourth bind silently
     removes the only in-situ verification of the map. It reports a fifth ROM ID once and refuses
     it; `--enroll --reset` is the way to start over.
- **Per-channel offsets are hand-entered in `KnurLogger.ini` and ARE APPLIED to what goes to
  RaceChrono** (owner decisions, 2026-09-10). Two earlier positions were reversed to get here —
  "never auto-apply an offset" and ROM-ID keying in the store — so do not restore either from git
  history; history §2.3 and §2.4 carry both, and §2.3 in particular states the one condition the
  reversal rests on: **the session record carries the raw reading (`centiC`), the offset in force
  (`offsetC`) and the value sent (`sentCentiC`) side by side, and anything that drops the raw value
  re-opens the original objection.**
  **The consequence of slot-keying, stated once because it is real:** an offset is a property of
  one particular DS18B20, so if the probes are re-enrolled in a different order, or one is swapped,
  the offsets stay with the slots and no longer describe the parts in them. **Re-check them after
  any re-enrollment.** What it buys is not nothing: `build/KnurLogger.ini` is **tracked in git**,
  so the calibration is version-controlled and survives a wiped data directory, which
  `channels.ini` — data-directory field state, never in git — would not.
  Four further properties, all verified 2026-09-10:
  1. **All four keys must be present.** `temp0OffsetC`..`temp3OffsetC` in `[thermal]`; a missing
     one is a startup failure rather than a silent zero, because an offset that quietly stopped
     being applied would be invisible in the data it corrupts. A non-numeric value and one past
     the bound also refuse to start.
  2. **Two bounds, deliberately different in kind.** Past ±5 °C it warns about a likely
     decimal-point slip and applies the value anyway; past ±50 °C it refuses to start. A logger
     that will not start in the car over a plausible-but-large typo loses the whole session, which
     is worse than a wrong offset that is recorded and reversible.
  3. **Only relative offsets mean anything.** There is no reference thermometer on the car, and
     ΔT_preheat depends on the four probes' differences rather than their absolute accuracy, so
     what belongs there is each probe's deviation from the four-probe mean at equilibrium.
  4. **The result is clamped clear of `INT16_MIN`.** A corrected reading that landed exactly on the
     invalid sentinel would be indistinguishable from "no reading at all".
- **Supply health is logged telemetry, read after a run** (owner decision, 2026-09-09), standing
  in for a bench instrument on commissioning item 5.7 — **not** for build sheet §10 step 2's
  meter. What to record, and the traps:
  1. **`vcgencmd get_throttled`** is the useful one, because bits 16–19 **latch** "has occurred
     since boot". That is what makes a 1 Hz sampler unable to miss a transient. Log the live bits
     *and* the sticky bits, and log the **first transition with a timestamp** — "something
     happened during a 40-minute session" is far weaker evidence than "it happened 3 s after the
     fan engaged". Cost measured at ~3 ms per call, so 1 Hz is free.
  2. **`/sys/class/hwmon/<n>/in0_lcrit_alarm` on the `rpi_volt` device** is the same undervoltage
     comparator without a subprocess, but it is **live only, with no sticky history**. Resolve it
     by reading each hwmon's `name` file — **the hwmon index is not stable across boots**, and it
     was `hwmon1` on one boot only.
  3. **`vcgencmd measure_volts core` is NOT the supply rail.** It reports the regulated SoC core
     voltage (~0.906 V) and says nothing about the 5 V input. Logging it beside the throttle
     flags invites exactly that misreading; if it is logged, name the field so it cannot be
     mistaken for a rail measurement.
  4. **This telemetry must reach the fsync'd session file, not only the journal.** The event most
     worth having is a brownout, which on the constant feed as built means a cranking dip or the
     fuse being pulled — the moment the box loses power.

Preserve CRC failures, clipping, disconnects and stale samples as **invalid data**, never as
carried-forward values presented as new.
