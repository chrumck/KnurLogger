# Implementation plan — the pressure worker

**Scope: add the five SDP810s to the logged and broadcast data.** This plan covers the logger
software only — a sixth worker, its config, its session records and its RaceChrono packets. It does
**not** cover the pneumatic rig (wands, tubing, filters, drainage), which is
`../ndLouvers/pressure-testing.md` §1, nor the installed qualification, which is its §2. **Nothing
in this plan produces a pressure measurement**; it produces the channel that a measurement will one
day travel down.

## How to use this file

**Update the `Work Progress` table at the end after every executed step**, so that re-reading this
file reveals where the work stopped and what comes next. Each step below is written to be
executable by someone who has read this file and nothing else: it names the files, the commands and
the acceptance criterion it needs. Where a step depends on an owner decision, that decision is
named in step 1 and the step says which one.

**Read these before starting**, in this order — they own things this plan is subordinate to:

1. `CLAUDE.md` — the box's hardware traps. The I2C retry, the mux-channel prohibition, and the
   rule that a faulty channel hangs the whole main bus are all load-bearing here.
2. `Hardware/logger-perfboard-wiring.md` §2 and §5 — the address map, the mux channel allocation
   and the five serials as fitted.
3. `../ndLouvers/CFD-Learning-Plan.md` Step 0b — **the authority on requirements**, especially
   commissioning item 2 (identity and per-session channel record) and item 4 (what a pressure
   sample must carry). This repository owns no measurement decision.
4. `../ndLouvers/pressure-testing.md` §3.1 — the bring-up this plan builds on, and the three
   cautions attached to it.
5. `README.md` §"RaceChrono channels" — the wire format rules and the existing slot map.

## What is already established, and must not be re-derived

All five sensors were brought up by hand on 2026-09-19 and the protocol is settled:

| Fact | Value |
|---|---|
| Address | `0x25`, all five, behind the PCA9548A at `0x70` |
| Populated mux channels | **0, 1, 2, 3, 4** — channel 5 is empty and **must never be addressed** |
| `P2` is the ±125 Pa | product `0x03020B01`, scale **240 counts/Pa** |
| `P0`/`P1`/`P3`/`P4` are ±500 Pa | product `0x03020A01`, scale **60 counts/Pa** |
| Serials | build sheet §5 — read them back, do not hard-code them |
| Stop continuous | `0x3FF9` |
| Identity | `0x367C` then `0xE102`, then an 18-byte read |
| Start continuous | `0x3615` — differential pressure, temperature compensated, averaged |
| Sample | select mux channel, then a 9-byte read |
| Frame layout | `dp` int16 + CRC, `temp` int16 + CRC, `scale` uint16 + CRC |
| Conversions | `pressure = dp / scale` Pa; `temperature = temp / 200` °C |
| CRC | CRC-8, polynomial `0x31`, init `0xFF`, over each 2-byte word |
| Measured cycle cost | **15.4 ms** for all five, worst 18.3 ms |
| Measured zero, open ports | within ±0.06 Pa, sd ≤0.011 Pa |

Four behaviours that will bite an implementation that does not expect them:

1. **`0x3615` is NAK'd if the sensor is already in continuous mode**, and selecting a different mux
   channel does **not** take it out of that mode. Start it once per sensor and track per-sensor
   state. A harness that re-armed it every cycle lost **145 of its 150 start-continuous commands**
   — 30 cycles × 5 sensors — and looked exactly like a degraded bus. (Owner, 2026-09-19: the
   denominator is the commands. An earlier "145 of 456" here counted the run's transfers instead
   and read as the same statistic.)
2. **Addressing an unpopulated or faulty mux channel hangs the entire main bus** — mux and BME280
   included — and only a `~RESET` pulse on GPIO17 recovers it. Both bus lines read idle-high and
   `i2cdetect` still lists every device while this is happening.
3. **The first transfer after an idle bus is refused on some boots** and not others — bimodal per
   boot, `../ndLouvers/` open item 44. `transferI2c` already retries ten times and counts;
   **do not add a second retry layer.**
4. **Scale factor is per sensor and is returned in every frame.** Never hard-code 60.

---

## Step 1 — Get the four owner decisions this plan cannot make

