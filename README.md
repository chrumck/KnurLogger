# KnurLogger

Headless data logger for the ND Miata hood-louver instrumentation ("box 2"), running on a
Raspberry Pi 4B in the cavity behind the right wheel well.

It logs differential pressure, temperature and enclosure conditions to the SD card, and publishes
the same channels over Bluetooth LE as a [RaceChrono DIY BLE
device](https://github.com/aollin/racechrono-ble-diy-device) for live viewing on a phone.

**Status: repository created 2026-09-09. There is no logger binary yet.** What exists is the host
setup under `SystemSetup/`, and even that has not been applied to the box.

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
| 5 × Sensirion SDP810 (4 × ±500 Pa, 1 × ±125 Pa) | I2C `0x25` behind a TCA9548A mux at `0x70` | `P0`–`P5` |
| 4 × DS18B20 | 1-Wire on GPIO4, addressed by 64-bit ROM ID | `temp0`–`temp3` |
| BME280 | I2C `0x76` on the main bus | enclosure pressure, humidity |

**Channel names are positional and carry no meaning.** `P0`–`P5` are fixed by mux position;
`temp0`–`temp3` are fixed by ROM ID at build time. The mapping from these to measurement roles
(`T_ambient`, `T_core_in`, `U`, `X`, `C` …) is a per-session record, deliberately not yet decided,
and is logged at boot. Do not rename a channel after a role.

All five SDP810s answer at the same fixed I2C address and cannot be strapped apart, which is why
the mux is mandatory rather than a convenience.

## Layout

```
SystemSetup/          host configuration; nothing here is logger code
  pi-headless-setup.md    the runbook — start here
  audit-boot.sh           read-only survey of what the box runs at boot
  install-dependencies.sh packages the build needs
  harden-headless.sh      boot-time service reduction and bus configuration
  ssh-harden.sh           key-only SSH
```

## Relationship to the other two loggers

| | KnurDash | iSitePiLogger | KnurLogger |
|---|---|---|---|
| Display | 4.3" DSI touchscreen, GTK under `startx` | none | none |
| Primary record | RaceChrono on the phone | files uploaded over HTTP | **SD card**, BLE is secondary |
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

## Getting on the box

SSH alias `KnurLogger` (`192.168.118.52`, user `chrum`), key-only.

```bash
ssh KnurLogger
```

## Next

Nothing in `SystemSetup/` has been run. `pi-headless-setup.md` §Work Progress is the authority on
where that stands. The logger architecture beyond "iSitePiLogger's shape" is not yet decided.
