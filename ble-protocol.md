# BLE behavior, diagnostics and recording audits

[racechrono-channels.md](racechrono-channels.md) owns packet bytes, equations and the phone slot
map. [CLAUDE.history.md](CLAUDE.history.md) owns incidents and diagnoses; this file owns the
current protocol constraints and reusable checks.

## Subscription handling

1. RaceChrono requests the union of packet IDs defined for every connected DIY device.
   Unknown IDs are normal traffic: log and ignore them without returning an ATT error.
   An error can abort the subscription burst after `deny all`, leaving the display frozen.
2. Logging starts with `deny all` followed by `allow single` requests. CAN-bus test mode uses
   `allow all`, so success there does not prove the logging subscription sequence works.
3. Accept command-length minima, not exact-length equality, so optional future fields do not
   turn a valid command into a rejection. Do not reinstate the old strict checks; see `CLAUDE.history.md` §1.
4. With filtering honored, a packet without any defined phone channel is never sent.
   Use a defined free-running counter for link-loss checks. The packet reference owns their offsets.
5. The two boxes share the phone's channel list. Adding a channel for one can expose a
   subscription-handling bug in the other. Do not assume separate repositories mean independent
   protocol configuration.

## GLib context ownership

1. Call `g_main_context_push_thread_default()` before `g_bus_get_sync()` and
   `binc_adapter_get_default()` in the BLE worker. GDBus binds subscriptions to the current
   thread-default context; this headless program does not iterate the global default context.
2. Keep disconnect handling: clear notification state and restart advertising so a lost link
   can reconnect. Missing adapter callbacks can silently prevent both.
3. KnurDash's GTK main loop iterates the global context. Do not copy this headless ordering fix
   into that app merely because its order differs.
4. Local records before the context fix on 2026-09-10 omit BLE connection events. Their absence
   is not evidence that no phone connected; see the dated history record.

## The phone is part of the instrument

1. The logger cannot detect a missing or incorrect phone equation. Compare a RaceChrono export
   with the local logger record of the same samples.
2. Re-run the disconnected-sensor sentinel check after any channel-list edit. Positive thermal
   values cannot distinguish signed from unsigned decoding. Differential pressure can be
   negative during normal operation, so signedness matters beyond the sentinel.
3. RaceChrono stores defined decoded CAN channels, not every byte of the arriving frame.
   An undefined CAN byte cannot be recovered from an old `.rcz`. Defining it later helps only
   later recordings; the thermal companion owns the affected-session record.
4. The saved profile and restoration procedure live in [RaceChrono/README.md](RaceChrono/README.md).
   Compare exports by `(pid, channelId)`, not textual diff order. Preserve the exported array
   order and strip `localUuid`; re-import with a sorted array has not been validated.

## Audit an export without the phone

Run from the KnurLogger checkout:

```bash
python3 Tools/rcz-channels.py session.rcz
```

1. A channel ID is `slot * 2**20 + channelType`. Types used here are Digital 70537,
   Temperature 70539, Pressure 70541 and Percent 70547. An old export records the slots in
   force then; disagreement with today's map does not by itself make either wrong.
2. The tool flags unsigned decoding of the negative sentinel: +327.68 in Temperature slots
   or +3.2768 in signed Pressure slots. Enclosure `Pressure Front 50` is unsigned by design
   and exempt. If wire scaling changes, update the tool's sentinel constants too.
3. An all-NaN channel needs investigation, not automatic repair. A sensor can intentionally
   report an out-of-range marker throughout a session. Box 1's `lowPass(E,254)` and
   `lowPass(F,254)` are examples; inspect operating conditions and the channel definition.
4. RaceChrono equation names are case-insensitive. Do not normalize casing merely for a diff;
   the single-letter distinction between signed `bytestoint` and unsigned `bytestouint` matters.
5. Read every fragment: the first is at archive root and subsequent stretches are under
   `resume_<n>/`. The tool reports them separately; channel edits between stretches can be real.
6. Match local records by cycle counters following [session-records.md](session-records.md).
   Include mounting state before treating correct protocol output as a physical measurement.

## When advertising fails

1. Try `bluetoothctl advertise on`. Failure there points to the platform rather than this logger.
2. Read `journalctl -u bluetooth` for the server-side reason.
3. As root, try `btmgmt add-adv -c -u 1ff8 1` via the legacy MGMT path. Success separates
   controller/kernel capability from a bluetoothd failure.
4. Capture an attempted advertisement with root `btmon -w file`, then inspect `btmon -r file`.
5. Inspect `SupportedInstances` and `ActiveInstances` on `org.bluez.LEAdvertisingManager1`.
   One active logger should register one advertisement; zero means registration did not succeed.
6. The tested combination is kernel `6.18.39`, BlueZ `5.82-1.1+rpt2`, and
   firmware-brcm80211 `1:20260519`. The older kernel `6.18.34` / BlueZ `5.82-1.1+rpt1`
   combination failed advertising; see `CLAUDE.history.md` §1.2 before diagnosing a regression in logger code.
