# The 1-Wire path — DS18B20 probes, the kernel, and what must not change

**This file owns 1-Wire behavior and standing requirements.**
[operations.md](operations.md) describes how to enroll probes, enter offsets and run the
fake-sysfs harness; this file says why each is shaped the way it is and what breaks if it changes.
`CLAUDE.history.md` holds the faults these guards came from — read it when a statement here
surprises you, never to reinstate something that looks missing.

**It owns no measurement decision.** [ndLouvers instrumentation-spec.md](../ndLouvers/instrumentation-spec.md) Step 0b's thermal-channels
subsection is the authority on what the thermal channels must achieve and what counts as done, and
`../ndLouvers/thermals-testing.md` holds the rig as built and every recorded result. A disagreement
between this file and Step 0b is resolved in Step 0b.

**Channel names are positional and mean nothing.** `temp0`–`temp3` are fixed by DS18B20 ROM ID at
enrollment, and a channel is never renamed after a role. The role map is decided and applied
(owner, 2026-09-09; enrolled in installed order 2026-09-10): **`temp0` = `T_ambient`,
`temp1` = `T_core_in`, `temp2` = `T_core_out`, `temp3` = `T_aft`** — confirmed independently by
warming each probe in installed order, which moved them in that order with clean separation.

---

## The probes and the bus

**A DS18B20 has no positional anchor.** Its 64-bit ROM ID *is* the channel definition, recorded
once at enrollment. **All four are bound as of 2026-09-10** — `temp0` `28-06254385da1f`,
`temp1` `28-0625424044b7`, `temp2` `28-062542ac86b6`, `temp3` `28-0625424e16c9` — in
`~/bin/KnurLogger.ini` on the box, in `build/KnurLogger.ini` in git, and in
`SystemSetup/logger-perfboard-wiring.md` §5a as the human record. **An earlier attempt the same day
bound three in a different order and was discarded** (history §1.11); do not reconcile anything
against it.
**The channel → role map IS independently confirmed** (2026-09-10): warming each probe by hand
in installed order moved `temp0`, `temp1`, `temp2`, `temp3` in that order with clean separation,
+3.8 to +5.4 K each. Plan open item 42 is closed on it.

**`w1-gpio`'s `pullup` parameter is ignored** on this firmware — the overlays README says so
outright. The overlay that drives an external strong pullup is a different one,
`w1-gpio-pullup`, and this build must not use it: `R11` is a plain 2.2 kΩ resistor to 3V3.

## The `28-*` family filter and the kernel behaviour behind it

**The `28-*` family filter is mandatory, and it can no longer be exercised on this box.** The
bare bus used to invent churning phantom `00-*` devices; with the sensor zone assembled and
`R11` terminating `GPIO4` the scans come back clean (history §1.4). The filter is correct
regardless of cause — a marginal 4 × 5 m star can produce garbage of its own — but its
correctness now rests on the parser rather than on an observation. Family code `00` is not a
valid 1-Wire family; a DS18B20 is family **`28`**.
1. **Match `28-*` and nothing else, everywhere** — enumeration, binding, and reads.
   `w1_master_slave_count` is not "off by one", it is **unstable**, and no code may branch on
   it.
2. **A `00-*` ROM ID must never be persisted** as a `temp` channel definition. Anything that
   auto-binds "the next device that appears" will bind noise within about ten seconds.
3. **An *empty* devices directory is the real failure signal**, because a working bus always
   registers its master.
4. **Bus rescan is every 10 s** (`w1_master_timeout = 10`), which is the hot-plug detection
   latency for anything that watches for a probe being connected. It cannot be shortened without
   root — the master attributes are root-owned and the logger runs as `chrum`.
## `therm_bulk_read` — the only way to sample four probes at 1 Hz

