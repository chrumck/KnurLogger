The role of this file is to describe common mistakes and confusion points that agents might
encounter as they work in this project. If you ever encounter something here that surprises you,
alert the developer and record it in this file so the next agent does not hit it.

# CLAUDE.md — KnurLogger

## Where authority lives

- **This repository owns software and host configuration. It owns no measurement decision.**
  `../ndLouvers/CFD-Learning-Plan.md` Step 0b is the authority on channels, acceptance criteria,
  calibration and commissioning. `../ndLouvers/step0b-rig/logger-perfboard-wiring.md` is the
  authority on wiring, I2C addresses, mux channel numbering and bring-up order — its §3a net list
  specifically, against which §3, §4 and §6 are views.
- **Cross-repo, not cross-directory.** `ndLouvers` is a separate git repository that happens to
  sit alongside this one. Relative links between them work on disk and break on a git host. Do not
  "fix" them by copying content across; a duplicated requirement is a requirement that will drift.

## Naming

- **Channel names are positional and mean nothing.** Pressure channels are `P0`–`P5`, fixed by mux
  position. Thermal channels are `temp0`–`temp3`, fixed by DS18B20 ROM ID at build time. The
  mapping to measurement roles is a per-session record, logged at boot, and **a channel is never
  renamed after a role**.
  - **Pressure (`U`/`X`/`C`) is still deliberately undecided.** Do not invent one.
  - **Thermal is decided but not yet applied.** Installing the probes on the car decided it (owner,
    2026-09-09): enrolled in installed order, lowest first, it is **temp0 = `T_ambient`,
    temp1 = `T_core_in`, temp2 = `T_core_out`, temp3 = `T_aft`**. No channel is bound yet, because
    enrollment needs a logger. The plan owns the positions and the reasoning; this is a pointer,
    not a second copy.
- **`P` names a logger channel only.** The two pitot probes are `T1`/`T2`, never `P1`/`P2`.

## Hardware facts that surprise people

- **All five SDP810s share one fixed I2C address (`0x25`) and cannot be strapped apart.** The
  TCA9548A mux is therefore mandatory, one sensor per channel. The mux does **not** pass pull-ups
  downstream, so every populated channel has its own pair.
- **The BME280's pressure channel is enclosure pressure, never a static reference.** The cavity is
  aerodynamically live; at Cp −1 the offset is ~464 Pa against 45–90 Pa measurands. It is
  tolerable as a density term and disqualifying as a reference. Name the logged field accordingly.
- **A DS18B20 has no positional anchor.** Its 64-bit ROM ID *is* the channel definition, recorded
  once at build. As of 2026-09-09 no ROM ID has been recorded, so no `temp` channel is yet defined.
- **The bare 1-Wire bus invents phantom devices, they are not probes, and THE SET CHURNS.** With
  the overlay loaded and nothing wired, `/sys/bus/w1/devices/` holds `w1_bus_master1` plus a
  varying number of `00-*` entries whose IDs change from scan to scan. Measured 2026-09-09 across
  35 s: first `00-800000000000` alone, then `00-dc0000000000` + `00-3c0000000000`, then
  `00-3c0000000000` + `00-bc0000000000`. `w1_master_slave_count` read `1`, then `2`, then `2`.
  These are bus-search results read off a floating line, and `w1_master_attempts` was already
  past 250 with nothing attached. Family code `00` is not a valid 1-Wire family; a DS18B20 is
  family **`28`**.
  1. **Match `28-*` and nothing else, everywhere** — enumeration, binding, and reads.
     `w1_master_slave_count` is not "off by one", it is **unstable**, and no code may branch on
     it.
  2. **A `00-*` ROM ID must never be persisted** as a `temp` channel definition. Anything that
     auto-binds "the next device that appears" will bind noise within about ten seconds.
  3. **An *empty* devices directory is the real failure signal**, because a working bus always
     registers its master.
  4. **Bus rescan is every 10 s** (`w1_master_timeout = 10`), which is the hot-plug detection
     latency for anything that watches for a probe being connected.

## The box

- **SSH alias `KnurLogger`** — `192.168.118.52`, user `chrum`, key-only.
- **Raspberry Pi OS Lite 64-bit, Trixie**, kernel `6.18.34+rpt-rpi-v8`, Pi 4B Rev 1.5, 4 GB.
  `/boot/firmware/config.txt` is the boot config path. NetworkManager is the network stack.
- **There is no `hciuart.service` on this image.** The BCM43455 is attached by udev and
  `bluetooth.service` is the only unit involved. Recipes that name `hciuart` predate this.