**This repository owns no measurement decision** (`CLAUDE.md`, "Where authority lives"). Four
choices below change what the data means, so they belong to `../ndLouvers/CFD-Learning-Plan.md`
Step 0b. Put them to the owner, record the answers in Step 0b, and only then write code. Each has a
recommendation; a recommendation is not a decision.

1. **Wire scaling for the BLE channels. Recommended: signed 0.1 Pa/LSB (decipascals) for all six
   channels, sentinel `INT16_MIN` = −3276.8 Pa.**
   The argument: `int16` at 0.01 Pa/LSB overflows at 327 Pa, so it cannot carry a ±500 Pa channel;
   0.1 Pa/LSB spans ±3276.7 Pa and covers both ranges with **one decode rule for every pressure
   channel**, which is the property that matters — `README.md` records that a *second* decode rule
   is exactly how the `bytesToUint`/`bytesToInt` fault survived for days. The cost is resolution on
   the ±125 Pa part, whose 1/240 Pa granularity is thrown away. **That cost is acceptable because
   the SD file carries raw counts and the scale factor at full resolution**, and BLE is the
   analysis path for *pressure differences of 45–90 Pa*, where 0.1 Pa is 0.1–0.2 %.
   The alternative — centipascals for `P2` alone — buys resolution nothing currently needs and
   introduces the exact class of fault this project has already paid for once.
2. **Sample rate. Recommended: make it configurable and run at 10 Hz, with the session-file volume
   accepted (step 9 measures it).**
   Step 0b item 4 targets 10 Hz pressure logging. A five-sensor cycle costs 15.4 ms, so 10 Hz fits
   a 100 ms budget with room. The open question is not CPU, it is **bytes**: step 9 measures the
   real rate and the owner accepts or reduces it there rather than guessing here.
3. **Whether the pressure worker's BLE packets are published when no role mapping exists.**
   Recommended: **yes**. The channels are positional (`P0`–`P5`) and carry no role, exactly as
   `temp0`–`temp3` do; publishing them lets the phone-side definitions be built and verified before
   the rig exists, which is the only way to avoid `../ndLouvers/` open item 47 repeating on the
   pressure side. **A channel with no definition on the phone is never sent at all.**
4. **Packet IDs. Recommended `0x605`, `0x606`, `0x607`** (step 3 says what each carries). `0x600`–
   `0x604` are taken. Confirm no other DIY device on this phone claims them; box 1's IDs are
   `0x7F0`, `0x78`, `0x86`, `0x202`, `0x420`, `0x4FA`, so there is no collision today.

**Acceptance:** all four recorded in Step 0b, and the role→channel mapping explicitly left open
(`../ndLouvers/` open item 30a) — recording which *part* is on which channel is not deciding which
*role* it serves, and this plan must not invent one.

---

## Step 2 — Add the data contracts

**File:** `dataContracts.hpp`. No behaviour, no other file changes. Build after it and confirm the
binary is unchanged in behaviour.

1. **Constants**, beside the existing BME280 block and following its commenting style — explain
   *why* a value is what it is, never what the line does:
   - `PACKET_ID_PRESSURE_A 0x605`, `PACKET_ID_PRESSURE_B 0x606`,
     `PACKET_ID_PRESSURE_STATUS 0x607` (per step 1 decision 4).
   - `SDP810_ADDRESS 0x25`, and the commands `SDP810_CMD_START_CONTINUOUS 0x3615`,
     `SDP810_CMD_STOP_CONTINUOUS 0x3FF9`, `SDP810_CMD_READ_PRODUCT_ID_1 0x367C`,
     `SDP810_CMD_READ_PRODUCT_ID_2 0xE102`.
   - `SDP810_PRODUCT_500PA 0x03020A01`, `SDP810_PRODUCT_125PA 0x03020B01`,
     `SDP810_SCALE_500PA 60`, `SDP810_SCALE_125PA 240` — **expected values for a warning, never
     substitutes for the returned scale factor.**
   - `SDP810_MEASUREMENT_LENGTH 9`, `SDP810_IDENTITY_LENGTH 18`, `SDP810_CRC_POLYNOMIAL 0x31`,
     `SDP810_CRC_INIT 0xFF`, `SDP810_TEMPERATURE_DIVISOR 200`.
   - `SDP810_START_SETTLE_US` — the datasheet's first-measurement delay after `0x3615`; 20 ms is
     ample and was used in bring-up.
   - `PRESSURE_DECI_PA_INVALID INT16_MIN`, and validity bounds per range. **A reading outside the
     part's range is invalid data, not a clipped value** — commissioning item 4.
   - `MUX_CHANNEL_NONE 0x00` and `MUX_MAX_CHANNEL 5`.
   - A comment block on the mux prohibition, pointing at `CLAUDE.md` rather than restating it.
