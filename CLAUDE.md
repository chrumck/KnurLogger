# KnurLogger — working instructions

Read [software-contract.md](software-contract.md) before changing code, then the subsystem
document below. [README.md](README.md) is the human task map.

## Document ownership

1. [software-contract.md](software-contract.md): architecture, durable logging and implementation invariants.
2. [hardware-interface.md](hardware-interface.md): I2C diagnostics and sensor acquisition rules.
3. [one-wire-probes.md](one-wire-probes.md): thermal bus, enrollment, offsets and accepted limitations.
4. [ble-protocol.md](ble-protocol.md): subscriptions, GLib contexts, diagnostics and recording audits.
5. [racechrono-channels.md](racechrono-channels.md): packet bytes, equations and phone slot map.
6. [session-records.md](session-records.md): counters, validity, clocks and session alignment.
7. [operations.md](operations.md): build, production configuration, deployment and operation.
8. [SystemSetup/pi-headless-setup.md](SystemSetup/pi-headless-setup.md): host setup and host progress.
9. [SystemSetup/logger-perfboard-wiring.md](SystemSetup/logger-perfboard-wiring.md): connections,
   addresses and sensor allocation. Its §3a.4 net list is authoritative over its diagram views.
10. [3DPrinting/calibrationBell.md](3DPrinting/calibrationBell.md): fixture geometry and as-built
    constants. The as-built record governs the existing bell; the nominal drawing is a different build.
10a. [3DPrinting/surfaceStaticButton.md](3DPrinting/surfaceStaticButton.md): the hood tap's
    geometry, build and brass line resistance.
11. [pressure-correction.md](pressure-correction.md): the pressure-correction decisions and implementation plan.
12. [ndLouvers instrumentation specification](../ndLouvers/instrumentation-spec.md): measurement
    requirements and acceptance. This repository owns no measurement decision.

## Working rules

These add to the global instructions (numbered lists, naming, comments, the history split and
the sync pass); they do not restate them.

1. Name channels positionally: `P0`–`P5` and `temp0`–`temp3`. Never rename a channel for a test
   role. The pitot probes are `T1`/`T2`; pressure role allocation belongs to the measurement project.
2. Update the owning document when behavior changes. Keep numbers and measured status in their
   owners; do not grow this router or README into another specification or progress report.
3. The history trail is [CLAUDE.history.md](CLAUDE.history.md). Do not rebuild the router's old
   incident catalogue or the build sheet's bring-up narrative — see its 2026-09-24 entries.
4. The sync pass includes ndLouvers whenever a shared requirement changes, and runs
   `python ../ndLouvers/tools/check-markdown-links.py`, which checks both repositories and must
   report 0 findings.
5. Both repositories are public. Keep credentials, keys and hotspot secrets out. Relative links
   between sibling repositories work locally and may break on GitHub; do not copy requirements
   across to fix navigation.

## Before operating hardware

1. Never probe mux channel 5 or sweep unconfigured channels. Read the hardware interface first.
2. Never disable Bluetooth or use `rfkill block all`. Wi-Fi gating is manual for diagnosis.
3. Stop `KnurLogger.service` before running another logger instance. Follow operations for
   production config changes; deployment does not merge new required keys.
4. Do not reinstate retired electrical qualification, GPS-time fetching, session-start cold-soak
   sampling, a separate bindings file or the 85 °C detector. The specification, software contract
   and 1-Wire document retain the applicable guards and history pointers.