- **`wpa_supplicant.service` is enabled and running, and NetworkManager drives it over D-Bus.**
  Disabling it because "NetworkManager spawns its own" loses Wi-Fi, which is the only way onto a
  box in a wheel-well cavity. An earlier draft of `harden-headless.sh` did exactly that; the first
  real audit caught it.
- **Swap is zram (`/dev/zram0`), not a file.** It is RAM-backed and costs no SD wear, so there is
  nothing to gain by turning it off. `dphys-swapfile` does not exist here. The one thing worth
  retiring is `rpi-zram-writeback.timer`, whose job is to push zram pages onto the card.
- **I2C needs a module that nothing loads; 1-Wire does not.** `dtparam=i2c_arm=on` registers the
  adapter but does **not** create `/dev/i2c-1` — the `i2c-dev` module does, and `/etc/modules` is
  empty with no modalias path to pull it in. `raspi-config`'s `do_i2c` does both steps and any
  script replacing it must too; `harden-headless.sh` writes
  `/etc/modules-load.d/knurlogger.conf`. 1-Wire has no equivalent gap because `w1_therm` carries
  the alias `w1-family-0x28`. A missing `/dev/i2c-1` after a reboot means one of the two halves
  did not take — it is never "the sensors are not built yet", which only explains an *empty scan*.
  **Both halves took on 2026-09-09** and `/dev/i2c-1` exists, scanning empty across all 112
  addresses as expected. **`/dev/i2c-20` and `/dev/i2c-21` exist too and are not yours** — they
  are the VC4 display DDC buses, which `i2c-dev` exposes against adapters the KMS driver
  registers. The perfboard is on **bus 1** and nothing else. Their appearance is how the two
  halves were told apart mid-run: loading `i2c-dev` before the reboot produced 20 and 21 but no 1,
  because the `i2c_arm` adapter does not exist until the firmware re-reads `config.txt`.
- **`w1-gpio`'s `pullup` parameter is ignored** on this firmware — the overlays README says so
  outright. The overlay that drives an external strong pullup is a different one,
  `w1-gpio-pullup`, and this build must not use it: `R11` is a plain 2.2 kΩ resistor to 3V3.
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
  `bluetoothctl` were already present. That run also dragged the whole util-linux family forward
  from `2.41-5` to `2.41.5-0+deb13u1` as a dependency — **`rfkill` among them**, despite the
  script correctly reporting it present and skipping it. Nothing broke, but a four-package
  pre-flight list is not a statement of what apt will change.
- **The Bluetooth radio shipped SOFT-BLOCKED, persistently — and the block is now CLEARED.**
  `harden-headless.sh` phase 7 ran on 2026-09-09 and the unblock **survived the reboot**:
  `rfkill list bluetooth` reads `Soft blocked: no`, `hciconfig` reads `UP RUNNING`, BlueZ reads
  `Powered: yes` / `PowerState: on`. `systemd-rfkill` now restores *unblocked* from the same
  state directory that used to restore the block. Everything below is why it mattered and how it
  comes back if anyone re-blocks it — as it shipped, `rfkill list` reported `Soft blocked: yes`
  and BlueZ `PowerState: off-blocked`. **In that state there is no BLE and therefore no
  product.** Two traps worth stating plainly:
  1. **`bluetoothctl power on` cannot clear it.** rfkill sits below BlueZ. The service runs, the
     controller enumerates, and it still refuses to power.
  2. **It survives reboots.** `systemd-rfkill` saves per-device state under
     `/var/lib/systemd/rfkill/` and restores it at boot.
  `sudo rfkill unblock bluetooth` clears it, and is persisted the same way.
  `harden-headless.sh` phase 7 does this. For the same reason, **never run `rfkill block all`**
  to gate Wi-Fi — it takes BLE down with it, persistently. Use `rfkill block wifi`.

