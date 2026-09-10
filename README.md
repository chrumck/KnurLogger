# KnurLogger

Headless data logger for the ND Miata hood-louver instrumentation ("box 2"), running on a
Raspberry Pi 4B in the cavity behind the right wheel well.

It publishes differential pressure, temperature and enclosure conditions over Bluetooth LE as a
[RaceChrono DIY BLE device](https://github.com/aollin/racechrono-ble-diy-device), which is the
**primary data path**, and writes the raw readings and diagnostics to the SD card, which is the
durable record and the only thing that can prove a sample was missing rather than held.

**Status: all five workers are written, all four DS18B20s are enrolled, the loaded 4 × 5 m
1-Wire star reads CRC-clean, and the BME280 is read and logged.** What the logger has never done
is run on a moving car.

> **⚠ `bme280IntervalMs` is a NEW REQUIRED KEY in `[sensors]`, and the production `.ini` is not
> in git.** A missing key is a startup failure, like every other key in this file, so **the
> deployed logger will refuse to start until the line is added to `~/bin/KnurLogger.ini`** —
> `deploy-logger.sh` replaces the binary and never updates the `.ini`. Add it before deploying:
>
> ```bash
> ssh KnurLogger "grep -q bme280IntervalMs ~/bin/KnurLogger.ini || sed -i '/^bme280Address=/a bme280IntervalMs=1000' ~/bin/KnurLogger.ini; grep -A3 '^\[sensors\]' ~/bin/KnurLogger.ini"
> ```

The session writer (append-only, ~1 s `fsync`, both measured), the supply-telemetry worker, the
RaceChrono BLE worker, **DS18B20 enrollment** and the **BME280 reader** are all done, and the
thermal and supply channels decode correctly in
RaceChrono on a phone (2026-09-09: `0x602` read −327.68 °C on all four thermal channels, the
deliberate no-probe-bound sentinel, which confirms packet ID, byte order, signedness and scaling
end to end). **The enrollment** (2026-09-10, at the car): 63 consecutive cycles enumerated four probes with
a valid-mask of 15 every cycle — zero read errors, zero CRC failures, zero non-probe entries —
which closes the plan's thermal item 1 first requirement. `temp0` = `28-06254385da1f`,
`temp1` = `28-0625424044b7`, `temp2` = `28-062542ac86b6`, `temp3` = `28-0625424e16c9`, in installed
order. An earlier attempt the same day bound three and was abandoned, having unplugged each probe
as the next went in; three faults it exposed are fixed (`CLAUDE.history.md` §1.11–§1.13).
**Sampling is 0.31 Hz, not 1 Hz, and the udev rule did not fix it.**
`SystemSetup/grant-w1-bulk-read.sh` cleared the `EACCES` on `therm_bulk_read` and the write is now
accepted — but with four real probes the cycle time did not move (3190–3309 ms over 331 cycles,
every probe still ~800 ms). **A successful write is not a conversion.** The first suspect is
parasite power; `CLAUDE.history.md` §1.14 has the diagnosis and the next step.

**The map is independently confirmed** (2026-09-10): warming each probe in installed order moved
`temp0`, `temp1`, `temp2`, `temp3` in that order with clean separation, +3.8 to +5.4 K each.
**The cold-soak calibration is done and the answer is no offsets** — 12.9 undisturbed minutes,
four-probe mean 21.958 °C, spread 0.193 K, which is ~3 % of a ~6 K ΔT_preheat signal and cannot be
separated from a real spatial gradient across four locations. All four `temp<N>OffsetC` stay 0.0.
**A 7.53 h unattended run holds up** (2026-09-10, bench, open air, no probes bound, no phone
connected): 27,123 sample cycles with inter-cycle gaps of median 1002 ms and a **maximum of
1004 ms**, zero gaps over 2 s, zero dropped records, zero error events, `throttled` live and
sticky 0 throughout, SoC temperature 50.1–55.0 °C, and a clean closing record on SIGTERM. Session
growth measured **1457 B/s**, so ~6 MB/h once four probes report — about 72 MB for a 12 h day
against 108 GB free. It exercised neither BLE notify load (`0 notifications sent`) nor the sealed
enclosure in the wheel well, where the thermal picture will be different. Getting BLE working needed an
`apt full-upgrade` on 2026-09-09 — `bluez 5.82-1.1+rpt1` on kernel `6.18.34` could not register an
advertisement at all, which `CLAUDE.history.md` records in full because the symptom points at the
logger and the cause is not in it; `CLAUDE.md` carries the diagnostic sequence to reuse. The host setup under `SystemSetup/` **has been applied**
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
is attributed to `R11` terminating the line. `CLAUDE.md` has the facts to code against;
`CLAUDE.history.md` has the diagnoses behind them.

---

## What this is for

The measurement requirements, acceptance criteria and channel definitions live in
`../ndLouvers/CFD-Learning-Plan.md` Step 0b, which is the authority. This repository owns the
software and the host configuration that satisfy them, and owns no measurement decision.

Read, in this order, before changing anything here:

1. `../ndLouvers/CFD-Learning-Plan.md` — Step 0b, especially commissioning items 2, 4, 5a and 5b.
2. `Hardware/logger-perfboard-wiring.md` — pinouts, I2C addresses, mux channel numbering and
   bring-up order. Its §3a net list is the authority on every connection. It is subordinate to the
   plan's Step 0b, which it lived alongside until 2026-09-10.
3. `SystemSetup/pi-headless-setup.md` — the host runbook.

## Hardware it drives

| Part | Interface | Channels |
|---|---|---|
| 5 × Sensirion SDP810 (4 × ±500 Pa, 1 × ±125 Pa) | I2C `0x25` behind a PCA9548A mux at `0x70` | `P0`–`P5` |
| 4 × DS18B20 | 1-Wire on GPIO4, addressed by 64-bit ROM ID | `temp0`–`temp3` |
| BME280 | I2C `0x77` on the main bus (amended from `0x76`, 2026-09-09) | enclosure pressure, cavity temperature, humidity |

**Channel names are positional and carry no meaning.** `P0`–`P5` are fixed by mux position;
`temp0`–`temp3` are fixed by ROM ID at enrollment. The mapping from these to measurement roles
(`T_ambient`, `T_core_in`, `U`, `X`, `C` …) is a per-session record and is logged at boot. Do not
rename a channel after a role. **Pressure is still deliberately undecided. Thermal is decided
and applied** — installing the probes on the car pinned it, and enrolled in installed order it
is temp0=`T_ambient`, temp1=`T_core_in`, temp2=`T_core_out`, temp3=`T_aft`. **All four are bound**
(2026-09-10); `Hardware/logger-perfboard-wiring.md` §5a has the ROM IDs. **The role map is
independently confirmed** — warming each probe in installed order moved `temp0`–`temp3` in that
order, +3.8 to +5.4 K each.

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

### `0x600` — enclosure conditions

**These two IDs used to be the ESP32 rig's synthetic test frames and no longer are** (owner
decision, 2026-09-10). That rig is spent, so the IDs were released for real use. **A channel
definition written against the rig's `0x600` decodes garbage here and must be re-entered.**
`0x602`–`0x604` are unchanged and still carry over.