**`therm_bulk_read` appears the moment a probe attaches, and it WORKS — since 2026-09-11**
(history §1.15; before that the attribute had never existed here, because `w1_therm` registers
it as a **master** attribute only once a slave of its family attaches).
It is the documented way to convert every probe at once and the only way to sample four probes
at 1 Hz: without it each `w1_slave` read pays its own conversion — **799–832 ms measured on
the loaded four-probe star** — so a cycle was **3198–3281 ms**, i.e. **0.31 Hz** against plan
thermal item 2's ~1 Hz. **With it, measured at the car over 123 cycles: one 762–790 ms
conversion for all four probes, a 1023 ms mean cycle interval — 0.977 Hz — and zero CRC
failures across 492 reads.**
**TWO conditions are necessary and NEITHER alone is sufficient. Both are in place; do not undo
either.**
1. **The permission.** The cause was `errno 13`, `EACCES` — `w1_therm` registers the attribute
   `0644 root:root` and the logger runs as `chrum`, under `KnurLogger.service` too, whose
   `User=chrum` and `SupplementaryGroups=video i2c gpio` *extend* rather than replace chrum's
   own group memberships. `SystemSetup/grant-w1-bulk-read.sh` fixes that and **is applied**:
   the attribute comes up `root:gpio 664` and `chrum` writes it.
2. **The trigger is EIGHT bytes, newline included.** `therm_bulk_read_store` gates on
   `size == sizeof("trigger")`, which is 8. `oneWireProbes.cxx` wrote `strlen("trigger")` = 7,
   so the command was never parsed and `trigger_bulk_read()` was never called — for a day
   that looked like a permission fix that had not worked. `ONE_WIRE_BULK_TRIGGER_CMD` now
   carries the `\n`; **do not "tidy" it away.**
**A successful write is not a conversion, and cannot be**: `therm_bulk_read_store` returns the
write size unconditionally and reports its refusal only with `dev_info`. The evidence is
therefore the readback and the kernel log, never `write()`'s return value —
`journalctl -k | grep therm_bulk_read_store` prints the errno userspace never sees, and it is
empty on a healthy boot. `err=-22` is `EINVAL`, which only the size guard produces; `err=-19`
is `ENODEV`, which means the command was parsed and the bus had nothing on it.
Four further properties, each of which cost something to learn:
1. **`0` and `1` are different answers and the code used to treat them alike.** The attribute
   reads `-1` while converting, `1` when results are ready and unread, `0` when no bulk
   conversion is pending — which after a fresh trigger means the trigger did nothing. The
   readback is recorded as `bulkState` in every `temp` record and **`"1"` is the healthy
   value.**
2. **The steady-state readback is `1`, NOT `-1`.** `trigger_bulk_read()` sleeps the whole
   conversion inside the write, so by the time `write()` returns the results are already
   there. `-1` is only observable by a concurrent reader, and this worker is the only one — so
   a poll loop that recognised a conversion solely by seeing `-1` could never recognise one,
   which is what the first version did.
3. **Time the conversion from BEFORE the write**, for the same reason: a clock started after
   `write()` returns measures nothing and puts a 0 on `0x603` bytes 6–7 for the most expensive
   part of the cycle. It also separates a real conversion (~780 ms) from a bulk path that ran
   and failed its bus reset (~0 ms — which still marks every slave ready, so `bulkState` alone
   cannot tell them apart).
4. **`bulkConversion` means a conversion the kernel confirmed**, not a write that succeeded.
**Parasite power was the standing first suspect and it was WRONG** — do not return to it.
`ext_power` reads `1` on all four probes in the one-shot `probeCapabilities` record, measured
twice (2026-09-10 and 2026-09-11), so the strong pullup this bus lacks was never needed.
**`probeCapabilities.bulkReadState` reads `"0"` on a HEALTHY box and that is not a fault.**
The record is emitted after the per-probe reads, and reading `w1_slave` is what consumes the
ready flag. The authoritative field is `bulkState` in the `temp` records.
**Three properties of the udev rule that grants the permission**, each of which cost something
to get right:
1. **The rule matches the SLAVE add, not the master's.** The attribute is a *master* attribute
   that only exists once a slave of the family attaches, so the master's own add event fires
   long before it. The kernel creates the family's master attributes from the bus notifier
   during `device_add()`, which runs before the slave's `KOBJ_ADD` uevent — so the slave event
   is both the earliest point the attribute exists and a point at which it does.
2. **`gpio` is the group** because chrum is already in it and the bus is bit-banged on GPIO4.
   No new group, no new membership, and it covers the service and a hand-run alike.