2. **Config keys**: a `CONFIG_GROUP_PRESSURE "pressure"` with `pressureIntervalMs`, and
   `pressureChannelsEnabled` — **a list of populated mux channels, not a count**, because step 4's
   loop iterates that list and a count would imply channels 0..n-1 are all safe.
3. **Structs**, following `Bme280Reading`/`Bme280Data`'s split between one transient conversion and
   the worker's persistent state:
   - `SdpReading` — `isPresent`, `isValid`, `rawDifferential`, `scaleFactor`, `rawTemperature`,
     `pressurePa` (double, full resolution), `pressureDeciPa` (gint16, what goes on the air),
     `temperatureCentiC`, `crcOk`, `invalidReason`, `isReadError`, `readMs`.
   - `PressureChannel` — `muxChannel`, `isEnabled`, `isPresent`, `productNumber`, `serial`,
     `expectedScale`, `isContinuousStarted`, `readErrors`, `lastReading`.
   - `PressureData` — `channels[PRESSURE_CHANNEL_COUNT]`, `fd`, `sampleCycles`, `readErrors`,
     `lastCycleMs`, `enabledCount`, the rate-limiter booleans the other workers carry, and
     `std::atomic<bool> isRunning`.
   `PRESSURE_CHANNEL_COUNT` is already defined as 6 — keep it, and let `isEnabled` carry which of
   the six are real.

**Acceptance:** `cmake --build build -j4` succeeds and the logger runs unchanged.

---

## Step 3 — Fix the wire format on paper before writing the packer

**File:** `README.md`, the "RaceChrono channels" section. **Write the byte tables and the slot map
first**, then implement against them. This ordering is deliberate: `README.md` records that the
phone's channel list is part of the instrument and that nothing in this code can check it, so the
specification has to exist before there is an implementation to disagree with.

Proposed layout, subject to step 1:

| Packet | Bytes | Carries |
|---|---|---|
| `0x605` | 0–1, 2–3, 4–5, 6–7 | `P0`, `P1`, `P2`, `P3` — signed decipascals |
| `0x606` | 0–1, 2–3 | `P4`, `P5` — signed decipascals |
| `0x606` | 4–5 | sample cycles, free-running, wraps at 65535 |
| `0x606` | 6–7 | last cycle duration, ms |
| `0x607` | 0 | channels enabled, bitmask |
| `0x607` | 1 | valid-this-cycle bitmask, bit *n* = `P<n>` |
| `0x607` | 2–3 | cumulative read errors, saturating |
| `0x607` | 4–5 | cumulative CRC failures, saturating |
| `0x607` | 6 | sensor temperature of the lowest enabled channel, °C, signed |
| `0x607` | 7 | mux channel currently selected — a cheap liveness tell |

Three properties this layout is chosen for, each mirroring something already proven on `0x603`:

1. **A free-running counter that advances whatever the sensors report** (`0x606` bytes 4–5), so a
   dead worker is distinguishable from five steady pressures. Five zeroes is a *plausible* reading
   at rest, which makes this more necessary here than on the thermal channels.
2. **A bitmask of what read cleanly this cycle** (`0x607` byte 1), so an invalid channel is visible
   from the phone without decoding the sentinel.
3. **Separate error counters for transport and CRC** (`0x607` bytes 2–3 and 4–5). The thermal side
   learned this the expensive way: one counter conflating "the bus retried" with "a sample was not
   trusted" is what made the 85 °C misclassification invisible for two track days.

**Every payload field is big-endian; only the 4-byte packet ID is little-endian.** Pick slots from
the free ranges — `Pressure Front 1`–`6` for the six channels, and Digital slots outside 1–5,
11–16 and 51–55 — and add them to the slot-map table with the equations as they will be typed.

**Acceptance:** the tables are in `README.md` and a reader could type the phone's channel list from
them without reading any code.

---

## Step 4 — Write the mux and SDP810 transport

**New file:** `pressureSensors.cxx`, included in `main.cxx` **after `i2cBus.cxx` and before
`raceChronoBle.cxx`** — the same position `bme280Sensor.cxx` holds, because a producer must come
after the packet primitives and before the BLE worker. Add it to no CMake target; this is a single
translation unit.

