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
  **This is now live rather than hypothetical:** this repository has a public upstream at
  `github.com/chrumck/KnurLogger`, so every `../ndLouvers/...` link 404s there. That is accepted.
- **This repository is PUBLIC. Weigh that before writing host specifics into it.** It already
  carries the box's LAN IP, its username and its Bluetooth MAC. **The password-authentication
  exposure is closed** — `ssh-harden.sh --execute` ran on 2026-09-09 and the box is key-only
  (`passwordauthentication no`, verified by a forced password-only attempt being refused) — but
  **git history is not retractable**, so the published statement that it once accepted passwords is
  permanent. Nothing here is reachable from the internet (RFC1918 address, and the BT MAC is
  broadcast to anyone in range anyway), and no credential, key or Wi-Fi PSK is in the repo. The
  test for anything new is "would I mind this being permanent and public", not "is it useful now".
  **Measured-state notes are the ones that age badly**: accurate and useful when written, then a
  public status page for a host that may not have been hardened yet. This one took a day to close;
  the next might not.

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
  SDP810s are still being delivered. Every document in both repositories previously said this zone
  was unbuilt and that **an empty I2C scan and zero `28-*` devices were the correct results**.
  That is no longer true and the acceptance criteria moved with it. The first scan of the assembled
  board found three things worth knowing before writing any bus code:
  1. **The BME280 is at `0x77`, and that is now the specified address** (owner decision,
     2026-09-09). It is a genuine BME280, not a mux at a strapped address — chip-ID register
     `0xD0` reads `0x60`. The build sheet used to say "**Do not use `0x77`**" and put `U3.SDO` on
     `GND_SIG`; the owner amended the document rather than the board, so **`0x77` is the address
     to code against** and `0x77` is no longer free for anything else. No collision results,
     because the mux is strapped to `0x70` alone.
  2. **The mux was silent because `~RESET` was soldered to header pin 9 instead of pin 11** —
     resoldered and verified answering at `0x70`, control register `0x00` (owner, 2026-09-09). **Pin 9 is a ground pin and pin 11 is
     GPIO17, and they are adjacent in the same row**, so this is a one-position off-by-one onto
     the worst possible neighbour: a PCA9548A held in reset does not degrade or partly work, it
     goes **completely silent**, which is indistinguishable from an absent or dead part.
     **The diagnostic that localised it is worth keeping.** `gpio=17=op,dh` is live in
     `/boot/firmware/config.txt` and `pinctrl get 17` reads `17: op -- pd | hi` — so the Pi was
     provably driving `~RESET` high while the mux end measured low, which puts the fault on the
     wire rather than on the host or the part. **Whenever a bus device is silent, check the Pi
     side with `pinctrl get <n>` before suspecting the device**; it is one command and it splits
     the search space in half.
     **Do not try to fix a reset problem in software.** GPIO17 was already high, so no
     `pinctrl`/libgpiod write could have helped — it would only have masked the diagnosis.
     Build sheet §2 note 2 and the §3a.7 check table carry the meter checks, including the one
     that **passes on a board with this fault** (`MUX_RST` ↔ `+3V3` still reads `R12`'s 10 kΩ).
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
- **All five SDP810s share one fixed I2C address (`0x25`) and cannot be strapped apart.** The
  TCA9548A mux is therefore mandatory, one sensor per channel. The mux does **not** pass pull-ups
  downstream, so every populated channel has its own pair.
- **The BME280's pressure channel is enclosure pressure, never a static reference.** The cavity is
  aerodynamically live; at Cp −1 the offset is ~464 Pa against 45–90 Pa measurands. It is
  tolerable as a density term and disqualifying as a reference. Name the logged field accordingly.
- **A DS18B20 has no positional anchor.** Its 64-bit ROM ID *is* the channel definition, recorded
  once at enrollment. As of 2026-09-10 **no ROM ID has been recorded, so no `temp` channel is yet
  defined** — `oneWireProbes.cxx` and its `channels.ini` store exist, and the store is empty on the
  box because the probes are on the car. Build sheet §5a's table is correspondingly still blank.
- **The bare 1-Wire bus invented phantom devices, the set CHURNED, and it has now STOPPED —
  because the line is terminated.** With the overlay loaded and **the sensor zone unbuilt**,
  `/sys/bus/w1/devices/` held `w1_bus_master1` plus a varying number of `00-*` entries whose IDs
  changed from scan to scan. Measured 2026-09-09 across 35 s: first `00-800000000000` alone, then
  `00-dc0000000000` + `00-3c0000000000`, then `00-3c0000000000` + `00-bc0000000000`, with
  `w1_master_slave_count` reading `1`, `2`, `2` and `w1_master_attempts` already past 250 with
  nothing attached. **With the sensor zone assembled, three scans over 36 s returned the master
  alone, `slave_count = 0`, and no `00-*` at all.** `R11`'s 2.2 kΩ to `+3V3` is in the sensor zone,
  so `GPIO4` no longer floats on the SoC's internal pull-up and the bus search reads a terminated
  line instead of noise. Well-supported but not proven — nobody re-floated the line to confirm.
  Family code `00` is not a valid 1-Wire family; a DS18B20 is family **`28`**.
  **The filter below is still mandatory** — it is correct regardless of cause, and a marginal
  4 × 5 m star can produce garbage of its own — but it **can no longer be exercised on this box**,
  so its correctness now rests on the parser rather than on an observation.
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
     application of a hand-entered offset were all verified this way. It is also what measured the
     session-start sample's inference failing, which is what retired that requirement. **Reach for this before concluding a sysfs-driven path is
     untestable.** What it does not establish: real bus timing, real conversion time, or
     `therm_bulk_read`.

## BLE advertising: the platform bug that cost a day, and how it was found

- **`bluez 5.82-1.1+rpt1` on kernel `6.18.34` CANNOT advertise on this box. Fixed by
  `apt full-upgrade` on 2026-09-09** → `bluez 5.82-1.1+rpt2`, kernel `6.18.39`,
  `firmware-brcm80211 1:20260519`. If a future image regresses to those versions, this is the
  explanation for a logger that starts, registers its GATT application and is never seen by
  RaceChrono.
  **The symptom** is `binc` logging `failed to register advertisement (error 36:
  GDBus.Error:org.bluez.Error.Failed)` and `bluetoothd` logging
  `add_client_complete() Failed to add advertisement: Invalid Parameters (0x0d)`.
  **The cause is a userspace/kernel structure mismatch**, visible only in an HCI trace: the
  `Add Extended Advertising Data (0x0055)` MGMT command arrives with `plen 14` while the fields it
  declares — instance, `adv_data_len: 3`, `scan_rsp_len: 0` — account for 6 parameter bytes. The
  kernel validates that length and rejects the mismatch. `Available adv data len` was 31, so it was
  never a capacity problem.
- **The diagnostic sequence is the reusable part. Do this before suspecting the logger:**
  1. **`bluetoothctl advertise on`.** If that fails too, the fault is not in this repository and no
     amount of reading `raceChronoBle.cxx` will find it. This single command separates "our code"
     from "the platform" and it should always be step one.
  2. **`journalctl -u bluetooth`** for the `bluetoothd`-side reason, which is more specific than the
     D-Bus error the client sees.
  3. **`btmgmt add-adv -c -u 1ff8 1`** (root). This uses the *legacy* MGMT path. It succeeded
     throughout, which proved the controller and kernel could advertise and narrowed the fault to
     `bluetoothd`'s extended-advertising path.
  4. **`btmon` while triggering an attempt** (root) — the only thing that showed the actual
     malformed command. `btmon -w file` then `btmon -r file`.
- **Three plausible-sounding explanations were tested and were all wrong.** Recorded so nobody
  spends the time again: it was **not** advertising-data overflow (a 4-character device name failed
  identically), **not** `max_adv_data_len` (31, ample), and **not** connectability, daemon config or
  stale daemon state (`ControllerMode = le`, `Experimental = true` and a `bluetooth` restart each
  changed nothing).
- **`SupportedInstances` and `ActiveInstances` on `org.bluez.LEAdvertisingManager1`** are the quick
  check that an advertisement actually registered. `ActiveInstances: 1` while the logger runs is
  the acceptance; `0` means it silently did not.

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
  **Both halves took on 2026-09-09** and `/dev/i2c-1` exists. It no longer scans empty, and an
  empty scan is no longer the expected result — see the sensor-zone status below. **`/dev/i2c-20` and `/dev/i2c-21` exist too and are not yours** — they
  are the VC4 display DDC buses, which `i2c-dev` exposes against adapters the KMS driver
  registers. The perfboard is on **bus 1** and nothing else. Their appearance is how the two
  halves were told apart mid-run: loading `i2c-dev` before the reboot produced 20 and 21 but no 1,
  because the `i2c_arm` adapter does not exist until the firmware re-reads `config.txt`.
- **`w1-gpio`'s `pullup` parameter is ignored** on this firmware — the overlays README says so
  outright. The overlay that drives an external strong pullup is a different one,
  `w1-gpio-pullup`, and this build must not use it: `R11` is a plain 2.2 kΩ resistor to 3V3.
- **The tar-over-ssh build loop OVERWRITES the deployed `build/KnurLogger.ini`, and that file now
  holds the thermal offsets** (found 2026-09-10 during the wrap-up pass, before it bit anyone).
  `build/KnurLogger.ini` is tracked in git and there is no separate untracked deployed copy, so an
  offset typed in on the box is destroyed by the next sync from the workstation — silently, with
  the logger carrying on using the repo's values. Edit offsets in the repo and sync, or add
  `--exclude=build/KnurLogger.ini` to the `tar` when editing on the box. The README's repository
  section carries the loop and both workarounds.
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

- **Some channels the plan needs will never appear on box 2's SD card.** CAN ambient
  (`0x420` byte 7) and, if it is on the bus, **cooling-fan state** arrive through box 1's CAN
  broadcast into RaceChrono — not through this logger (`../ndLouvers/` §7 open item 35). So the
  record for a session is **split across two devices, and RaceChrono is what reassembles it** —
  which is the whole reason BLE is the primary data path. It is *not* reassembled by a
  session-start time offset; that was the earlier reading, and it is superseded. The consequence
  that survives is sharper: **a box-2 channel that never reaches the phone cannot be aligned to
  the fan state that explains it**, and the fan can change state mid-run with no driver input and
  no speed change. So a BLE gap is not a cosmetic loss — it is the loss of the only link between
  box 2's pressures and the fan behaviour that conditions them.
- **`fake-hwclock` is not installed, and the clock in the car will be wrong.** A Pi 4B has no RTC.
  `systemd-timesyncd` saves the time to `/var/lib/systemd/timesync/clock` and restores it at boot,
  so a session file is never stamped 1970 — but with no NTP in the car the clock simply resumes
  from the last bench sync and is **wrong by however long ago that was**, while looking perfectly
  plausible. **The car has no network at all** — it lives in an underground garage with no cell
  coverage (owner, 2026-09-09) — so there is no NTP to reach even in principle.
  **Do not build a GPS-time fetch to fix this.** The earlier requirement to record an offset
  against the phone's GPS time over the RaceChrono link is superseded twice over: RaceChrono
  stamps every source on arrival, so cross-device alignment does not need the Pi clock, and the
  DIY BLE protocol carries no time transfer to fetch it with. What the logger does instead is
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

## Architecture

Follow `iSitePiLogger`, which is the structural model:

- **Single translation unit.** All `.cxx` files are `#include`-d into `main.cxx`. Do not add them
  to `CMakeLists.txt` as independent targets.
- **`initialiseBlePackets()` is called exactly once, from `main`, before any worker starts.**
  `raceChronoBleLoop` used to call it a second time and that was a latent bug, harmless only while
  nothing produced a thermal reading: it re-runs `g_mutex_init` on live mutexes and resets `0x602`
  to the all-invalid sentinel, so once `oneWireProbes` existed a real reading taken before the BLE
  worker finished starting would have been silently clobbered back to −327.68 °C. Removed
  2026-09-10. Initialise shared packet state in `main`, never in a worker.
- **Procedural workers, no OOP.** Plain `gpointer fn(gpointer)` passed to `g_thread_new()`.
- **`CLOCK_TAI` throughout**, to avoid leap-second discontinuities in sample timestamps and file
  names.
- **The `.ini` sits beside the binary** and its path is resolved from `/proc/self/exe`, not the
  working directory.

Five requirements came from the plan rather than from iSitePiLogger. **Four stand; the session-start thermal sample was retired whole on 2026-09-10 and is the fifth entry below, kept as a retirement note rather than deleted** — git history and plan revisions up to rev 71 still describe it as live.

- **BLE is the primary data path; the SD card is the durable raw and diagnostic record**
  (owner decision, 2026-09-09, item 5b — this **reverses** the earlier "SD primary, BLE
  secondary", so do not reinstate it from memory or from git history). The analysis record is
  RaceChrono's consolidated log, because RaceChrono is what collects box 1's CAN broadcast, box
  2's channels and the phone's GPS and stamps them into one frame set on one timebase. Box 2's
  channels have to reach the phone to be useful.
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
     ignition-off — the moment the link and the logger both stop.
- **Note, not a sixth requirement — this one RETIRES part of an earlier requirement.** The
  session-start clock offset is no longer load-bearing, and its stated mechanism does not exist. Because RaceChrono stamps every source on arrival, box 1 and box 2 are never aligned
  against each other's clocks. Keep recording elapsed-since-boot beside the wall clock in local
  records, but **do not build a GPS-time fetch**: the RaceChrono DIY protocol is device→phone
  notifications plus a filter-write channel and carries no time transfer.
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
  4. **The store carries provenance: the ROM ID, the channel and the bind timestamp.** It also
     carried a per-probe `offsetC` from its first version, on the reasoning that thermal item 1
     needed somewhere to put offsets and retrofitting the field later would be a format change.
     **That is retired** (owner decision, 2026-09-10): offsets are slot-keyed in
     `KnurLogger.ini`, and the key is gone from the store rather than left dead — two fields that
     look like an offset, one of which does nothing, is worse than one.
  5. **Enrollment must report which ROM ID it just bound**, so the binding can be checked against
     the lead being plugged in. **The probes are already installed on the car** (owner,
     2026-09-09) and the owner can identify each lead at the logger end, so enrollment binds
     channel → location directly and the older "mark the probe body" step is moot. Provide an
     identification fallback anyway: with all four bound, **warming one probe by hand must be
     visible as one channel moving**, which confirms the map in situ and doubles as a liveness
     test.
  6. **A BOUND channel whose ROM ID goes absent must be logged as present-but-invalid, never
     omitted** (owner, 2026-09-10 — this was NOT in the five above and it is not implied by them).
     Iterating the four channels rather than the devices present on the bus is what produces it. If
     the record simply loses the channel, a mid-session dropout becomes indistinguishable from the
     logger not having run — which destroys the one property the SD file exists for, namely that a
     gap in the local stream is the only evidence a sample was missing rather than held. The
     sentinel goes out on `0x602` for it, and `reason` in the session record separates `unbound`,
     `absent`, `crc`, `powerOnDefault`, `outOfRange` and `readFailed`.
     **`absent` must not increment the read-error counter.** A dropped lead and a marginal bus send
     you to different parts of the car, and inflating `0x603` byte 2–3 with absences would bury the
     bus-quality signal it exists to carry.
  7. **Enrollment must keep running after the fourth bind** (owner, 2026-09-10 — also not in the
     five). Requirement 5's fallback check *is* "warm one probe and watch which channel moves", and
     that needs a logger still sampling and still notifying. An enroller that exits on the fourth
     bind silently removes the only in-situ verification of the map. It reports a fifth ROM ID once
     and refuses it; `--enroll --reset` is the way to start over.
- **RETIRED, and do not reinstate it: the logger takes NO session-start thermal sample**
  (owner decision, 2026-09-10). It used to. A "cold-soak spread" / `sessionStartCommonTemperature`
  requirement stood here from 2026-09-09, was implemented, and was then **dropped whole** — the
  `commonTemperature` record, the `settling*` config keys and the drift fit are all gone. Git
  history and plan revisions up to rev 71 still describe it, so this note exists to stop the next
  agent rebuilding it from either.
  **Why it went, because the reasoning is worth more than the feature.** The requirement was to
  *flag* whether the car looked settled rather than assert it, and settledness was inferred from
  each probe's drift rate across a 90 s window. That inference does not work, and it was measured
  failing: a car parked ~5–6 h drifts about 11 mK/min, which over 90 s is ~18 mK against the
  DS18B20's 62.5 mK code step — so **zero code transitions, a fitted drift of exactly 0.0 mK/min,
  and `spreadUsable: true`** while the bay still held an 810 mK real gradient that would have gone
  straight into ΔT_preheat. Note what that rules out: **no threshold fixes it**, because the
  reported drift is exactly zero rather than merely small, and resolving 11 mK/min needs a
  20–30 minute window, which is not a session start. Drift *rate* and level *gradient* are
  independent quantities, and inferring "no gradient" from "no drift" was the error.
  **Nothing is lost by dropping it.** Every session already logs all four probes' absolute
  readings at 1 Hz, so a cold soak the owner *knows* was a cold soak is still fully derivable from
  the ordinary `temp` records by hand. What went was the automatic 90 s summary and its unreliable
  verdict, not the calibration.
- **Per-channel offsets are hand-entered in `KnurLogger.ini` and ARE APPLIED to what goes to
  RaceChrono** (owner decisions, 2026-09-10). Two things were reversed here on the same day and
  both are recorded so nobody restores them from git history:
  1. **"Never auto-apply an offset — an offset applied silently cannot be un-applied later" is
     REVERSED.** The objection was put to the owner and answered on its own terms: it is neither
     silent nor irreversible, because the session record carries the raw reading (`centiC`), the
     offset in force (`offsetC`) and the value that actually went on the air (`sentCentiC`) side by
     side. A mistyped offset costs a reprocess, not a session. **That record-both property is the
     entire basis on which the reversal is safe — anything that later drops the raw value re-opens
     the original objection.**
  2. **The offsets are SLOT-keyed in the config, not ROM-ID-keyed in the store.** They were
     briefly the latter; `channels.ini` no longer has an `offsetC` key at all. The owner chose the
     simplification after the trade-off was put to them, so do not "restore" ROM-ID keying.
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
     worth having is a brownout at ignition-off, which is the moment the box loses power.

Preserve CRC failures, clipping, disconnects and stale samples as **invalid data**, never as
carried-forward values presented as new.