3. **It is testable without a probe.** `--verify` writes a fake family-0x28 ROM ID to
   `w1_master_add`, which makes `w1_therm` bind and the attribute appear, checks that chrum
   can write it, then removes the fake slave. Reach for `w1_master_add` before concluding a
   w1_therm behaviour cannot be exercised on a probe-less box.
**Do not restore the once-per-cycle warning.** It logged 863 identical lines in 14 minutes at
the car and buried the two `BOUND` messages that were the point of the run.

## A pulled probe answers with NOTHING for ~100 s, and that is not a read error

**A probe pulled off the bus keeps its sysfs entry for ~100 s and answers with NOTHING, and
that must not count as a read error** (measured at the car, 2026-09-10). The kernel unregisters
a slave only after `w1_slave_ttl` (10) missed searches at `w1_master_timeout` (10 s), so for
up to ~100 s after a lead comes out the directory is still there, `w1_slave` still reads, and
the content carries neither `crc=` nor `t=`. That is a dropped lead, and the requirement in
§"Requirements this path carries from the plan" — "`absent` must not increment the read-error
counter" — applies to it for exactly the same
reason, arriving through the one path that still had a directory to read. Getting this wrong
charged **186 read errors to a bus that had not failed once** (history §1.12).
1. **The reason is `notAnswering`, and it is counted in its own field**, not in `readErrors`
   and not on `0x603` byte 2–3. A bus that is genuinely dropping out still shows here, so
   nothing is lost — it is just not confused with a bus that read badly.
2. **The split rule is the two markers, not the byte count.** `w1_therm` prints `crc=…` and
   `t=…` whenever the probe answered at all — a CRC failure prints both — so content with
   neither means the kernel got no reading. Content with one of them is malformed and *is* a
   read error.
3. **Unparseable content is now recorded** (`unparsed` in the `temp` record, truncated). The
   first version threw the bytes away, which is why the fault at the car could not be told
   apart from a marginal bus without going back.

## 85.00 °C is a reading, and the power-on default is deliberately not detected

**A reading of exactly 85.00 °C is accepted like any other temperature** (owner, 2026-09-23;
`../ndLouvers/` open item 53, closed). 85.00 °C is the DS18B20's power-on scratchpad default AND a
temperature a probe can be at, and the scratchpad is byte-identical in both cases —
`50 05 4b 46 7f ff 0c 10 1c`, with a valid CRC — so nothing in the reading can separate them.
`T_aft` peaks at **101.25 °C** and spends 2 351 samples above 85 °C in post-run heat soak; **the aft
probe lives in this band**, so a check keyed on the value flags a hot probe far more often than a
reset one. The owner prefers keeping every reading over flagging.

1. **The accepted blind spot:** a genuine power-on reset now passes as a plausible 85.00 °C
   reading, with `valid` true and no reason code. One is on record — all four probes in the first
   cycle after boot, 14.1 s in, in the two-day session `2026-09-11T18-08-06`. **Four probes
   reading exactly 85.00 °C together in a session's first cycle is the signature**; a transit sits
   between neighbours a resolution step (~60 mK) away.
2. **There is no `powerOnDefault` reason code**, and `TEMP_POWER_ON_DEFAULT_CENTI_C` is gone. **Do
   not reinstate the check** — `CLAUDE.history.md`, 2026-09-23.
3. **Sessions recorded before 2026-09-23 still carry `powerOnDefault` flags**, and they are the
   historical record: in the two-day session 21 of them, four the genuine boot-cycle reset and 17
   `temp3` transiting 85.000 °C with a good CRC — none a bus fault. **In those sessions `0x603`
   bytes 2–3 counted the flags too**, so there it is not a bus-quality figure on its own
   (`../ndLouvers/thermals-testing.md` §2.5 item 3, §3.11).

The measurement-accuracy question, open item 52, was **dropped** (2026-09-20): every `T_aft` sample
above 85 °C is permanently an indication, not a measurement.

## Testing the whole path without probes

