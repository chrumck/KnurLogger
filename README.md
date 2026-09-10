# KnurLogger

Headless data logger for the ND Miata hood-louver instrumentation ("box 2"), running on a
Raspberry Pi 4B in the cavity behind the right wheel well.

It publishes differential pressure, temperature and enclosure conditions over Bluetooth LE as a
[RaceChrono DIY BLE device](https://github.com/aollin/racechrono-ble-diy-device), which is the
**primary data path**, and writes the raw readings and diagnostics to the SD card, which is the
durable record and the only thing that can prove a sample was missing rather than held.

**Status: all four workers are written, and the logger's channels decode correctly in RaceChrono on
a phone** (verified 2026-09-09: `0x602` reads −327.68 °C on all four thermal channels, the
deliberate no-probe-bound sentinel, which confirms packet ID, byte order, signedness and scaling
end to end). The session writer (append-only, ~1 s `fsync`, both measured), the supply-telemetry
worker, the RaceChrono BLE worker and **DS18B20 enrollment** are all done. **No probe is enrolled
yet** — the four are installed on the car, so binding needs a trip to the car; the enrollment
mechanism itself was exercised end to end against a fake 1-Wire tree (see
§[Testing the 1-Wire path without probes](#testing-the-1-wire-path-without-probes)). Getting BLE working needed an
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
`temp0`–`temp3` are fixed by ROM ID at enrollment. The mapping from these to measurement roles
(`T_ambient`, `T_core_in`, `U`, `X`, `C` …) is a per-session record and is logged at boot. Do not
rename a channel after a role. **Pressure is still deliberately undecided. Thermal is decided but
not yet applied** — installing the probes on the car pinned it, and enrolled in installed order it
is temp0=`T_ambient`, temp1=`T_core_in`, temp2=`T_core_out`, temp3=`T_aft`. **No channel is bound
yet** — the enrollment mode exists and works, so what binding now needs is the trip to the car.

All five SDP810s answer at the same fixed I2C address and cannot be strapped apart, which is why
the mux is mandatory rather than a convenience.

## RaceChrono channels — what to type into the phone

RaceChrono channel definitions are entered by hand, so they live here. Add the device from inside
RaceChrono (**Settings → other devices → add a DIY device**), **not** from the phone's Bluetooth
pairing screen: this is a BLE GATT peripheral with no bonding, advertising `BR/EDR Not Supported`,
so the OS pairing list will never show it.

**Every payload field is big-endian. Only the 4-byte packet ID is little-endian**, per the DIY API.
Packet IDs `0x600`–`0x603` are inherited from the ESP32 rig in
`../ndLouvers/step0b-rig/racechrono_ble_test/` so channel definitions written against it carry over.

### `0x602` — the four thermal channels

| Bytes | Channel | Equation |
|---|---|---|
| 0–1 | `temp0` | `bytesToInt(raw, 0, 2) / 100` |
| 2–3 | `temp1` | `bytesToInt(raw, 2, 2) / 100` |
| 4–5 | `temp2` | `bytesToInt(raw, 4, 2) / 100` |
| 6–7 | `temp3` | `bytesToInt(raw, 6, 2) / 100` |

**Signed — use `bytesToInt`, not `bytesToUint`.** Sub-zero ambient is a real reading and would
otherwise decode as ~655 °C. A channel with no trustworthy reading sends `-32768`, i.e. **−327.68 °C**,
deliberately absurd rather than plausible because RaceChrono holds the last value it received
indefinitely and an invalid marker has to be visible.

**These values carry the per-channel calibration offset** from `KnurLogger.ini`, if one is set — see
§[Calibration offsets](#calibration-offsets). The session file records the raw reading beside the
value sent, so the two can always be reconciled.

**Until a probe is enrolled all four read −327.68 °C, and the packet keeps being republished once
per sample cycle.** Unbound, bound-but-absent and read-badly all send the same sentinel, which is
right — none of them is a temperature — and the session file is where the three are told apart. To
see *which* it is from the phone alone, watch `0x603`: byte 0 is how many probes are on the bus and
byte 1 is which channels read cleanly.

### `0x604` — supply health and logger liveness

| Bytes | Content | Equation |
|---|---|---|
| 0 | low nibble = live throttle bits; bit 4 = records dropped; bit 5 = enrollment mode | `bytesToUint(raw, 0, 1)` |
| 1 | sticky throttle bits, latched since **boot** not since session start | `bytesToUint(raw, 1, 1)` |
| 2 | undervoltage comparator; **255 = could not be read**, not "no alarm" | `bytesToUint(raw, 2, 1)` |
| 3–4 | SoC core millivolts — **NOT the supply rail** | `bytesToUint(raw, 3, 2)` |
| 5–6 | SoC temperature | `bytesToInt(raw, 5, 2) / 100` |
| 7 | **heartbeat, +1 per second, wraps at 255** | `bytesToUint(raw, 7, 1)` |

**Byte 7 is the channel to watch, and it is the only honest liveness indicator here.** Every other
field is a physical quantity allowed to sit still — SoC core voltage reads a constant 840 mV on an
idle box for hours — so a frozen value proves nothing about the link. A counter freezing means the
link died; a counter skipping means a notification was dropped, which is exactly what commissioning
item 5a asks to be logged.

### `0x603` — 1-Wire bus health

| Bytes | Content | Equation | Expected with four probes |
|---|---|---|---|
| 0 | probes enumerated | `bytesToUint(raw, 0, 1)` | **4** |
| 1 | valid-this-cycle bitmask, bit *n* = `temp<n>` | `bytesToUint(raw, 1, 1)` | **15** |
| 2–3 | cumulative read errors, saturating | `bytesToUint(raw, 2, 2)` | **0**, and staying there |
| 4–5 | sample cycles | `bytesToUint(raw, 4, 2)` | +1 per second, wraps at 65535 |
| 6–7 | last conversion, ms | `bytesToUint(raw, 6, 2)` | ~750 at 12 bits |

Transcribed byte for byte from the ESP32 rig's `0x603`, so channel definitions written against that
rig carry over. **This is what makes the thermal channels checkable rather than merely present:**
four plausible numbers on `0x602` say nothing about whether they are being read, and bytes 0–3 are
where a probe that dropped off the bus or a bus that is retrying shows up. Byte 4–5 is the thermal
equivalent of `0x604`'s heartbeat — it advances every cycle regardless of what the probes read, so
it separates a dead worker from four steady temperatures.

**Byte 2–3 counts only probes that answered and read badly.** A bound channel whose probe is *absent*
does not increment it, because a dropped lead and a marginal bus send you to different parts of the
car; absence shows up as byte 0 falling and byte 1 losing a bit.

**Bench-verified in the session file, not yet on the phone.** The packing is the same primitive as
`0x604`'s, which is phone-proven, but nobody has yet added these five channel definitions in
RaceChrono and watched them.

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
oneWireProbes.cxx     DS18B20 enrollment, the binding store, 0x602 and 0x603
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
instead and keep GitHub for durable commits. The loop in use is:

```bash
tar czf - --exclude=.git --exclude=build/CMakeCache.txt --exclude=build/CMakeFiles . | ssh KnurLogger 'tar xzf - -C ~/KnurLogger && cd ~/KnurLogger && cmake --build build -j4'
```

> **⚠ That sync OVERWRITES `build/KnurLogger.ini` on the box, which is where the thermal offsets
> live.** The file is tracked in git and has no separate untracked deployed copy, so a hand-edited
> `temp0OffsetC` typed in at the car is destroyed by the next sync from the workstation — silently,
> and the logger will keep running with the offsets reverted to whatever the repo says. Two ways
> round it, and the choice depends on where you are:
> 1. **Preferred: edit the offsets in the repo on the workstation**, commit, then sync. That is the
>    workflow slot-keyed-offsets-in-the-config was chosen for — the calibration ends up
>    version-controlled.
> 2. **If you must edit on the box** (at the car, over the phone hotspot), add
>    `--exclude=build/KnurLogger.ini` to the `tar` above, or copy the box's file back into the repo
>    before the next sync. Otherwise the edit is lost.

**The `../ndLouvers/...` links throughout this repository are broken on github.com and that is
deliberate.** `ndLouvers` is a separate repository that happens to sit alongside this one on disk.
The links resolve locally, which is where the work happens. Do not "fix" them by copying
requirements across — a duplicated requirement is one that will drift, and `CLAUDE.md` says so.

## Getting on the box

SSH alias `KnurLogger` (`192.168.118.52`, user `chrum`), key-only.

```bash
ssh KnurLogger
```

## Enrolling the four DS18B20s

`temp0`–`temp3` mean nothing until a ROM ID is bound to each. Binding is a deliberate mode and
never happens during a logging run, so a probe that drops out and comes back mid-session cannot
re-label a channel.

```bash
ssh KnurLogger 'cd ~/KnurLogger/build && ./KnurLogger --enroll'
```

1. Start with **no probe connected**, then plug them in **one at a time, lowest channel first** —
   the installed order the plan fixes is `temp0` = `T_ambient`, `temp1` = `T_core_in`,
   `temp2` = `T_core_out`, `temp3` = `T_aft`.
2. **Allow ~10 s per probe.** `w1_master_timeout` is 10 s, so that is the hot-plug latency; nothing
   can shorten it without root.
3. Each bind prints `OneWire: BOUND temp0 <- 28-…, reading 21.50 C` and writes an `enrollment`
   record. Check the ROM ID against the lead you just connected.
4. **Two unbound probes in one scan are refused, not guessed.** sysfs order is not arrival order,
   so there is no fact available that says which came first. Unplug one and re-seat it alone.
5. **Enrollment keeps sampling after the fourth bind.** With all four bound, warm one probe by hand
   and watch one channel move on the phone — four warmings confirm the whole map in situ and double
   as a liveness test. Stop it with `pkill -TERM -x KnurLogger`.
6. `--enroll --reset` discards the whole store and starts from `temp0`. The bindings it throws away
   are written into the session file first, because they are otherwise unrecoverable.

The store is `<filesDir>/channels.ini` — deliberately in the data directory rather than beside the
binary, because it is field state that must outlive a rebuild. It carries the ROM ID, the channel
and the bind timestamp, and nothing else; calibration offsets live in `KnurLogger.ini`.

Record the resulting table in `../ndLouvers/step0b-rig/logger-perfboard-wiring.md` §5a, which is
still empty. **Channel → role is a per-session record, never a channel name.**

## Calibration offsets

Hand-edited in `KnurLogger.ini`, one per channel, applied to the value sent to RaceChrono:

```ini
[thermal]
temp0OffsetC=0.0
temp1OffsetC=-0.375
temp2OffsetC=0.0
temp3OffsetC=0.0
```

**How to get the numbers.** Park the car long enough to reach equilibrium — engine cold, no sun, no
residual heat — and run one stationary logging session. At equilibrium all four probes sit in the
same air, so their differences are sensor error and nothing else: read the four values out of the
session file's 1 Hz `temp` records and enter each probe's deviation from the four-probe mean. Only
*relative* offsets mean anything, because there is no reference thermometer on the car and
ΔT_preheat depends on the probes' differences rather than their absolute accuracy. **Entering
nothing is a legitimate outcome** if the differences are small against a ~6 K signal.

Five things to know:

1. **They are keyed to the channel slot, not to the probe.** An offset is really a property of one
   particular DS18B20, so if the probes are ever re-enrolled in a different order, or one is
   swapped, the offsets stay with the slots and no longer describe the parts in them — **re-check
   them after any re-enrollment.** In exchange they live in the git-tracked config beside every
   other setting rather than in the data directory, where a wipe would take them.
2. **The session file records all three values** — `centiC` (raw), `offsetC` (in force) and
   `sentCentiC` (what went on the air) — so a mistyped offset costs a reprocess, not the session.
3. **All four keys must be present.** A missing one is a startup failure, not a silent zero: an
   offset that quietly stopped being applied would be invisible in the data it corrupts.
4. **Past ±5 °C it warns** about a likely decimal-point slip and applies the value anyway; **past
   ±50 °C it refuses to start.** The offsets in force are printed at startup and recorded in the
   `thermalBaseline` record.
5. **The ROM ID → channel bindings are not here.** They are machine-written by `--enroll` into
   `<filesDir>/channels.ini`, which holds bindings only and carries no offset.

**The logger takes no automatic session-start sample and makes no judgement about whether the car
was settled** — see `CLAUDE.md` for why that was tried, measured failing, and dropped.

## Testing the 1-Wire path without probes

The probes are on the car and the phantom `00-*` devices stopped once `R11` terminated the line, so
neither a real reading nor the family filter can be exercised on this box. The whole path is
nonetheless testable, because the code reads sysfs and sysfs can be replaced:

```bash
unshare -Urm --map-root-user /bin/bash -c 'mount --bind /tmp/fake-w1 /sys/bus/w1/devices && ...'
```

An unprivileged user namespace gives a private mount namespace, so a fake tree can be bind-mounted
over `/sys/bus/w1/devices` with **no root and no risk to the real box**. Populate it with a
`w1_bus_master1/` directory, `28-…/w1_slave` files carrying the kernel's two-line format, and a
`00-…` entry to prove the filter drops it. This is how enrollment, the ambiguous-step refusal, the
store's own family and duplicate guards, bound-but-absent, CRC failure, the 85.00 °C power-on
default, out-of-range rejection and the application of a hand-entered offset were all verified.

## Next

**The SDP810 and BME280 readers**, once the pressure sensors arrive — and, needing no code, a trip
to the car to enroll the four probes. Commissioning item 5a's installed BLE link check is
unblocked, and item 5.7's under-load supply telemetry can be collected on the first real run.

`pi-headless-setup.md` §Work Progress is the authority on host state. `CLAUDE.md` carries the five
architecture requirements the plan imposes — BLE as the primary data path with the SD card as the
durable raw/diagnostic record, an append-only file with a ~1 s `fsync` cadence, DS18B20 channel
enrollment, and supply-health telemetry — plus the hardware traps, and two retirement notes that
exist to stop the next agent rebuilding what was deliberately removed. Read it before writing
code.

**Build order was BLE first** (owner, 2026-09-09), and that is now spent — all four workers exist.
The reasoning still matters for the trip to the car: BLE is the primary data path *and* the only
feedback channel there, because the probes are on a car parked in an underground garage with no
network, so a phone watching `temp0`–`temp3` move is the only way to see what enrollment did
without carrying the box home first.

**What is testable on the box today:** the build itself, config loading, the session-file writer
and its fsync cadence, the supply-telemetry worker (`vcgencmd` and the `rpi_volt` hwmon both
answer), a BLE advertiser against a phone, the **BME280 at `0x77`** since the sensor zone was
assembled, and — via the fake-sysfs harness above — **every branch of the 1-Wire worker except a
real reading**. **What is not:** anything requiring a real SDP810 (not delivered) or a real DS18B20
(the probes are on the car), and the mux, which does not answer at `0x70`.
**What the harness does NOT establish:** that a real DS18B20 answers on a real 4 × 5 m star, what
its conversion time is on that bus, or whether `therm_bulk_read` behaves as documented — the
attribute does not exist until a `w1_therm` slave attaches, so the bulk path is unexercised and the
per-probe fallback is the only one that has ever run.
