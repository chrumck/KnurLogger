# Pressure correction — implementation plan

**Read this first.** This file is the executable plan for correcting the five SDP810 channels
before they reach RaceChrono. Each step below stands alone: it names the files, the change, the
command and the acceptance. **After executing a step, update its row in [Work Progress](#work-progress)**
with the status and what was done, so re-reading this file always shows where the work stopped
and what comes next. Work from `C:\_claude\KnurLogger` on the workstation and build on the box
(`ssh KnurLogger`, `~/KnurLogger/build`); [operations.md](operations.md) owns build and deployment.

[ndLouvers instrumentation-spec.md](../ndLouvers/instrumentation-spec.md) owns measurement
acceptance and [pressure-testing.md §2.4 step 4](../ndLouvers/pressure-testing.md#24-the-calibration-regime)
owns the correction's physics. The fitted values are in
[pressure-testing.md §3.3a](../ndLouvers/pressure-testing.md#33a-the-tubing) and §3.3b; read
them from there rather than from any copy.

## Decisions (owner, 2026-09-24)

1. **Divide.** The true pressure at the pickup is the reading divided by the combined factor
   `g(P)`. The SDP810 is a thermal mass-flow sensor calibrated at 966 mbar, so denser air makes it
   read high: `reading = Δp × P_abs/96 600 Pa`. At 1006.5 mbar a true 100 Pa reads 104.2 Pa.
   Dividing recovers 100 Pa; multiplying gives 108.6 Pa, applying the error twice. The bench ladder
   brought all five sensors within ±1.5 % only with division, and the span terms were fitted that way.
   **Do not implement the multiply form** — the old README's wording, see history 2026-09-24.
2. **Stale absolute pressure: hold, then invalidate.** Use the last valid BME280 pressure for up
   to 10 s — a steep climb moves it ~0.2 % in that time — then send the invalid sentinel on every
   corrected channel until a valid value returns. Raw counts are logged throughout. Every pressure
   record carries the absolute pressure used and its age.
3. **Configuration shape: constants plus lengths.** Global `r` and `R_fixed`, per-slot route
   lengths in metres, per-slot span pair `ε+`/`ε−`, and per-slot bypass-resistance fit
   (coefficient and exponent). The code computes `R_path`.

## The correction, in the form the code uses

For one channel with reading `rdg` (Pa, signed), absolute pressure `P_abs`, and the slot's values:

    B       = P_abs / 96 600 Pa
    ε       = ε+ if rdg ≥ 0, else ε−
    Δp_port = |rdg| / (B · (1 + ε))                      true Δp across the sensor's ports
    Q       = (Δp_port / (a · 10⁻⁶))^(1/(n + 1))          mL/s, from R_s(Q) = a · Q^n
    R_path  = Σ over the slot's lines with L > 0 of (R_fixed + r · L)
    P       = sign(rdg) · (Δp_port + Q · 10⁻⁶ · R_path)

This is §2.4 step 4's `P = reading / g(P)` solved explicitly: the sensor passes a flow `Q` set by
its own bypass curve, and the lines upstream and downstream lose `Q · R_path` before the sensor
sees the rest. A port left open to the bay has no line, no filter and no wand, so it has length 0
and contributes nothing. A zero reading corrects to zero.

**Reference values** for acceptance, with `a` = 2.093 × 10⁸, `n` = 0.615, `r` = 3.1 × 10⁶,
`R_fixed` = 2.6 × 10⁶:

| case | inputs | corrected |
|---|---|---|
| bench charge C, `P0`, rung 6 | rdg 364.86 Pa, `P_abs` 100 090 Pa, ε 0, L_H 10.088 m, L_L 0 | **398.885 Pa** (bell 394.5) |
| bench charge A, `P0`, rung 3 | rdg 101.94 Pa, `P_abs` 100 090 Pa, ε 0, L_H 10.088 m, L_L 0 | **119.611 Pa** (bell 121.7) |
| `P1`, negative, no lines | rdg −100 Pa, `P_abs` 101 325 Pa, ε+ 0.0091, ε− −0.0091 | **−96.212 Pa** |
| 70 Pa on 3 m + 3 m | rdg 70 Pa, `P_abs` 101 325 Pa, ε 0, L_H 3, L_L 3 | **78.463 Pa** |
| zero | rdg 0 | **0** |

---

## Step 1 — Configuration keys and data contracts

**Files:** `dataContracts.hpp`, `config.cxx`, `build/KnurLogger.ini`.

1. In `dataContracts.hpp`, add to the `[pressure]` group:
   1. `tubingResistancePerMetre` (Pa·s/m³ per m; bounds 1e5–1e8) and `lineFixedResistance`
      (Pa·s/m³; bounds 0–1e8), both global.
   2. Per slot, as format strings like `CONFIG_KEY_TEMP_OFFSET_C_FORMAT`: `p{}SpanPositive`,
      `p{}SpanNegative` (fraction; bounds ±0.05), `p{}LineLengthHighM`, `p{}LineLengthLowM`
      (m; bounds 0–20), `p{}BypassCoefficient` (Pa·s/m³ at 1 mL/s; bounds 1e7–1e9) and
      `p{}BypassExponent` (bounds 0–2).
   3. `SDP810_CALIBRATION_ABSOLUTE_PA 96600.0` and `PRESSURE_ABSOLUTE_HOLD_MS 10000`, each with a
      one-line comment saying why (the datasheet's calibration pressure; the hold bound's 0.2 %).
   4. The matching `AppConfig` fields, per-slot arrays indexed by mux channel.
2. In `config.cxx`, load the globals always and the per-slot keys **for every enabled channel**.
   A missing or out-of-range key is a startup error through `logErrorAndKill`, never a silent zero —
   the rule the thermal offsets already follow.
3. In `build/KnurLogger.ini`, add the keys with the §3.3a/§3.3b values: `r` 3.1e6, `R_fixed`
   2.6e6; spans 0/0 for `P0`, `P2`, `P3`, `P4` and 0.0091/−0.0091 for `P1`; bypass 2.093e8/0.615
   for `P0`, `P1`, `P3`, `P4` and 1.586e8/0.216 for `P2` (provisional, two points); **all line
   lengths 0** until the routes are measured on the car.

**Acceptance:** the logger builds, starts with the new template, and refuses to start — naming the
key — when one enabled slot's key is deleted.

## Step 2 — Share the BME280's absolute pressure with the pressure worker

**Files:** `dataContracts.hpp`, `bme280Sensor.cxx`.

1. Add to `Bme280Data` two atomics, `std::atomic<gdouble> heldPressurePa` and
   `std::atomic<guint64> heldPressureBootUs`, with a comment that they are the one cross-thread
   read of this struct and why (the pressure correction). `Bme280Data`'s "no lock" comment must be
   updated to say so.
2. In `sampleBme280()`, write both whenever `reading.isPressureValid` — never on an invalid
   reading, so the held value is always the last good one.
3. Add `gboolean getHeldAbsolutePressure(gdouble* outPa, guint64* outAgeMs)`, returning FALSE if
   no valid value has ever been read. `bme280Sensor.cxx` is included before `pressureSensors.cxx`,
   so the pressure worker can call it directly.

**Acceptance:** builds; a hand-run session's `enclosure` records and the held value agree.

## Step 3 — The correction

**File:** `pressureSensors.cxx`.

1. Add `gdouble getLinePathResistance(gint slot)` and
   `gboolean correctPressure(gint slot, gdouble readingPa, gdouble absolutePa, gdouble* outPa)`
   implementing the section above exactly. Its comment states the divide decision in one line and
   points at this file.
2. In `samplePressure()`, read the held absolute pressure once per cycle. For each valid raw
   reading: if the held value exists and is no older than `PRESSURE_ABSOLUTE_HOLD_MS`, correct it;
   otherwise the channel's sent value is `INT16_MIN` with reason `absolutePressureStale` (or
   `absolutePressureMissing`). **The raw `pressurePa` is kept unchanged.**
3. `pressureDeciPa` — what goes on the air — becomes the corrected value. The raw value stays in
   the record at full resolution beside it.
4. Log one `event` record when the absolute pressure goes stale and one when it recovers, not one
   per cycle (the rate-limiter pattern of `isChannelRefusalReported`).

**Acceptance:** a throwaway test harness, or a temporary `--selftest` path removed before commit,
reproduces every row of the reference table to within 0.001 Pa.

## Step 4 — Records and packets

**Files:** `pressureSensors.cxx`.

1. Each channel in the `pressure` record gains `correctedPa` (null when not sent). The record gains
   top-level `absolutePressurePa` and `absolutePressureAgeMs` — the value used, not the latest.
2. `pressureBaseline` gains a `correction` object: the formula name (`divide`), the hold bound,
   `r`, `R_fixed`, and per enabled slot its spans, line lengths, bypass fit and computed `R_path`.
   A recording then states its own correction, as the thermal offsets do.
3. `0x607`'s valid mask describes the **sent** value: a channel whose raw reading is valid but whose
   correction is stale is not valid there.

**Acceptance:** one hand-run session on the box shows the new fields, and `0x605`/`0x606` carry the
corrected decipascals.

## Step 5 — Offline check against a recording

**File:** new `Tools/pressure-correction-check.py`.

1. The script reads a session file, takes `correction` from `pressureBaseline`, recomputes every
   channel's corrected value from `pressurePa`, `absolutePressurePa` and the parameters, and reports
   the largest difference from the logged `correctedPa`, plus any stale-hold periods.
2. Run it on the step 4 session.

**Acceptance:** largest difference under 0.01 Pa on every channel; the stale-hold count matches the
`event` records.

## Step 6 — Deploy

1. **Hand-merge every new key into `~/bin/KnurLogger.ini` first** — deployment never touches that
   file, and a build with a missing required key crash-loops the service
   ([operations.md](operations.md)). Line lengths stay 0.
2. Build on the box, stop `KnurLogger.service`, deploy with `SystemSetup/deploy-logger.sh`, start
   the service, then back the production `.ini` up into `build/KnurLogger.ini` and commit.

**Acceptance:** the service runs; the phone shows the pressure channels; the session header's
`correction` matches the production `.ini`.

## Step 7 — Documentation

1. [racechrono-channels.md](racechrono-channels.md): `0x605`/`0x606` now carry the corrected
   pressure; the channel equations are unchanged.
2. [session-records.md](session-records.md): the new record fields and the stale-hold behavior.
3. [operations.md](operations.md): the new keys and where their values come from.
4. ndLouvers: `pressure-testing.md` §2.4 step 4 and `instrumentation-spec.md` Step 0b say the
   correction is implemented; `work-progress.md` Step 0b row.
5. [CLAUDE.history.md](CLAUDE.history.md): a dated entry. Run the sync pass and
   `python ../ndLouvers/tools/check-markdown-links.py`.

**Acceptance:** 0 link findings; no document still describes the correction as unimplemented.

## Step 8 — Route lengths (after the lines are cut on the car)

1. Enter each slot's measured `p{}LineLengthHighM`/`p{}LineLengthLowM` into `~/bin/KnurLogger.ini`,
   restart the service, back the file up, commit.
2. Run the installed smoke test (ndLouvers `pressure-testing.md` §2.4 step 5).

**Acceptance:** every installed channel passes the smoke test; the session header's `R_path` per
slot matches the measured lengths.

---

## Work Progress

| Step | Status | What was done |
|---|---|---|
| Decisions: direction, stale input, config shape | **done** 2026-09-24 | Owner: divide; hold 10 s then invalidate; constants plus lengths |
| 1 — config keys and data contracts | not started | |
| 2 — share the BME280 absolute pressure | not started | |
| 3 — the correction | not started | |
| 4 — records and packets | not started | |
| 5 — offline check | not started | |
| 6 — deploy | not started | |
| 7 — documentation | not started | |
| 8 — route lengths | not started — needs the lines cut on the car | |