1. **`selectMuxChannel(gint fd, gint channel)`** — writes `1 << channel` to the mux, then **reads
   the control register back and verifies it**. The read-back is not belt-and-braces: a write that
   appears to succeed onto a faulty segment is precisely the failure that hangs the bus, and the
   read-back is where it is caught.
   **It must refuse any channel not in the configured enabled list**, and log that refusal as an
   event. This is the code half of the prohibition in `CLAUDE.md`; a sweep must be impossible to
   write by accident.
2. **`deselectMux(gint fd)`** — writes `MUX_CHANNEL_NONE`. Called at the end of every cycle and on
   shutdown, so the bus is never left with a channel bridged onto it.
3. **`sdpCrc8(const guint8* data, guint length)`** — polynomial `0x31`, init `0xFF`.
4. **`sendSdpCommand(gint fd, guint16 command)`** and
   **`readSdpFrame(gint fd, guint8* out, guint length)`** — both straight through `transferI2c`,
   which already carries the retry. **Add no retry here.**
5. **`parseSdpMeasurement(const guint8* frame, SdpReading* out)`** — checks all three CRCs,
   converts, and sets `invalidReason` to one of `crc`, `outOfRange`, `notPresent`, `readFailed`,
   `notStarted`. **A CRC failure is invalid data, never a carried-forward value.**

**Acceptance:** compiles; no worker calls it yet.

---

## Step 5 — Identity at boot, and the baseline record

**File:** `pressureSensors.cxx`. Commissioning item 2 requires product, revision, serial and CRC
read at boot and logged with the channel mapping.

1. **`initialisePressureSensors()`** — opens the bus (reuse `openI2cBus`; the worker may hold its
   own fd as the BME280 worker does), then for each **enabled** channel: select, stop-continuous
   (tolerating a NAK, since it may already be idle), read identity, verify all six CRCs, record
   product number and serial, warn if the product number is not one of the two known values, and
   warn if the returned scale factor differs from the expected one for that product.
2. **`writePressureBaseline()`** — modelled on `writeBme280Baseline()`. It must carry: the I2C bus
   and mux address, the enabled channel list, and per channel the mux position, product number,
   serial, scale factor and range. **This record is the per-session channel→part provenance that
   commissioning item 2 asks for**, and it is the only place a later analysis can learn which
   physical part produced a channel.
   Include a `roleMappingNote` field stating that the role→channel mapping is deliberately not
   recorded here because it is not decided — the same shape as `thermalBaseline`'s note.
3. **Warn, do not refuse, on a mismatch.** A logger that will not start in the car because one
   sensor reports an unexpected product number loses every other channel. The existing
   `TEMP_OFFSET_MAX_C` comment states this principle; follow it.

**Acceptance:** run `~/bin/KnurLogger` on the bench with the service stopped; the session file's
first records include a `pressureBaseline` carrying all five serials matching build sheet §5.

---

## Step 6 — The sampling loop

**File:** `pressureSensors.cxx`, function `pressureSensorsLoop(gpointer)`, following
`bme280SensorLoop`'s shape exactly.

1. **Start continuous measurement once per sensor**, after identity, recording
   `isContinuousStarted`. Wait `SDP810_START_SETTLE_US` before the first read.
2. **Each cycle**: for each enabled channel, select, read 9 bytes, parse, store. Then deselect.
   Record `lastCycleMs` from before the first select to after the deselect.
3. **Recovery, and this is the subtle part.** If a channel's read fails, mark it not present and
   **clear `isContinuousStarted`**; on the next cycle, re-issue `0x3615` before reading it again. A
   sensor that browned out has forgotten it was in continuous mode, and one that did not has not —
   and re-arming one that did not is a NAK, which is harmless as long as the code expects it.
   **Do not re-arm unconditionally**: that is the 145-of-456 failure from bring-up.
4. **Preserve invalid data as invalid.** Every channel that did not read cleanly sends
   `PRESSURE_DECI_PA_INVALID` and is recorded with its `invalidReason`. No carried-forward values.
5. **Publish the three BLE packets** through `updateBlePacket`, the worker that owns the reading
   doing its own packing — `blePackets.cxx` says why.