**The whole 1-Wire path IS testable without probes, and this is how** (2026-09-10).
`unshare -Urm --map-root-user` gives an unprivileged user namespace with a private mount
namespace, so a fake tree can be bind-mounted over `/sys/bus/w1/devices` — **no root, no sudo,
no risk to the real box, and nothing to undo** since the namespace dies with the shell.
Populate it with `w1_bus_master1/`, `28-…/w1_slave` files in the kernel's two-line
`crc=xx YES` / `t=<millidegrees>` format, and a `00-…` entry to prove the family filter drops
it. Enrollment order, the ambiguous-step refusal, the store's family and duplicate guards,
bound-but-absent, CRC failure, out-of-range rejection and the
application of a hand-entered offset were all verified this way. **Reach for this before
concluding a sysfs-driven path is untestable.**

**Its one limitation: a plain file cannot emulate an attribute whose read value differs from what
was written**, and `therm_bulk_read` is exactly that — you write `trigger` and read back
`-1`/`0`/`1`. In the fake tree it reads back `trigger`, so that state machine is unexercised, and
neither is real bus timing or real conversion time. **So the harness covers every branch a plain
file can represent, which is not every branch.** Two workarounds fill the gap and are worth
reaching for before concluding something is untestable: a **FIFO** in place of `w1_slave` makes a
read block for a controlled time, which is how the `conversionMs` fallback was verified; and
**`w1_master_add`** attaches a real family-0x28 slave with no hardware, which is how the udev rule
was verified.

## Requirements this path carries from the plan

These two came from the ndLouvers specification rather than from
`iSitePiLogger`. A third — an automatic session-start thermal sample — was implemented and
then **retired whole**; do not rebuild it, and read history §3.1 before concluding it is
missing by accident.

**The logger binds `temp0`–`temp3` itself, by discovery order, and persists the binding**
(owner decision, 2026-09-09). The owner plugs the four DS18B20s in one at a time, lowest channel
first; the logger notices each new `28-*` ROM ID and writes the binding to a store that survives
restarts. This replaces reading ROM IDs off a bench rig and typing them into a config by hand.
Eight things make it correct rather than merely convenient, and skipping any of them produces
silently mislabelled temperature data — which is worse than no data, because ΔT_preheat rests on
the *differences* between these four probes:
1. **`28-*` only.** The bare bus invents churning `00-*` phantoms (see the family-filter trap
   above). Binding "the next new device" without the family filter binds noise.
2. **Binding happens only in an explicit enrollment mode, never during a logging run.** A
   dropout and reconnect mid-session, or a probe replaced after a failure, must not silently
   re-bind a channel. Outside enrollment an unknown ROM ID is **invalid data and a logged
   event**, not a new channel.
3. **One probe per enrollment step.** If two unbound `28-*` IDs appear in the same 10 s scan the
   arrival order between them is unknowable — sysfs order is not arrival order — so the logger
   must refuse the ambiguous step and say so rather than guess.
4. **The binding carries provenance: the ROM ID, the channel and the bind timestamp**, and it
   lives in the `[thermal]` section of `KnurLogger.ini` beside the binary, as `temp<N>RomId` /
   `temp<N>BoundTaiUs` / `temp<N>BoundIso` (owner decision, 2026-09-10; history §2.7).
   **There is no `channels.ini` and no separate binding store — do not rebuild one.** One file
   holds the whole logger configuration, and the bindings sit in the same section as the
   slot-keyed offsets they invalidate when they change, which is where that consequence is
   visible rather than filed elsewhere. Two properties the machine-written file keeps:
   an empty `temp<N>RomId` is an unbound channel and a legitimate state, unlike a missing offset
   which is a startup failure; and a hand-typed ROM ID is guarded exactly as a scanned one is —
   the `28-*` family filter and the one-probe-one-channel rule both apply on load.
5. **Enrollment must report which ROM ID it just bound**, so the binding can be checked against
   the lead being plugged in. **The probes are already installed on the car** (owner,
   2026-09-09) and the owner can identify each lead at the logger end, so enrollment binds
   channel → location directly and the older "mark the probe body" step is moot. Provide an
   identification fallback anyway: with all four bound, **warming one probe by hand must be
   visible as one channel moving**, which confirms the map in situ and doubles as a liveness
   test.