| Bytes | Channel | Equation | Invalid |
|---|---|---|---|
| 0–3 | enclosure pressure, Pa | `bytesToUint(raw, 0, 4)` | `4294967295` |
| 4–5 | cavity temperature | `bytesToInt(raw, 4, 2) / 100` | `-32768` → −327.68 °C |
| 6–7 | enclosure humidity, % | `bytesToUint(raw, 6, 2) / 100` | `65535` → 655.35 % |

**Temperature is signed — use `bytesToInt`.** Pressure and humidity are unsigned and use
`bytesToUint`; pressure needs the full four bytes because absolute pressure does not fit in two
at 1 Pa resolution. Each invalid marker is absurd after the divide for the same reason `0x602`'s
is: RaceChrono holds the last value it received indefinitely.

**Name these three channels for their roles, not for the part**, because two of the three are
easy to point at the wrong thing:

1. **Pressure is ENCLOSURE pressure and never a static reference.** The cavity is
   aerodynamically live: at Cp −1 the offset is ~464 Pa against 45–90 Pa measurands, five to ten
   times the signal, and it is speed-correlated so it does not average out of a speed sweep. It
   is tolerable as a density term and disqualifying as a reference.
2. **Temperature is the cavity thermometer** (plan item 1c), with Pi SoC temperature on `0x604`
   as a cross-check rather than the primary proxy. **It is not the inlet density term** — that is
   `T_ambient`'s DS18B20 on `0x602` byte 0–1.