6. **Write one `pressure` session record per cycle**, carrying per channel: raw differential counts,
   scale factor, computed pascals at full resolution, the decipascals actually sent, sensor
   temperature, CRC result, validity and reason. Step 0b item 4 asks for exactly this list, and the
   raw-plus-scale pair is what makes a reprocess possible if the scaling decision in step 1 is ever
   revisited.
7. **Shutdown**: stop continuous on each sensor, deselect the mux, close the fd. Add the worker to
   the `producersRunning` count and the joins in `main.cxx`.

**Acceptance:** a 10-minute bench run with zero exhausted transfers, the valid mask reading all
enabled channels on every cycle, and `0x606`'s counter advancing by exactly 1 per cycle.

---

## Step 7 — Initialise the packets with sentinels

**File:** `blePackets.cxx`, in `initialiseBlePackets()`, and `getAllPackets()` extended to return
the three new packets so `forEachBlePacket` covers them.

**Publish all six pressure channels as `INT16_MIN` before the worker has read anything.** The
existing comment on the thermal channels gives the reason and it applies with more force here:
**zero is a completely plausible differential pressure**, so a phone that connects before the first
cycle would otherwise see six believable readings of nothing. This is the one place where the
pressure channels are more dangerous than the thermal ones.

**Acceptance:** with the sensors physically unplugged, all six channels decode as the sentinel on
the phone and `0x607` byte 1 reads 0. **The number is −3276.8, not the −327.68 written here before
the wire format was settled** — the wire carries decipascals and the phone divides by 10, so the
readout is in pascals. Still negative, still impossible; only the digits differ.

---

## Step 8 — Config, and the enabled-channel list

**Files:** `dataContracts.hpp` (done in step 2), `config.cxx`, `build/KnurLogger.ini`.

1. Parse `pressureIntervalMs` with a sane range (50–60000) and `pressureChannelsEnabled` as a
   comma-separated list, validating that every entry is 0–5 and that **channel 5 is rejected with a
   clear message** until its pull-ups are fitted.
2. Add the `[pressure]` section to `build/KnurLogger.ini` **with comments explaining why the list is
   a list**, in the style of the existing `[thermal]` block. That file is the template and its
   comment block is documentation.
3. **After deploying, copy the production `.ini` back**:
   `scp KnurLogger:bin/KnurLogger.ini build/KnurLogger.ini` and commit. `deploy-logger.sh` only ever
   *creates* the `.ini`, never updates it, so **a new config key will not appear in the production
   copy by itself** — it must be added there by hand or the logger will fail to start on a missing
   key. This is the single most likely way this plan breaks in the car.

**Acceptance:** the logger refuses to start with a missing or out-of-range key, and starts cleanly
with `pressureChannelsEnabled=0,1,2,3,4`.

---

## Step 9 — Measure the cost, and decide the rate

**Runs on the box.** This is where step 1 decision 2 is actually settled.

1. Run a **one-hour bench session at 10 Hz** with all five sensors and the service stopped.
2. Measure: bytes per second against the 1457 B/s the 1 Hz baseline produced, worst-case inter-cycle
   gap, `i2cExhausted`, and SoC temperature. Compare the projected daily volume against the 108 GB
   free.
3. **Check the fsync cadence still holds.** The ~1 s `fsync` is a requirement, not a tuning
   parameter, and a 10 Hz producer writing ten times the records is the first thing that could
   starve it.
4. Report the numbers to the owner and record the accepted rate in Step 0b item 4.

**Acceptance:** a measured figure for bytes/day at the chosen rate, and an owner decision recorded.

---

## Step 10 — Define the phone's channels, and verify them

**Nothing in this repository can check the phone's channel list** — `CLAUDE.md` owns that rule and
it has already cost this project three faults and five sessions of a CAN byte.

1. Enter the channels from step 3's table into RaceChrono.
2. **Run the sentinel check with the sensors disconnected**, which is the only state it works in:
   every pressure channel must read **−3276.8**, not +3276.8 (this said −327.68 until step 3
   settled the divide; see the Work Progress row). A positive reading means
   `bytesToUint` where `bytesToInt` belongs — the exact fault found on `Temperature Front 2`.
3. **Verify the free-running counter** on `0x606` bytes 4–5 advances by exactly 1 per sample.
4. Re-export the vehicle profile to `RaceChrono/vehicleProfile.json` and commit it.
5. Run `python3 Tools/rcz-channels.py <session>.rcz` against a recording and confirm every new slot
   appears and none is all-`NaN`.

