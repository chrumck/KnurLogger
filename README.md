# KnurLogger

Headless data logger for the ND Miata hood-louver instrumentation ("box 2"), running on a
Raspberry Pi 4B in the cavity behind the right wheel well.

It publishes differential pressure, temperature and enclosure conditions over Bluetooth LE as a
[RaceChrono DIY BLE device](https://github.com/aollin/racechrono-ble-diy-device), which is the
**primary data path**, and writes the raw readings and diagnostics to the SD card, which is the
durable record and the only thing that can prove a sample was missing rather than held.

**Status: the logger builds, runs and advertises to RaceChrono.** The session writer (append-only,
~1 s `fsync`, both measured), the supply-telemetry worker and the RaceChrono BLE worker are done;
DS18B20 enrollment is the remaining one of the four. Getting BLE working needed an
`apt full-upgrade` on 2026-09-09 — `bluez 5.82-1.1+rpt1` on kernel `6.18.34` could not register an
advertisement at all, which `CLAUDE.md` records in full because the symptom points at the logger
and the cause is not in it. The host setup under `SystemSetup/` **has been applied**
(2026-09-09): dependencies installed, the boot-time pass run, rebooted and re-audited. `/dev/i2c-1`
and the 1-Wire bus exist, the build toolchain is installed, and the
Bluetooth soft block is cleared and survived a reboot, and the box is **key-only over SSH**
(`ssh-harden.sh` ran too, so all four scripts have now been applied), and the box has since taken a
full upgrade to kernel `6.18.39` / `bluez 5.82-1.1+rpt2`.
**The perfboard's sensor zone is assembled**, minus the pressure-sensor part — the five SDP810s
are still being delivered. So the buses are no longer silent, and an empty I2C scan is no longer
the correct result. The BME280 answers at **`0x77`**, which is now its specified address; the mux
is a **PCA9548A** and was silent because its `~RESET` had been soldered to header pin 9 instead of
pin 11 — resoldered and verified answering at `0x70`; and the 1-Wire phantoms have stopped, which
is attributed to `R11` terminating the line. `CLAUDE.md` has the detail.

---

## What this is for

The measurement requirements, acceptance criteria and channel definitions live in
`../ndLouvers/CFD-Learning-Plan.md` Step 0b, which is the authority. This repository owns the
software and the host configuration that satisfy them, and owns no measurement decision.

Read, in this order, before changing anything here:

1. `../ndLouvers/CFD-Learning-Plan.md` — Step 0b, especially commissioning items 2, 4, 5a and 5b.
2. `../ndLouvers/step0b-rig/logger-perfboard-wiring.md` — pinouts, I2C addresses, mux channel
   numbering and bring-up order. Its §3a net list is the authority on every connection.
3. `SystemSetup/pi-headless-setup.md` — the host runbook.

## Hardware it drives

| Part | Interface | Channels |
|---|---|---|
| 5 × Sensirion SDP810 (4 × ±500 Pa, 1 × ±125 Pa) | I2C `0x25` behind a PCA9548A mux at `0x70` | `P0`–`P5` |
| 4 × DS18B20 | 1-Wire on GPIO4, addressed by 64-bit ROM ID | `temp0`–`temp3` |
| BME280 | I2C `0x77` on the main bus (amended from `0x76`, 2026-09-09) | enclosure pressure, humidity |

**Channel names are positional and carry no meaning.** `P0`–`P5` are fixed by mux position;
`temp0`–`temp3` are fixed by ROM ID at build time. The mapping from these to measurement roles
(`T_ambient`, `T_core_in`, `U`, `X`, `C` …) is a per-session record and is logged at boot. Do not
rename a channel after a role. **Pressure is still deliberately undecided. Thermal is decided but
not yet applied** — installing the probes on the car pinned it, and enrolled in installed order it
is temp0=`T_ambient`, temp1=`T_core_in`, temp2=`T_core_out`, temp3=`T_aft`. No channel is bound
yet, because binding needs a logger.

All five SDP810s answer at the same fixed I2C address and cannot be strapped apart, which is why
the mux is mandatory rather than a convenience.

## Layout

Single translation unit: every `.cxx` is `#include`-d into `main.cxx`, in the order below, and only
`main.cxx` is named in `CMakeLists.txt`.

```
main.cxx              argument parsing, worker startup, thread joins
dataContracts.hpp     includes, constants, the config and state structs
appData.cxx           the two globals
helpers.cxx           TAI and boot clocks, sysfs and subprocess readers
config.cxx            the .ini, resolved from /proc/self/exe
sessionWriter.cxx     append-only NDJSON, record queue, ~1 s fsync cadence
blePackets.cxx        RaceChrono packet wire format — before every producer
supplyMonitor.cxx     vcgencmd + rpi_volt hwmon at 1 Hz, sticky-bit transitions
raceChronoBle.cxx     bluez_inc: adapter, advertisement, GATT, notify timers
bluez_inc/            submodule, github.com/weliem/bluez_inc

build/KnurLogger.ini  config template; the deployed copy sits beside the binary

SystemSetup/          host configuration; nothing here is logger code
  pi-headless-setup.md    the runbook — start here
  audit-boot.sh           read-only survey of what the box runs at boot
  install-dependencies.sh packages the build needs
  harden-headless.sh      boot-time service reduction and bus configuration
  ssh-harden.sh           key-only SSH
  KnurLogger.service      systemd unit — written, NOT yet installed
```

## Relationship to the other two loggers

| | KnurDash | iSitePiLogger | KnurLogger |
|---|---|---|---|
| Display | 4.3" DSI touchscreen, GTK under `startx` | none | none |
| Primary record | RaceChrono on the phone | files uploaded over HTTP | **RaceChrono over BLE**; SD card is the durable raw/diagnostic record |
| Radios | BLE | both disabled | **BLE required**, Wi-Fi gated per session |
| Language | C | C++, single translation unit | C++, single translation unit |

**KnurDash** contributes the RaceChrono BLE protocol implementation and its `bluez_inc` usage.
It is not the structural model: it is a GUI dash that boots into `startx`, and this box has no
display.

**iSitePiLogger** is the structural model — `SystemSetup/` layout, pre-flight-by-default scripts,
single-translation-unit CMake build, procedural GLib workers, an `.ini` beside the binary,
`CLOCK_TAI` throughout. Two things in it must **not** be copied, and both are load-bearing:

1. Its `setupNotes.txt` disables Bluetooth. BLE is this box's product.
2. Its `setupNotes.txt` disables Wi-Fi. This box lives in a wheel-well cavity with no Ethernet,
   so Wi-Fi is the only way back in; it is gated per session instead.

## Repository and where the code is built

Upstream is **https://github.com/chrumck/KnurLogger** (`origin`), and it is **public** — see the
note at the end of this section before adding anything host-specific.

The box has the full toolchain (`git 2.47.3`, `cmake 3.31.6`, `gcc`/`g++`, `libglib2.0-dev`), so
**the code is built on the Pi**, not cross-compiled. First time:

```bash
ssh KnurLogger 'git clone https://github.com/chrumck/KnurLogger.git'
```

Thereafter `git -C ~/KnurLogger pull` on the box. The `build/` directory is gitignored except for
its tracked `KnurLogger.ini` template; the deployed `.ini` carries real values and is ignored.

**Do not put push credentials on the box.** Commit and push from the workstation; the box pulls
only. A logger in a wheel-well cavity is physically exposed — if the car is broken into or the SD
card is pulled, any PAT or writable deploy key on it becomes an attacker's write access to this
repo. A public repo needs no credential to clone or pull, so read-only costs nothing.

For a tight edit-build loop, round-tripping through GitHub is slow; copy the tree to the box
instead (`scp -r` or `rsync -a --exclude build/`) and keep GitHub for durable commits.

**The `../ndLouvers/...` links throughout this repository are broken on github.com and that is
deliberate.** `ndLouvers` is a separate repository that happens to sit alongside this one on disk.
The links resolve locally, which is where the work happens. Do not "fix" them by copying
requirements across — a duplicated requirement is one that will drift, and `CLAUDE.md` says so.

## Getting on the box

SSH alias `KnurLogger` (`192.168.118.52`, user `chrum`), key-only.

```bash
ssh KnurLogger
```

## Next

**DS18B20 enrollment is the remaining worker**, and after it the SDP810 and BME280 readers once
the pressure sensors arrive. The binary now exists, so commissioning item 5a's installed BLE link
check is unblocked and item 5.7's under-load supply telemetry can be collected on the first real
run.

`pi-headless-setup.md` §Work Progress is the authority on host state. `CLAUDE.md` carries the five
architecture requirements the plan imposes — BLE as the primary data path with the SD card as the
durable raw/diagnostic record, an append-only file with a ~1 s `fsync` cadence, DS18B20 channel
enrollment, a session-start common-temperature sample, and supply-health telemetry — plus the
hardware traps. Read it before writing code.

**Build order: BLE first** (owner, 2026-09-09). It is the primary data path, and it is also the
only feedback channel at the car — the probes are installed on a car parked in an underground
garage with no network, so a phone watching `temp0`–`temp3` move is the only way to see what
enrollment did without carrying the box home first.

**What is testable on the box today:** the build itself, config loading, the session-file writer
and its fsync cadence, the supply-telemetry worker (`vcgencmd` and the `rpi_volt` hwmon both
answer), a BLE advertiser against a phone, and — newly, since the sensor zone was assembled — the
**BME280 at `0x77`**. **What is not:** anything requiring a real SDP810 (not delivered) or a real
DS18B20 reading (the probes are on the car), and the mux, which does not answer at `0x70`.
**No longer testable:** the 1-Wire phantom filter. The phantoms stopped once `R11` terminated the
line, so the filter is still mandatory but its correctness now rests on the parser rather than on
an observation.