3. **Humidity is a seal and desiccant diagnostic** for the condensation risks in plan items 1a
   and 1d. It has no measurement consumer.

### `0x601` — BME280 health

| Bytes | Content | Equation | Expected |
|---|---|---|---|
| 0 | bit 0 present, bit 1 calibration read, bit 2 pressure valid, bit 3 temperature valid, bit 4 humidity valid | `bytesToUint(raw, 0, 1)` | **31** |
| 1 | chip ID as read | `bytesToUint(raw, 1, 1)` | **96** (`0x60`); `88` would be a BMP280 |
| 2–3 | cumulative read errors, saturating | `bytesToUint(raw, 2, 2)` | **0** |
| 4–5 | sample cycles | `bytesToUint(raw, 4, 2)` | +1 per second, wraps every **18.2 h** |
| 6–7 | last read, ms | `bytesToUint(raw, 6, 2)` | **~15** |

**This is `0x603`'s argument applied to the BME280, and it matters more here.** A sealed cavity's
temperature and pressure legitimately sit still for minutes, so a frozen `0x600` is not by itself
evidence of anything. Byte 4–5 advances every cycle regardless of what the part reports, which is
what separates a dead worker from a still cavity.

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
| 4–5 | sample cycles | `bytesToUint(raw, 4, 2)` | +1 per second, wraps at 65535 — i.e. every **18.2 h** |
| 6–7 | last conversion, ms | `bytesToUint(raw, 6, 2)` | **~3200 with four probes** — this is the whole cycle's cost while the bulk path does nothing; ~800 if it ever works |

Transcribed byte for byte from the ESP32 rig's `0x603`, so channel definitions written against that
rig carry over. **This is what makes the thermal channels checkable rather than merely present:**
four plausible numbers on `0x602` say nothing about whether they are being read, and bytes 0–3 are
where a probe that dropped off the bus or a bus that is retrying shows up. Byte 4–5 is the thermal
equivalent of `0x604`'s heartbeat — it advances every cycle regardless of what the probes read, so
it separates a dead worker from four steady temperatures.

**Byte 2–3 counts only probes that answered and read badly.** A bound channel whose probe is *absent*
does not increment it, because a dropped lead and a marginal bus send you to different parts of the
car; absence shows up as byte 0 falling and byte 1 losing a bit.

**A probe that has just been unplugged is `absent` for this purpose too, for ~100 s before it looks
it.** The kernel keeps an unregistered slave's sysfs entry for `w1_slave_ttl` (10) missed searches
at `w1_master_timeout` (10 s), and reads of it succeed and return nothing — so byte 0 stays up,
byte 1 loses its bit, and byte 2–3 stays put. That last part was a bug until 2026-09-10, when
unplugging probes during enrollment charged 186 read errors to a bus that had not failed once
(`CLAUDE.history.md` §1.12). The session file counts these separately as `notAnswering`, so a bus
that really is dropping out is still visible; it is just not confused with one that read badly.