**Acceptance:** all six pressure channels and the status channels decode correctly against the
logger's own session record of the same samples.

---

## Step 11 — Documentation sync and commit

Run the required sync pass across both repositories before committing:

1. `README.md` — status, the hardware table, the layout block, "what is testable".
2. `CLAUDE.md` — any trap the implementation proved or disproved. **If something in this plan
   surprised the implementer, that belongs in `CLAUDE.md` and its predecessor in
   `CLAUDE.history.md`.**
3. `Hardware/logger-perfboard-wiring.md` — only if the build changed; it should not have.
4. `../ndLouvers/CFD-Learning-Plan.md` Step 0b — the commissioning item 2 identity record is now
   produced automatically, which is a status change that plan owns.
5. `../ndLouvers/pressure-testing.md` §3 — that the channel exists, and that **it still carries no
   measurement**.
6. `../ndLouvers/` open item 44 — whether any boot in this work was in the refusing mode. If one
   was, **that is the full-device-count observation the item has been waiting for.**

---

## Risks, and what each would cost

1. **A mux channel hangs the bus in the field.** The worker takes every other channel down with it,
   including the BME280. **Mitigation:** the enabled-list refusal in step 4, plus consider a
   `~RESET` pulse on GPIO17 as an automatic recovery after N consecutive exhausted transfers —
   **this is new behaviour and needs its own decision**, because a reset also disturbs a healthy
   BME280 mid-conversion.
2. **10 Hz starves the fsync cadence or fills the card.** Step 9 measures it before it is trusted.
3. **The production `.ini` is not updated** and the logger refuses to start at the car, losing a
   session. Step 8 item 3; this is the highest-probability failure in the plan.
4. **The phone's channel list is wrong and nothing detects it.** Step 10, and the rule survives the
   step — re-run the sentinel check after *any* edit to the list.
5. **The scaling decision is regretted later.** Cheap to reverse: the session file carries raw
   counts and the scale factor, so every past session can be reprocessed. Only the `.rcz` recordings
   cannot be, which is the same asymmetry that governs everything on the BLE path.

---

## Work Progress

Update this table after every executed step, with what was actually done rather than what was
planned.