- **`fake-hwclock` is not installed, and the clock in the car will be wrong.** A Pi 4B has no RTC.
  `systemd-timesyncd` saves the time to `/var/lib/systemd/timesync/clock` and restores it at boot,
  so a session file is never stamped 1970 — but with no NTP in the car the clock simply resumes
  from the last bench sync and is **wrong by however long ago that was**, while looking perfectly
  plausible. **The logger must record an offset against an external time source at session start**
  (the phone's GPS time over the RaceChrono link is the obvious one) rather than trusting the Pi
  clock for anything that has to line up with RaceChrono data.
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
  Gate it per session with `rfkill block wifi` or `nmcli radio wifi off` instead — never
  `rfkill block all`, which takes BLE down with it and persists. Whether it needs gating at all is
  plan item 5a's installed link check to answer, and that check has not been run.

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

## Architecture, when there is code

Follow `iSitePiLogger`, which is the structural model:

- **Single translation unit.** All `.cxx` files are `#include`-d into `main.cxx`. Do not add them
  to `CMakeLists.txt` as independent targets.
- **Procedural workers, no OOP.** Plain `gpointer fn(gpointer)` passed to `g_thread_new()`.
- **`CLOCK_TAI` throughout**, to avoid leap-second discontinuities in sample timestamps and file
  names.
- **The `.ini` sits beside the binary** and its path is resolved from `/proc/self/exe`, not the
  working directory.

Four requirements come from the plan rather than from iSitePiLogger:

- **The SD card is the primary record and BLE is secondary** (item 5b). Raw readings, timestamps,
  counters and validity flags are written locally regardless of link state.
- **Append-only session file, `fsync` on a fixed ~1 s cadence** — not per sample, not only at
  close. The accessory feed disappears without warning at ignition-off, so the last durable write
  bounds the loss. Flushing per sample at 10 Hz buys a shorter window at the price of write
  amplification without changing the failure mode.

- **The logger binds `temp0`–`temp3` itself, by discovery order, and persists the binding**
  (owner decision, 2026-09-09). The owner plugs the four DS18B20s in one at a time, lowest channel
  first; the logger notices each new `28-*` ROM ID and writes the binding to a store that survives
  restarts. This replaces reading ROM IDs off a bench rig and typing them into a config by hand.
  Five things make it correct rather than merely convenient, and skipping any of them produces
  silently mislabelled temperature data — which is worse than no data, because ΔT_preheat rests on
  the *differences* between these four probes:
  1. **`28-*` only.** The bare bus invents churning `00-*` phantoms every 10 s (see the trap
     above). Binding "the next new device" without the family filter binds noise.
  2. **Binding happens only in an explicit enrollment mode, never during a logging run.** A
     dropout and reconnect mid-session, or a probe replaced after a failure, must not silently
     re-bind a channel. Outside enrollment an unknown ROM ID is **invalid data and a logged
     event**, not a new channel.
  3. **One probe per enrollment step.** If two unbound `28-*` IDs appear in the same 10 s scan the
     arrival order between them is unknowable — sysfs order is not arrival order — so the logger
     must refuse the ambiguous step and say so rather than guess.
  4. **The store carries provenance and a per-probe offset field from the first version.** The
     ROM ID, the channel, the timestamp it was bound, and a calibration offset that attaches to
     the ROM ID rather than to the slot. Thermal item 1 needs the offsets; retrofitting the field
     later means a format change.
  5. **Enrollment must report which ROM ID it just bound**, so the binding can be checked against
     the lead being plugged in. **The probes are already installed on the car** (owner,
     2026-09-09) and the owner can identify each lead at the logger end, so enrollment binds
     channel → location directly and the older "mark the probe body" step is moot. Provide an
     identification fallback anyway: with all four bound, **warming one probe by hand must be
     visible as one channel moving**, which confirms the map in situ and doubles as a liveness
     test.
- **A cold-soak spread is recorded at session start** (plan thermal item 1). With the probes
  installed, the bench cross-comparison is replaced by an in-situ common-temperature check, and
  the logger is what captures it: on a cold car at equilibrium all four probes are at one
  temperature, so the spread between them *is* the set of relative offsets — the quantity
  ΔT_preheat depends on, since a difference of two ±0.5 °C probes carries ~1 K against a signal of
  order 6 K. Requirements:
  1. **Record the four raw readings and their spread before anything warms up**, into the session
     file, with the timestamp and the elapsed-since-boot.
  2. **Flag whether the car looks settled rather than asserting it.** A spread taken on a
     heat-soaked or sunlit car is worse than none, because it bakes a false offset into the one
     number the thermal workstream exists to produce. Sun through the grille lands on the
     T_ambient probe specifically. If the four are still visibly drifting relative to each other,
     say so in the record and mark the sample unusable.
  3. **Never auto-apply an offset.** Record the spread as data; applying corrections is the plan's
     decision, not the logger's, and an offset applied silently cannot be un-applied later.
  4. Repeated across sessions at different ambients, these snapshots accumulate the multi-point
     calibration the bench comparison would have given, at no cost.
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
     worth having is a brownout at ignition-off, which is the moment the box loses power.

Preserve CRC failures, clipping, disconnects and stale samples as **invalid data**, never as
carried-forward values presented as new.
