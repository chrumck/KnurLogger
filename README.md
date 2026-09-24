# KnurLogger

Headless Raspberry Pi logger for the ND Miata instrumentation. It publishes sensor channels to
RaceChrono over BLE and retains raw readings and diagnostics in append-only local session files.

## Start here

| Task | Read |
|---|---|
| Work on the code | [Working instructions](CLAUDE.md), then [software contract](software-contract.md) |
| Build, deploy, enroll probes or retrieve logs | [Operations](operations.md) |
| Configure the phone or decode a packet | [Channel reference](racechrono-channels.md) |
| Diagnose BLE or inspect a recording | [BLE protocol](ble-protocol.md) |
| Diagnose I2C or change sensor acquisition | [Hardware interfaces](hardware-interface.md) |
| Change thermal sampling | [1-Wire probes](one-wire-probes.md) |
| Interpret counters and clocks | [Session records](session-records.md) |
| Implement pressure correction | [Correction requirements](pressure-correction.md) |
| Wire the board | [Perfboard build sheet](SystemSetup/logger-perfboard-wiring.md) |
| Configure the host or inspect setup progress | [Host runbook](SystemSetup/pi-headless-setup.md) |
| Restore the phone profile | [RaceChrono profile](RaceChrono/README.md) |
| Understand a retired feature or resolved fault | [History](CLAUDE.history.md) |

Measurement requirements, qualification and the project backlog belong to
[ndLouvers](../ndLouvers/CFD-Learning-Plan.md). Measured results belong to its
[thermal](../ndLouvers/thermals-testing.md) and [pressure](../ndLouvers/pressure-testing.md)
companions. Logger operation is not pressure-channel qualification.

## Hardware it drives

| Part | Interface | Channels |
|---|---|---|
| 5 × Sensirion SDP810 (4 × ±500 Pa on `P0`/`P1`/`P3`/`P4`, 1 × ±125 Pa on **`P2`**) | I2C `0x25` behind a PCA9548A mux at `0x70` | `P0`–`P5` |
| 4 × DS18B20 | 1-Wire on GPIO4, addressed by 64-bit ROM ID | `temp0`–`temp3` |
| BME280 | I2C `0x77` on the main bus | enclosure pressure, cavity temperature, humidity |

**Channel names are positional and carry no meaning.** `P0`–`P5` are fixed by mux position;
`temp0`–`temp3` are fixed by ROM ID at enrollment. The mapping from these to measurement roles
(`T_ambient`, `T_core_in`, `U`, `X`, `C` …) must be recorded with the session metadata. Do not
rename a channel after a role. **Pressure is still deliberately undecided. Thermal is decided
and applied** — installing the probes on the car pinned it, and enrolled in installed order it
is temp0=`T_ambient`, temp1=`T_core_in`, temp2=`T_core_out`, temp3=`T_aft`. **All four are bound**
(2026-09-10); `SystemSetup/logger-perfboard-wiring.md` §5a has the ROM IDs. **The role map is
independently confirmed** — warming each probe in installed order moved `temp0`–`temp3` in that
order, +3.8 to +5.4 K each.

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
oneWireProbes.cxx     DS18B20 enrollment, the bindings in the .ini, 0x602 and 0x603
i2cBus.cxx            I2C_RDWR transport with the mandatory retry — the mux shares it
bme280Sensor.cxx      BME280 forced-mode reads, compensation, 0x600 and 0x601
pressureSensors.cxx   PCA9548A channel select, five SDP810s, 0x605, 0x606 and 0x607
raceChronoBle.cxx     bluez_inc: adapter, advertisement, GATT, notify timers
bluez_inc/            submodule, github.com/weliem/bluez_inc

build/KnurLogger.ini  config TEMPLATE; the deployed copy is ~/bin/KnurLogger.ini on the box

one-wire-probes.md    the 1-Wire subsystem's traps and standing requirements — read before
                      touching oneWireProbes.cxx

pressure-worker-plan.md  the implementation plan for the sixth worker: the five SDP810s into
                      the logged and broadcast data. Numbered steps, four owner decisions at
                      step 1, and a Work Progress table to update as it is executed

Tools/                offline diagnostics; nothing here runs on the box
  rcz-channels.py       decode a RaceChrono .rcz's channel slots and flag a mistyped
                        equation — the phone's channel list, audited without the phone
  bell-marks.py         evaluate calibration-bell marks against one pressure channel of a
                        session, on the as-built bell constants and the barometric factor
  ladder-fit.py         re-derive every ladder mark in force and fit each sensor's span
                        term: constant, sign split, slope, per-rung offsets, validation

3DPrinting/           every printed part for both boxes — enclosure, sensor holders, boom
                      tip, and the calibration bell (calibrationBell.md owns its constants)

bellTesting.xlsx      the bell-session spreadsheet

RaceChrono/           the phone's configuration, which lives nowhere else
  vehicleProfile.json   RaceChrono's exported vehicle profile, localUuid stripped;
                        both boxes' channels, since the two share one channel set
  README.md             why it is here, how to re-import it, and its traps

SystemSetup/          host configuration; nothing here is logger code
  pi-headless-setup.md    the runbook — start here
  logger-perfboard-wiring.md  the perfboard build sheet — §3a's net list is the authority
                          on every connection. Subordinate to the plan's Step 0b.
  audit-boot.sh           read-only survey of what the box runs at boot
  install-dependencies.sh packages the build needs
  harden-headless.sh      boot-time service reduction and bus configuration
  ssh-harden.sh           key-only SSH
  deploy-logger.sh        installs the production binary into ~/bin, never its .ini
  grant-w1-bulk-read.sh   udev rule for therm_bulk_read — one of the two conditions for 1 Hz
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


## Next

Use [ndLouvers Work Progress](../ndLouvers/work-progress.md) for project work,
[pressure-correction.md](pressure-correction.md) for the pending logger correction, and
the [host runbook](SystemSetup/pi-headless-setup.md#work-progress) for host-specific open items.
The [acquisition plan](pressure-worker-plan.md) retains its pending recording audit and links
to the completed implementation record.

## Licence

MIT — see [LICENSE](LICENSE), which also states what it does not cover and the absence of any
warranty of fitness for vehicle use. **`bluez_inc` is a submodule, not a copy**, so its own MIT
licence travels with it rather than being restated here. The sibling
[ndLouvers](https://github.com/chrumck/ndLouvers) repository carries the same licence.