| Step | Status | What was done |
|---|---|---|
| 1 — Owner decisions | **done** 2026-09-19 | All four put to the owner as choices with recommendations; **all four recommendations taken**. Recorded in `../ndLouvers/CFD-Learning-Plan.md` Step 0b commissioning item 4a. Decipascals on all six channels with `INT16_MIN` sentinel; configurable rate shipped at 10 Hz, subject to step 9; packets published before any role mapping; IDs `0x605`/`0x606`/`0x607`, checked clear against the committed `RaceChrono/vehicleProfile.json` (the only IDs either box claims are `0x78`, `0x202`, `0x420`, `0x4FA`, `0x600`–`0x604`, `0x7F0`). The role→channel mapping is untouched and still open — `../ndLouvers/` open item 30a |
| 2 — Data contracts | **done** 2026-09-19 | `dataContracts.hpp`: the three packet IDs, the SDP810 command/product/scale/CRC/length constants, `SDP810_START_SETTLE_US`, `PRESSURE_DECI_PA_INVALID`, per-product range bounds, `MUX_CHANNEL_NONE`/`MUX_MAX_CHANNEL`, the mux-prohibition comment block, the `[pressure]` config keys, and `SdpReading`/`PressureChannel`/`PressureData`. `PRESSURE_CHANNEL_COUNT` left at 6 with `isEnabled` carrying which are real. Built on the box, logger ran unchanged. **One deviation from the plan's field list:** `crcFailures` was added to `PressureChannel` and `PressureData`, because step 3's `0x607` needs a CRC counter separate from the transport one and the plan's struct list did not carry it |
| 3 — Wire format on paper | **done** 2026-09-19 | `README.md`: slot-map rows and two new sections written **before** the packer. Layout as the plan proposed. Slots are `Pressure Front 1`–`6`, `Digital Front 21`–`27` and `Temperature Front 21`, all clear of every slot already in use. **One decision the plan left implicit: the phone-side divide.** Written as `/10000` first, for genuine kPa, and **reversed to `/10` the same day (owner) after the readout proved unusable** — full scale showed 0.05 and the 45–90 Pa measurands 0.045–0.090. `/10` puts **pascals** on the gauge, the unit every requirement in the plan is written in, at the cost of a column RaceChrono labels kPa containing Pa. **Resolution was never the issue and that was measured before choosing:** RaceChrono stores samples as **float64** — `Pressure Front 50`'s come back as `101.15000000000001`, 8 bytes each — so `/10000` would have cost nothing in the recording. The only cost was the readout, and the readout is the primary check instrument. The sentinel is therefore **−3276.8** |
| 4 — Mux and SDP810 transport | **done** 2026-09-19 | New `pressureSensors.cxx`, included in `main.cxx` after `i2cBus.cxx` and before `raceChronoBle.cxx`, in no CMake target. `selectMuxChannel` refuses any channel outside the configured list and logs the refusal as an event; it writes then **reads the control register back and verifies it**. `deselectMux`, `sdpCrc8`, `sendSdpCommand`, `readSdpFrame`, `parseSdpMeasurement` all straight through `transferI2c` with **no second retry layer**. **One deviation:** `parseSdpMeasurement` takes the part's range as an argument — the plan asked the same function to set `outOfRange`, which it cannot do without knowing the part. It also rejects a zero scale factor, which passes CRC and would otherwise divide by zero |
| 5 — Identity and baseline record | **done** 2026-09-19 | `initialisePressureSensors()` opens its own fd, and per enabled channel selects, stop-continues (NAK tolerated), reads identity, verifies all six CRCs, records product and serial, and warns on an unknown product or a scale factor disagreeing with it. **Warns, never refuses.** `writePressureBaseline()` carries bus, mux and sensor addresses, the enabled list and per channel the mux position, product, serial, scale and range, plus a `roleMappingNote` stating the role map is deliberately absent. **Acceptance met:** a bench run's `pressureBaseline` carries all five serials and they match build sheet §5 exactly, including the ±125 Pa's `0x00000000978B88F8` on `P2` at 240 counts/Pa |
| 6 — Sampling loop | **done** 2026-09-19 | `pressureSensorsLoop` follows `bme280SensorLoop`'s shape. `0x3615` once per sensor; re-issued only when `isContinuousStarted` is false, which a failed read clears — never unconditionally. Every failure path writes an explicit `invalidReason` (`disabled`, `busUnavailable`, `muxSelectFailed`, `notStarted`, `readFailed`, `crc`, `scaleFactorZero`, `outOfRange`, `notPresent`); **no carried-forward values.** One `pressure` record per cycle with raw counts, returned scale factor, full-resolution pascals, the decipascals sent, sensor temperature, CRC result, validity and reason, plus the three `i2c*` counters. Worker added to `producersRunning` and the joins. **Acceptance met on a 25 s bench run:** 239 cycles, enabled mask 31 and valid mask **31 on every cycle**, counter +1 exactly, 0 read errors, 0 CRC failures, 0 exhausted transfers, `muxSelected` 0 throughout. Cycle cost 16–18 ms, matching bring-up's 15.4 ms |
| 7 — Sentinel initialisation | **done** 2026-09-19 | `initialiseBlePackets()` publishes all six channels as `INT16_MIN` before the worker reads anything, with the reason stated: zero is a plausible differential pressure where 0 °C at least looks like weather. `getAllPackets()` extended to 8 so `forEachBlePacket` covers the three. `0x607` is left at its zero default deliberately — zero **is** the correct enabled/valid mask before the first cycle |
| 8 — Config and enabled list | **done** 2026-09-19 | `config.cxx` parses `pressureIntervalMs` (50–60000) and `pressureChannelsEnabled` as a comma-separated list. `[pressure]` added to `build/KnurLogger.ini` with the comment block explaining why the list is a list. **Item 3 discharged:** the production `~/bin/KnurLogger.ini` was backed up to `KnurLogger.ini.bak-20260919` and replaced with the template — a diff first confirmed the two differed **only** in comment blocks and the new section, so no offset or binding was at risk, and the four ROM IDs were re-read afterwards. **Acceptance met — six guards tested on the box and all six refuse to start:** channel 5 named (by name, not as a range error), missing interval, interval out of range, non-numeric entry, stray comma, missing list. Starts cleanly on `0,1,2,3,4` |
| 9 — Measure cost, decide rate | **done** 2026-09-19 | One hour on the bench at 10 Hz, all five sensors, service stopped. **34 537 cycles at 9.594 Hz**, cycle counter exact `+1`, valid mask 31 on every one, **0 read errors, 0 CRC failures, 0 exhausted transfers, 0 dropped records**, worst inter-cycle gap 120 ms with nothing over 250 ms, cycle cost 18 ms typical / 23 ms p99. **20.2 kB/s → ~873 MB for a 12 h day against 108 GB free**, i.e. 0.8 % and ~120 such days before the card fills, against the 1457 B/s 1 Hz baseline. `i2cExhausted` 0; SoC 50.6–57.0 °C open-air; live, sticky and `rpi_volt` comparator 0 throughout. **Item 3 met: the fsync cadence is not starved** — the three 1 Hz workers were untouched (BME280 1015/1021 ms, 1-Wire 1002/1005, supply 1017/1024) and the largest gap between any two records of any kind was 445 ms. **Owner accepted 10 Hz on these numbers**, recorded in Step 0b commissioning item 4a.2. **One finding the plan did not predict: the configured interval is a floor, so "10 Hz" is 9.594 Hz** — every worker schedules from the cycle's start and pays the poll granularity, which is why the 1 Hz workers have always run at 1002–1017 ms. Not lost samples; the counter is exact. Recorded in Step 0b item 4 and `pressure-testing.md` §3.2 |
| 10 — Phone channels and verification | **blocked — owner and phone** | Items 1–5 all need the phone, and item 2 additionally needs the sensors physically disconnected, which cannot be done from here. What is ready: the slot map and byte tables in `README.md`, and `Tools/rcz-channels.py` **extended to flag the unsigned sentinel on `Pressure` slots as well as `Temperature` ones** — it did not, which would have left the six new channels unaudited by the one instrument that can check the phone's list without the phone. `Pressure Front 50` is exempt, `0x600` being unsigned by design. **Correction to item 2 of this step: the sentinel reads −3276.8, not −327.68**, because the wire carries decipascals and the phone divides by 10 (step 3). Still negative, still impossible, so the check is unaffected — but the number to look for is different. **`Tools/rcz-channels.py`'s `UNSIGNED_SENTINELS` carries the same figure and must move with any future change to that divide** — nothing would warn you, the check would simply stop matching and report a healthy channel. **Until the channels are entered, `0x605`–`0x607` are never sent at all** |
| 11 — Doc sync and commit | **done** 2026-09-19 | Sync pass run across both repositories. `README.md`: status, hardware and layout blocks, the two new packet sections, the slot map, "Next" and "what is testable". `CLAUDE.md`: the re-arm shape, the mux control read-back, and **a 10 Hz cycle not keeping the bus warm**; the battery-drain estimate flagged as predating both the SDP810s' load and the sixth worker. `CLAUDE.history.md`: a dated entry. `RaceChrono/README.md`: the profile is now knowingly stale and says so. `Hardware/logger-perfboard-wiring.md` §5: the build did not change, but the "never sweep" rule is now enforced in code and the note says where. `../ndLouvers/`: Step 0b items 2, 4, 4a and the status block; `CLAUDE.md` trap 1; `pressure-testing.md` §3 preamble, §3.1, new §3.2 and §4 item 9. **Mechanical drift fixed:** "all five workers" → six, "ONE is fitted to mux channel 0" → all five, "refused every time" → bimodal per boot, "the pressure worker will use" → does. **One contradiction surfaced rather than resolved, and the owner settled it:** the `0x3615` harness figure was "145 of 456 transfers" in two files and "145 of 150" in four; the answer is **150 start-continuous commands**, and all six now name the denominator |

**Blockers:** none remaining in this repository. **Step 10 is the only outstanding step and it is
owner-and-phone work** — the six pressure channels and their status fields have to be typed into
RaceChrono, and until they are the three packets are never sent at all. The sentinel check within it
additionally needs the sensors physically disconnected, which is the only state it works in.

**What this plan deliberately did not do, so that nobody reads its completion as more than it is.**
It produced no pressure measurement and discharged no part of `../ndLouvers/pressure-testing.md` §2.
It did not decide the role→channel mapping (open item 30a) and the `pressureBaseline` record states
that absence in a field of its own. It did not add the `~RESET`-on-exhaustion recovery from risk 1,
which the plan itself says needs its own owner decision. And it did not answer open item 44: every
boot it ran on was in the non-refusing mode.
