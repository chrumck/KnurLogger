# Pressure correction — implementation requirements

**Status: not implemented.** The acquisition worker is complete; its original delivery record
is linked from [pressure-worker-plan.md](pressure-worker-plan.md), which retains the pending
recording audit. Acquisition delivery does not include correction.

[ndLouvers instrumentation-spec.md](../ndLouvers/instrumentation-spec.md) owns measurement
acceptance. [pressure-testing.md §2.4 step 4](../ndLouvers/pressure-testing.md) owns the correction
equation, fitted parameters and prerequisites. This file owns the implementation handoff.

## Unresolved direction wording

The measurement procedure defines `P = reading / g(P)`, with `P_abs / 96 600 Pa` inside `g(P)`.
The former logger router and README instead said to multiply the correction by that factor.
These are not interchangeable. The owner has not resolved this conflict; do not implement the conversion until it is resolved.
Preserve the procedure's equation meanwhile.

In practical terms: under the model written in the procedure, a true 100 Pa at the stated
bench atmospheric pressure would read about 104.2 Pa. Dividing by 1.042 recovers about 100 Pa;
multiplying instead gives about 108.6 Pa. Division is consistent with that written model.
This consistency check does not independently validate the sensor model or its physical assumptions.
The question affects the planned phone conversion, not a correction already running on the logger.

## Required behavior

1. Derive per-channel correction from the sensor span fit, route lengths and line-resistance
   work. Read values from their measurement owner instead of copying a fit table here.
2. Store separate positive and negative span values in `KnurLogger.ini`, keyed by channel slot.
   No hard-coded span and no implicit zero for an enabled channel: a missing key is a startup error.
3. Recheck a slot's parameters when its sensor is replaced. They describe the fitted sensor,
   not an interchangeable board position.
4. Use the live BME280 absolute-pressure measurement rather than a fixed calibration-day value.
   Preserve raw counts in local logging; correction applies to the RaceChrono feed.
5. Record correction parameters and the absolute-pressure input in the session header as the
   measurement procedure requires. The procedure also requires a live input. Before implementation, settle how each corrected
   sample identifies the BME280 value used and what happens when that value is invalid or stale;
   the present documents do not specify those behaviors. A header alone records only startup state.
6. Before deploying a build with required keys, hand-merge those keys into the production
   `~/bin/KnurLogger.ini`; deployment does not update that file. Follow [operations.md](operations.md).

## Work Progress

Update this table after each implementation step once the conversion wording is settled.
Measurement dependencies retain their own progress owners; do not duplicate their status here.

| Work | Status | Dependency |
|---|---|---|
| Resolve direction wording and live-input behavior | blocked | Owner decision and missing behavior above |
| Complete line-resistance input | See run sheet | [Run sheet](../ndLouvers/step0b-rig/line-resistance-runsheet.md) owns execution status |
| Configure, implement and verify correction | not started | Accepted equation, measured routes and fitted inputs |
| Installed acceptance | See measurement progress | Measurement procedure §2.4 step 5 owns the test |