**Byte 6–7 reads ~3200 with four probes bound**, because the bulk conversion path does not work on
this box and the reads are sequential — see §[Next](#next). **It read 0 for one session**, which is
how the fault was found: the field reported the bulk wait whenever the *write* succeeded, so a
0 ms wait masked ~800 ms of real conversion. It now reports what the cycle actually paid, and the
`temp` records carry `bulkState` — the raw `therm_bulk_read` readback — beside it.

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
oneWireProbes.cxx     DS18B20 enrollment, the bindings in the .ini, 0x602 and 0x603
i2cBus.cxx            I2C_RDWR transport with the mandatory retry — the mux will share it
bme280Sensor.cxx      BME280 forced-mode reads, compensation, 0x600 and 0x601
raceChronoBle.cxx     bluez_inc: adapter, advertisement, GATT, notify timers
bluez_inc/            submodule, github.com/weliem/bluez_inc

build/KnurLogger.ini  config TEMPLATE; the deployed copy is ~/bin/KnurLogger.ini on the box

Hardware/             box-2 hardware; nothing here is logger code
  logger-perfboard-wiring.md  the perfboard build sheet — §3a's net list is the authority
                              on every connection. Subordinate to the plan's Step 0b.

SystemSetup/          host configuration; nothing here is logger code
  pi-headless-setup.md    the runbook — start here
  audit-boot.sh           read-only survey of what the box runs at boot
  install-dependencies.sh packages the build needs
  harden-headless.sh      boot-time service reduction and bus configuration
  ssh-harden.sh           key-only SSH
  deploy-logger.sh        installs the production binary into ~/bin, never its .ini
  grant-w1-bulk-read.sh   udev rule for therm_bulk_read — the difference between 1 Hz and 0.31 Hz
  60-knurlogger-w1-bulk-read.rules  what that script installs
  KnurLogger.service      systemd unit — INSTALLED and enabled, survives a reboot
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

## Repository, deployment and where the code is built

Upstream is **https://github.com/chrumck/KnurLogger** (`origin`), and it is **public** — see the
note at the end of this section before adding anything host-specific.

The box has the full toolchain (`git 2.47.3`, `cmake 3.31.6`, `gcc`/`g++`, `libglib2.0-dev`), so
**the code is built on the Pi**, not cross-compiled. First time:

```bash
ssh KnurLogger 'git clone https://github.com/chrumck/KnurLogger.git'
```

Thereafter `git -C ~/KnurLogger pull` on the box. The `build/` directory is gitignored except for
its tracked `KnurLogger.ini`, which is the **template**, not the deployed file.

### The dev copy and the production copy are two different files

The logger is **built** in `~/KnurLogger/build/` and **run** from `~/bin/`, and the two are kept
apart deliberately (owner decision, 2026-09-10):

| | Path | Owned by | Carries |
|---|---|---|---|
| dev | `~/KnurLogger/build/KnurLogger.ini` | git, overwritten by every sync | the seed, and the **version-controlled backup** of whatever was last copied back |
| production | `~/bin/KnurLogger.ini` | the box and `--enroll` | the live ROM ID bindings and offsets |

The dev copy is not permanently empty: it carries the four ROM IDs enrolled on 2026-09-10, copied
back after the trip. That is what makes it a backup rather than only a template — a fresh box
seeded from it starts with the current bindings, which is right, since the probes it will read are
the same four.

**The reason is that the configuration and the source now share a file.** `KnurLogger.ini` carries
the DS18B20 bindings as well as the calibration offsets; the bindings can only be made at the car;
and the `tar`-over-ssh loop below overwrites everything under `~/KnurLogger`. With one copy, one
sync from the workstation silently discards a trip to the car — which is exactly the trap this
README used to document and ask you to remember your way around.

```bash
ssh KnurLogger 'bash ~/KnurLogger/SystemSetup/deploy-logger.sh --execute'
```

`deploy-logger.sh` **always replaces the binary and only ever creates the `.ini`, never updates
it.** Run it with no arguments first, like every script in `SystemSetup/`. `KnurLogger.service`
points at `/home/chrum/bin/KnurLogger`, not at the build tree.

> **⚠ The production `.ini` is not in git, so nothing else backs up a calibration or a binding.**
> After enrolling or entering offsets at the car, copy `~/bin/KnurLogger.ini` into the repo as
> `build/KnurLogger.ini` and commit it. That is a deliberate act rather than a side effect, which
> is the point — but it is also the only thing standing between a wiped SD card and another trip:
>
> ```bash
> scp KnurLogger:bin/KnurLogger.ini build/KnurLogger.ini
> ```

**The box is fed from constant 12 V, not the accessory circuit** (as built, 2026-09-10), so the
logger runs the whole day and the fuse is the off switch. Two consequences worth having here:
`SystemSetup/KnurLogger.service` is **required** rather than convenient, because nothing
hand-starts the logger when the fuse goes in; and the hard cut the ~1 s `fsync` protects against
is now the fuse being pulled, or a cranking dip, rather than ignition-off. `CLAUDE.md` has the
rest, including the battery-drain arithmetic.

**Do not put push credentials on the box.** Commit and push from the workstation; the box pulls
only. A logger in a wheel-well cavity is physically exposed — if the car is broken into or the SD
card is pulled, any PAT or writable deploy key on it becomes an attacker's write access to this
repo. A public repo needs no credential to clone or pull, so read-only costs nothing.

For a tight edit-build loop, round-tripping through GitHub is slow; copy the tree to the box
instead and keep GitHub for durable commits. The loop in use is:

```bash
tar czf - --exclude=.git --exclude=build/CMakeCache.txt --exclude=build/CMakeFiles . | ssh KnurLogger 'tar xzf - -C ~/KnurLogger && cd ~/KnurLogger && cmake --build build -j4'
```

**That sync overwrites `~/KnurLogger/build/KnurLogger.ini`, and that is now harmless** — it is the
template, and the running logger does not read it. Nothing under `~/bin/` is touched until
`deploy-logger.sh` is run, and even then only the binary. This is what the dev/production split
above bought; before it, one sync destroyed a hand-entered offset silently.

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

**Stop the service first — it is installed and enabled, so the logger is always already running:**

```bash
ssh -t KnurLogger 'sudo systemctl stop KnurLogger'
```

Two instances both poll the bus and both advertise, and nothing warns. See `CLAUDE.md`.

```bash
ssh KnurLogger '~/bin/KnurLogger --enroll'
```

1. Start with **no probe connected**, then plug them in **one at a time, lowest channel first** —
   the installed order the plan fixes is `temp0` = `T_ambient`, `temp1` = `T_core_in`,
   `temp2` = `T_core_out`, `temp3` = `T_aft`.
2. **Every probe stays plugged in once it is in. Do not unplug one to make room for the next.**
   Binding works either way, which is what makes this easy to get wrong — it was got wrong at the
   car on 2026-09-10 — but unplugging as you go costs three things that binding does not:
   1. **The four-probe star is never loaded**, so the run proves nothing the ESP32 bench rig had
      not already proved with single probes. All four together on the 4 × 5 m bus, enumerating
      with CRC-clean reads, is the open acceptance criterion in plan thermal item 1, and it is
      only met with all four connected at once.
   2. **Step 5's map check becomes impossible**, because warming one probe and watching one
      channel move needs four live channels.
   3. **Every unplug leaves a ~100 s tail** of a channel that is present in sysfs and answering
      with nothing — see `CLAUDE.md` on `w1_slave_ttl`.
3. **Allow ~10 s per probe.** `w1_master_timeout` is 10 s, so that is the hot-plug latency; nothing
   can shorten it without root.
4. Each bind prints `OneWire: BOUND temp0 <- 28-…, reading 21.50 C` and writes an `enrollment`
   record. Check the ROM ID against the lead you just connected. **A bind whose reading comes back
   `INVALID` is a warning, not a failure** — the binding is still written, but the probe answered
   badly on the very read that is meant to confirm it, so re-seat that lead and watch the channel
   before trusting it.
5. **Two unbound probes in one scan are refused, not guessed.** sysfs order is not arrival order,
   so there is no fact available that says which came first. Unplug one and re-seat it alone.
6. **Enrollment keeps sampling after the fourth bind.** With all four bound, warm one probe by hand
   and watch one channel move on the phone — four warmings confirm the whole map in situ and double
   as a liveness test. Stop it with `pkill -TERM -x KnurLogger`.
7. `--enroll --reset` discards every binding and starts from `temp0`. The bindings it throws away
   are written into the session file first, because they are otherwise unrecoverable.

The bindings are written into the `[thermal]` section of the **`KnurLogger.ini` beside the
binary**, alongside the calibration offsets, as `temp<N>RomId` / `temp<N>BoundTaiUs` /
`temp<N>BoundIso` (owner decision, 2026-09-10 — this replaced a separate `channels.ini` in the data
directory; see `CLAUDE.history.md` §2.7 and do not rebuild that file). An empty `temp<N>RomId` is
an unbound channel and a perfectly good state. Three consequences:

1. **Enrollment rewrites a file you hand-maintain**, so it does it line by line: only the twelve
   binding lines are touched and every other byte, comments included, is copied through. It is not
   written through GLib's key-file serialiser, which destroys non-ASCII characters in comments —
   measured 2026-09-10, this file's em-dashes came back as `?`.
2. **The offsets are now in front of you when you change a binding**, which is the point: an offset
   is keyed to the slot, so re-enrolling in a different order strands it. See
   §[Calibration offsets](#calibration-offsets).
3. **This is the file `SystemSetup/deploy-logger.sh` refuses to overwrite.** The production copy is
   `~/bin/KnurLogger.ini` and the git-tracked template is `build/KnurLogger.ini`; see
   §[Repository, deployment and where the code is built](#repository-deployment-and-where-the-code-is-built).

**Editing a ROM ID by hand is allowed and guarded.** Anything that is not a 15-character `28-…` is
refused and logged as an event, and one ROM ID appearing on two channels is refused on the second
— a duplicate would otherwise produce two channels tracking each other perfectly, which is a
mislabelling that looks like agreement.

`Hardware/logger-perfboard-wiring.md` §5a is the human record of the resulting table, and it is
**filled** as of 2026-09-10. **Channel → role is a per-session record, never a channel name** — it
is in every session file's `thermalBaseline`, which is where an analysis should take it from.

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
   them after any re-enrollment.** In exchange they live in the config beside every other setting,
   including the bindings that say which probe each slot holds.
2. **The session file records all three values** — `centiC` (raw), `offsetC` (in force) and
   `sentCentiC` (what went on the air) — so a mistyped offset costs a reprocess, not the session.
3. **All four keys must be present.** A missing one is a startup failure, not a silent zero: an
   offset that quietly stopped being applied would be invisible in the data it corrupts.
4. **Past ±5 °C it warns** about a likely decimal-point slip and applies the value anyway; **past
   ±50 °C it refuses to start.** The offsets in force are printed at startup and recorded in the
   `thermalBaseline` record.
5. **The ROM ID → channel bindings are in this same section**, as `temp<N>RomId`, machine-written
   by `--enroll` (2026-09-10). That is deliberate rather than incidental: item 1's consequence —
   an offset keyed to a slot stops describing the part in it after a re-enrollment — is only
   noticeable if the two are read together. A changed `temp2RomId` is the signal to re-check
   `temp2OffsetC` three lines above it.

**The logger takes no automatic session-start sample and makes no judgement about whether the car
was settled** — see `CLAUDE.history.md` §3.1 for why that was tried, measured failing, and
dropped.

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

**Its one limitation, found the hard way:** a plain file cannot represent a sysfs attribute whose
read value differs from what was written, and `therm_bulk_read` is exactly that — you write
`trigger` and read back `-1`, `0` or `1`. In the fake tree it reads back `trigger`, so that state
machine is unexercised. Two things fill the gap and are worth reaching for before concluding
something is untestable:

1. **A FIFO in place of `w1_slave`** makes a read block for a controlled time, which is how the
   `conversionMs` fallback was verified — a read forced to 385 ms produced `conversionMs 385`
   where the old code produced 0.
2. **`w1_master_add`** attaches a real family-0x28 slave with no hardware present, so `w1_therm`
   binds and its master attributes appear. This is how the udev rule was verified. It needs root,
   `w1_master_remove` undoes it, and the id is parsed as `%02x-%012llx` — the hyphen matters.

## Next

**Find out why the bulk read converts nothing.** The permission is fixed and the write is
accepted, and the cycle is still 3.2 s. Next time probes are attached, read the one-shot
`probeCapabilities` record first — **`ext_power` per probe is the first suspect**, because a bulk
conversion of parasite-powered probes needs a strong pullup this bus does not have. Then read
`bulkState` in the `temp` records: `0` means no device on the bus supports bulk reading at all,
`1` means the kernel claims the results are ready without ever having marked a conversion.
`CLAUDE.history.md` §1.14 has the reasoning. **This costs sample rate and nothing else** — the
readings have always been correct, just slow.

Then, needing only a drive: **commissioning item 5a's installed BLE link check** and **item 5.7's
under-load supply telemetry**, both of which want the enclosure as built and the car moving.
Neither has ever been exercised — the logger has never run on a moving car.

**The thermal side is otherwise finished.** The cold-soak calibration is done and the answer was
no offsets, so **all four `temp<N>OffsetC` staying 0.0 is a result, not an oversight** — do not
"fix" it. The channel → role map is confirmed by warming. Both are closed as plan open items 41
and 42.

**The BME280 reader is DONE** (2026-09-10) and it did not wait for the pressure sensors. It sits
on the main I2C bus at `0x77` behind no mux, so it was independent of the five undelivered
SDP810s. `bme280Sensor.cxx` reads it in forced mode at ×1 oversampling with the IIR filter off —
the datasheet's lowest-self-heating setting, chosen because self-heating in the cavity
thermometer is an error in the quantity it exists to report. Bench-measured over 64 cycles: chip
ID `0x60`, calibration read, ~100.4 kPa / 34.5 °C / 26 %RH, 15-16 ms per cycle. **A five-minute
run gave 300 valid cycles out of 300, zero read errors and nothing exhausted.**
The fixed-point compensation was checked against the datasheet's independent floating-point
reference and agrees to **0.05 Pa, 0.002 °C and 0.005 %RH**.

**Its one hard-won finding is a bus characteristic, not a driver detail — read
§[Getting on the box](#getting-on-the-box)'s neighbour in `CLAUDE.md` before writing any more I2C
code.** The first transfer after an idle bus is refused every single time and a retry 500 µs
later fixes it, which made the first version of this reader fail 100 % of the time while
`i2cdetect` insisted the part was fine. `i2cBus.cxx` retries and **counts** the retries into
every `enclosure` record. **The same bus carries the five SDP810s, so this will apply to them
too, and the physical cause is not established** — `../ndLouvers/` open item 44.

**`0x600` and `0x601` are still bench-verified in the session file and not yet on a phone**, the
same caveat `0x603` carries. The packing was checked byte for byte against the packet buffer, and
it is the primitive `0x604` has already proven on the phone.

**Then the SDP810 readers and the mux**, once the pressure sensors arrive. Expect the retry in
`i2cBus.cxx` to matter for them, and expect a mux channel switch plus a sensor read to be two
transfers that must not be interleaved with anything else on the bus.

`pi-headless-setup.md` §Work Progress is the authority on host state. `CLAUDE.md` carries the four
architecture requirements the plan imposes — BLE as the primary data path with the SD card as the
durable raw/diagnostic record, an append-only file with a ~1 s `fsync` cadence, DS18B20 channel
enrollment, and supply-health telemetry — plus the hardware traps. Read it before writing code.
`CLAUDE.history.md` is its audit trail: resolved faults with the diagnostics that found them,
reversed decisions, and the retirement notes that exist to stop the next agent rebuilding what was
deliberately removed.

**Build order was BLE first** (owner, 2026-09-09), and that is now spent — all five workers exist.
The reasoning still matters for the trip to the car: BLE is the primary data path *and* the only
feedback channel there, because the probes are on a car parked in an underground garage with no
network, so a phone watching `temp0`–`temp3` move is the only way to see what enrollment did
without carrying the box home first.

**What is testable on the box today:** the build itself, config loading, the session-file writer
and its fsync cadence, the supply-telemetry worker (`vcgencmd` and the `rpi_volt` hwmon both
answer), a BLE advertiser against a phone, the **BME280 at `0x77`** since the sensor zone was
assembled, and — via the fake-sysfs harness above — **every branch of the 1-Wire worker except a
real reading**. **What is not:** anything requiring a real SDP810 (not delivered) or a real DS18B20
(the probes are on the car). **The mux answers at `0x70`** since the `~RESET` resolder, so it is
no longer on this list.
**Those unknowns were answered at the car on 2026-09-10, and one answer was "still no".** The loaded 4 × 5 m star
enumerates and reads CRC-clean with all four probes on it — 63 consecutive cycles, valid-mask 15,
zero read errors. A per-probe read costs **799–832 ms**, so a four-probe cycle is **3198–3281 ms**.
`therm_bulk_read` **does** appear the moment a `w1_therm` slave attaches; the write to it was
refused with `EACCES`, that is fixed, and **it still converts nothing** — 331 cycles at
3190–3309 ms with the rule applied. **What the star result does NOT cover** is a session:
3.4 minutes, stationary, cold, in a garage. Read `0x603` bytes 2–3 on the first drive.
