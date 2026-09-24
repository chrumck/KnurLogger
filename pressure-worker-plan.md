# Pressure acquisition — delivery and remaining verification

The acquisition implementation is delivered. The remaining acceptance is a recording audit;
this is distinct from the unimplemented [pressure correction](pressure-correction.md).
Update Work Progress below after executing the audit, with the recording paths and outcome.

The original completed steps and evidence are in
[CLAUDE.history.md](CLAUDE.history.md#pressure-worker-delivery). Do not replay those steps or
restore the retired `pressureBaseline` documentation fields — see that history's 2026-09-23
entry on removing document citations. The current schema is implemented in the logger.

## Step 10 — Audit a recording containing pressure channels

1. Work from `C:\_claude\KnurLogger`. Obtain a RaceChrono `.rcz` with the pressure slots and
   the corresponding local logger session file. The channels and equations are in
   [racechrono-channels.md](racechrono-channels.md); export interpretation is in
   [ble-protocol.md](ble-protocol.md).
2. Run `python3 Tools/rcz-channels.py <session>.rcz`. Inspect every fragment, including
   `resume_<n>/`. Confirm the five defined pressure slots and their status channels appear.
   `Pressure Front 6` is deliberately absent because mux channel 5 is unpopulated.
3. Match samples using cycle counters and [session-records.md](session-records.md).
   Confirm decoded values and validity against the same local samples. Investigate all-empty
   channels; emptiness alone is not proof of a configuration error.
4. Acceptance: the five defined channels and status decode correctly against the local record,
   with no unexplained missing or empty channels. Record the checked artifact paths and result below.
5. After any phone-channel edit, repeat the disconnected-sensor sentinel check (negative
   pressure sentinel), verify the counter and re-export the profile following
   [RaceChrono/README.md](RaceChrono/README.md). A previous pass does not validate a later edit.

## Work Progress

| Original step | Status | Evidence / remaining work |
|---|---|---|
| 1–9 — acquisition implementation and bench verification | done | [Delivery record](CLAUDE.history.md#pressure-worker-delivery) |
| 10 items 1–4 — phone entry, sentinel, counter and export | done | Delivery record; repeat checks after any definition edit |
| 10 item 5 — recording audit | not started | Procedure above; acceptance remains unmet |
| 11 — original documentation sync | done | Delivery record; the current cleanup is recorded separately in history |

## Boundaries

1. Acquisition delivery qualifies no pressure channel. The measurement project owns acceptance.
2. Pressure role allocation remains [ndLouvers open item 30a](../ndLouvers/work-progress.md).
3. Automatic mux reset on exhausted transfers requires its own owner decision; do not infer it
   from the diagnostic reset procedure. The idle-bus physical cause remains open item 44.