6. **A BOUND channel whose ROM ID goes absent must be logged as present-but-invalid, never
   omitted** (owner, 2026-09-10). Iterating the four channels rather than the devices present on
   the bus is what produces it. If the record simply loses the channel, a mid-session dropout
   becomes indistinguishable from the logger not having run — which destroys the one property the
   SD file exists for, namely that a gap in the local stream is the only evidence a sample was
   missing rather than held. The sentinel goes out on `0x602` for it, and `reason` in the session
   record separates `unbound`, `absent`, `notAnswering`, `unparseable`, `crc`,
   `outOfRange`, `readFailed` and `cycleAbandoned`.
   **`absent` must not increment the read-error counter.** A dropped lead and a marginal bus send
   you to different parts of the car, and inflating `0x603` byte 2–3 with absences would bury the
   bus-quality signal it exists to carry.
7. **Every probe stays plugged in once it is in** (owner procedure, corrected 2026-09-10 after
   the first attempt at the car did the opposite; history §1.11). Unplugging each probe as the
   next goes in *binds correctly*, which is what makes it easy to get wrong, and costs three
   things binding does not: the four-probe star is never loaded, so plan thermal item 1's star
   acceptance criterion (closed rev 77, on a run with every probe connected) is untouched and the
   run proves nothing the ESP32 bench rig had not already proved with single probes; rule 5's warm-one-probe map check needs four live channels
   and becomes impossible; and every unplug leaves a ~100 s tail of a channel present in sysfs
   and answering with nothing.
8. **Enrollment must keep running after the fourth bind** (owner, 2026-09-10). Requirement 5's
   fallback check *is* "warm one probe and watch which channel moves", and that needs a logger
   still sampling and still notifying. An enroller that exits on the fourth bind silently
   removes the only in-situ verification of the map. It reports a fifth ROM ID once and refuses
   it; `--enroll --reset` is the way to start over.

**Per-channel offsets are hand-entered in `KnurLogger.ini` and ARE APPLIED to what goes to
RaceChrono** (owner decisions, 2026-09-10). Two earlier positions were reversed to get here —
"never auto-apply an offset" and ROM-ID keying in the store — so do not restore either from git
history; history §2.3 and §2.4 carry both, and §2.3 in particular states the one condition the
reversal rests on: **the session record carries the raw reading (`centiC`), the offset in force
(`offsetC`) and the value sent (`sentCentiC`) side by side, and anything that drops the raw value
re-opens the original objection.**
**The consequence of slot-keying, stated once because it is real:** an offset is a property of
one particular DS18B20, so if the probes are re-enrolled in a different order, or one is swapped,
the offsets stay with the slots and no longer describe the parts in them. **Re-check them after
any re-enrollment.** What it buys is that the offsets sit in the same `[thermal]` section as the
`temp<N>RomId` bindings that say which probe each slot holds (owner decision, 2026-09-10), so a
changed binding is visible three lines from the offset it invalidates. **The production
`KnurLogger.ini` is NOT in git** — `operations.md` carries the dev/production split — so
copying it back into the repo after a calibration is the only thing that version-controls it.
Five further properties:
1. **All four keys must be present.** `temp0OffsetC`..`temp3OffsetC` in `[thermal]`; a missing
   one is a startup failure rather than a silent zero, because an offset that quietly stopped
   being applied would be invisible in the data it corrupts. A non-numeric value and one past
   the bound also refuse to start.
2. **Two bounds, deliberately different in kind.** Past ±5 °C it warns about a likely
   decimal-point slip and applies the value anyway; past ±50 °C it refuses to start. A logger
   that will not start in the car over a plausible-but-large typo loses the whole session, which
   is worse than a wrong offset that is recorded and reversible.
3. **Only relative offsets mean anything.** There is no reference thermometer on the car, and
   ΔT_preheat depends on the four probes' differences rather than their absolute accuracy, so
   what belongs there is each probe's deviation from the four-probe mean at equilibrium.
4. **All four offsets are currently 0.0, and that is the RESULT** (owner, 2026-09-10). The
   in-situ cold soak measured a 0.193 K spread across the four probes, ~3 % of a ~6 K ΔT_preheat
   signal and inseparable from a real spatial gradient across four locations, so the owner's
   decision was to enter nothing. **Do not "fix" the zeros.** Plan open item 41 is closed on it.
5. **The result is clamped clear of `INT16_MIN`.** A corrected reading that landed exactly on the
   invalid sentinel would be indistinguishable from "no reading at all".
