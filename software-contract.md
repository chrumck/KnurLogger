# Logger software contract

This file owns implementation invariants. Measurement acceptance is in
[ndLouvers instrumentation requirements](../ndLouvers/instrumentation-spec.md).
Read [hardware-interface.md](hardware-interface.md) for I2C, [one-wire-probes.md](one-wire-probes.md)
for thermal sampling, [ble-protocol.md](ble-protocol.md) for BLE, and
[session-records.md](session-records.md) for interpreting local records.

## Architecture

Follow `iSitePiLogger`, which is the structural model:

1. **Single translation unit.** All `.cxx` files are `#include`-d into `main.cxx`. Do not add them
  to `CMakeLists.txt` as independent targets.
1. **`initialiseBlePackets()` is called exactly once, from `main`, before any worker starts.**
  Initialise shared packet state in `main`, never in a worker — `CLAUDE.history.md` §1.5 is the latent bug that
  made this a rule.
1. **Procedural workers, no OOP.** Plain `gpointer fn(gpointer)` passed to `g_thread_new()`.
1. **`CLOCK_TAI` throughout**, to avoid leap-second discontinuities in sample timestamps and file
  names.
1. **The `.ini` sits beside the binary** and its path is resolved from `/proc/self/exe`, not the
  working directory.

Four requirements came from the plan rather than from iSitePiLogger. Two of them — DS18B20 channel
enrollment and the calibration offsets — moved to [`one-wire-probes.md`](one-wire-probes.md) with
the rest of that subsystem, along with a fifth that was implemented and then **retired whole**.

1. **BLE is the primary data path; the SD card is the durable raw and diagnostic record**
  (owner decision, 2026-09-09, item 5b — this **reverses** the earlier "SD primary, BLE
  secondary", so do not reinstate that from memory or from git history; `CLAUDE.history.md` §2.1). The analysis
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
1. **Append-only session file, `fsync` on a fixed ~1 s cadence** — not per sample, not only at
  close. The supply vanishes without warning — the fuse pulled at the end of the day, or a
  cranking dip — so the last durable write bounds the loss. Flushing per sample at 10 Hz buys a
  shorter window at the price of write amplification without changing the failure mode.
1. **DS18B20 channel enrollment and the calibration offsets** — both in
  [`one-wire-probes.md`](one-wire-probes.md) §"Requirements this path carries from the plan",
  with the retired session-start thermal sample recorded there too.
1. **Supply health is logged telemetry, read after a run** (owner decision, 2026-09-09), standing
  in for a bench instrument on commissioning item 5.7. It does not replace build sheet §10 step 2's
  meter, which is retired — do not rebuild it ([ndLouvers risk 14](../ndLouvers/risk-register.md)).
  What to record, and the traps:
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

## Configuration and startup constraints

1. Preserve unrelated configuration bytes when saving bindings. Do not use
   `g_key_file_to_data()` to write the file: it damaged non-ASCII comments. Keep GKeyFile for
   reading and the line-preserving binding writer for changes; see `CLAUDE.history.md` §2.7.
2. Do not depend on `network-online.target`; host setup masks NetworkManager's wait-online
   service. The logger must start without a network, using its own readiness checks.
3. Pressure acquisition warns on an unknown product or unexpected scale factor and retains the
   returned identity; it must not silently substitute a known part. Reject an invalid zero scale
   factor before conversion. The configured channel list forbids unpopulated channel 5.
4. Automatic mux reset on exhausted transfers is not an approved recovery policy. The manual
   diagnostic reset in hardware-interface.md does not authorize adding it to the worker.
