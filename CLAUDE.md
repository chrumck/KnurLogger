The role of this file is to describe common mistakes and confusion points that agents might
encounter as they work in this project. If you ever encounter something here that surprises you,
alert the developer and record it in this file so the next agent does not hit it.

# CLAUDE.md — KnurLogger

**`CLAUDE.history.md` is the companion audit trail** — faults that were found and fixed, the
diagnostics that found them, decisions that were reversed, and requirements that were built and
then deliberately removed. Nothing in it is a live instruction. Read it when a statement here
surprises you, when you are about to reinstate something that looks missing, or when a figure you
remember from git history or an older transcript does not appear here. **The guards against
reinstating retired work are here; the reasoning behind them is there.** When a conclusion
changes, put the new one here and the superseded one there — do not leave "this used to say"
narrative in this file.

## Where authority lives

- **This repository owns software, host configuration and the box-2 hardware build sheet. It owns
  no measurement decision.**
  `../ndLouvers/CFD-Learning-Plan.md` Step 0b is the authority on channels, acceptance criteria,
  calibration and commissioning. **`Hardware/logger-perfboard-wiring.md` is the authority on
  wiring, I2C addresses, mux channel numbering and bring-up order** — its §3a net list
  specifically, against which §3, §4 and §6 are views. It lives here rather than in `ndLouvers`
  (owner decision, 2026-09-10) because it describes this box's own hardware, and **moving it
  changed nothing about what it may decide**: it is still subordinate to Step 0b, and a
  disagreement between the two is still resolved in the plan. Owning the build sheet is not owning
  a measurement decision.
- **Cross-repo, not cross-directory.** `ndLouvers` is a separate git repository that happens to
  sit alongside this one. Relative links between them work on disk and break on a git host. Do not
  "fix" them by copying content across; a duplicated requirement is a requirement that will drift.
  This repository has a public upstream at `github.com/chrumck/KnurLogger`, so every link that
  climbs out of it into `ndLouvers` — `../ndLouvers/...` from the root, `../../ndLouvers/...` from
  `Hardware/` and `SystemSetup/` — 404s there. That is accepted.
- **This repository is PUBLIC. Weigh that before writing host specifics into it.** It already
  carries the box's LAN IP, its username, its Bluetooth MAC and — since 2026-09-10 —
  `Hardware/logger-perfboard-wiring.md`, the full perfboard net list. That file was checked for
  host specifics before the move and carries none; it is component-level hardware detail, which is
  no more sensitive than the parts list of any hobby build. Nothing here is reachable from the
  internet (RFC1918 address, and the BT MAC is broadcast to anyone in range anyway), and no
  credential, key or Wi-Fi PSK is in the repo. **Git history is not retractable**, so the test for
  anything new is "would I mind this being permanent and public", not "is it useful now".
  **Measured-state notes are the ones that age badly**: accurate and useful when written, then a
  public status page for a host that may not have been hardened yet. History §1.6 is the worked
  example — it took a day to close, and the next might not.

## Naming

- **Channel names are positional and mean nothing.** Pressure channels are `P0`–`P5`, fixed by mux
  position. Thermal channels are `temp0`–`temp3`, fixed by DS18B20 ROM ID at enrollment. The
  mapping to measurement roles is a per-session record, logged at boot, and **a channel is never
  renamed after a role**.
  - **Pressure (`U`/`X`/`C`) is still deliberately undecided.** Do not invent one.
  - **Thermal is decided AND applied.** Installing the probes on the car decided it (owner,
    2026-09-09): enrolled in installed order, lowest first, it is **temp0 = `T_ambient`,
    temp1 = `T_core_in`, temp2 = `T_core_out`, temp3 = `T_aft`**. **All four are bound**
    (2026-09-10) — see the ROM IDs below. The plan owns the positions and the reasoning; this is a
    pointer, not a second copy.
- **`P` names a logger channel only.** The two pitot probes are `T1`/`T2`, never `P1`/`P2`.

## Hardware facts that surprise people

- **The sensor zone is ASSEMBLED, minus the pressure-sensor part** (owner, 2026-09-09) — the five
  SDP810s are still being delivered. **An empty I2C scan is therefore no longer the correct
  result**, and neither is zero `28-*` devices (history §2.6 for what the acceptance criteria used
  to say). Three facts from the assembled board matter before writing any bus code:
  1. **The BME280 is at `0x77`, and that is the specified address** (owner decision, 2026-09-09,
     amending the build sheet rather than the board). It is a genuine BME280 — chip-ID register
     `0xD0` reads `0x60`. So **`0x77` is the address to code against** and it is no longer free for
     anything else; **never strap a mux upward on this board.** No collision results, because the
     mux is strapped to `0x70` alone. History §1.7 has the discrepancy this resolved.
  2. **Whenever a bus device is silent, check the Pi side with `pinctrl get <n>` before suspecting
     the device.** It is one command and it splits the search space in half. This is what localised
     the mux's `~RESET` miswiring (history §1.1, since resoldered and answering at `0x70`).
     **Do not try to fix a reset problem in software** — the GPIO was already high, so no
     `pinctrl`/libgpiod write could have helped, and one would only have masked the diagnosis.
  3. **The mux is a PCA9548A, not a TCA9548A** — the board is marked PCA9548A (owner,
     2026-09-09). Functionally equivalent for everything here: same pinout, same `0x70`–`0x77`
     range, same single-control-byte channel register, same active-LOW `~RESET`. Recorded so that
     searching the board for a "TCA9548A" does not suggest the wrong part was fitted.
- **Testing the thermal channels requires a trip to the car, and the car has no network**
  (owner, 2026-09-09). The four DS18B20s are installed on the car; the car is in an underground
  garage with **no cell coverage and no internet**. So the logger is carried there, run, and
  brought back with artefacts to inspect. Five consequences:
  1. **There IS a way in: the owner's phone hotspot.** The owner can raise a local Wi-Fi network
     from the phone and SSH into the box — no internet, but a shell. So enrollment does not have
     to be blind. **The precondition is that the box already knows that SSID and its PSK**, since
     there is no other way to configure it there; a NetworkManager profile for the hotspot must
     exist *before* the trip, and no PSK goes in this public repository.
  2. **It must nonetheless survive unattended from power-on**, because the box is fed from the
     car's supply and *will* be power-cycled in the garage. A hand-started process does not
     survive that, which is why the systemd unit exists.
  3. **BLE is the richest feedback channel at the car**, and the only one that shows the data as
     RaceChrono will see it. A phone watching `temp0`–`temp3` move is what makes the "warm one
     probe by hand and watch which channel moves" identification check performable in situ.
  4. **Do not run the BLE link qualification with Wi-Fi up.** Wi-Fi and BT share one radio and one
     antenna on the BCM43455, and plan item 1b names background Wi-Fi scanning as a known source
     of BLE jitter. The hotspot is a debugging convenience, so **gate Wi-Fi off for any
     measurement of link quality** and bring it back up afterwards to collect artefacts.
     `rfkill block wifi`, never `rfkill block all`.
  5. **The pressure sensors have no such problem** — they sit on the perfboard and bench-test
     directly, once they arrive.
- **THE FIRST I2C TRANSFER AFTER AN IDLE BUS IS REFUSED, EVERY TIME, AND A RETRY FIXES IT**
  (measured 2026-09-10 against the BME280 at `0x77`). This is the single most expensive thing to
  not know on this board, because it presents as *the device is dead* and it is not.
  1. **The measurement.** With an idle gap of **10 ms or more the first `I2C_RDWR` fails every
     single time** — `EREMOTEIO`, a NAK. A second attempt **500 µs** later succeeded **60 of 60**
     across gaps of 50, 200 and 1000 ms. Back to back at 2 ms the first attempt mostly works
     (13/15). So it is a property of the idle bus, not of the device and not of the code.
  2. **A 1 Hz sampler hits it on every single cycle**, which is why the first BME280 reader
     written without a retry failed 100 % of the time and looked exactly like an absent part.
  3. **`i2cdetect` will tell you the device is fine while your program says it is not, and both
     are right.** `i2cdetect` and a shell loop of `i2ctransfer` issue transfers milliseconds
     apart, so they mostly stay inside the "bus is warm" window. **Do not conclude from a clean
     `i2cdetect` that a failing program has a bug in it** — that cost most of an afternoon.
     `i2ctransfer -y 1 w1@0x77 0xd0 r1` is the one-line check, and **run it several times**: a
     single result of either kind means nothing.
  4. **The retry lives in `i2cBus.cxx` and is COUNTED, not swallowed** — `firstAttemptFailures`,
     `recoveredTransfers` and `exhaustedTransfers` land in every `enclosure` record. About **one
     recovered transfer per sample cycle is the measured normal**; `i2cExhausted` moving off zero
     is the signal that the bus has actually degraded. A retry that hid this would have turned a
     hardware characteristic into folklore.
  5. **Ten attempts, not four, and the difference was measured.** A chain long enough for one
     isolated register read is not long enough for a cycle of eight transfers: at four attempts
     **2 of 64 cycles still lost every channel**; at ten, **64 of 64 and then 300 of 300
     over a five-minute run came back clean**, nothing exhausted. The attempts are free when
     unused — cycle read time was 15-16 ms either way.
  6. **This is unqualified hardware and it will apply to the five SDP810s too**, which sit on the
     same `SDA_MAIN`/`SCL_MAIN` behind the mux. The cause is not established — the main bus has
     no added pull-up (net list rows 10 and 11) and the BME280 breakout's own pull-ups are what
     the bus relies on. **Do not treat the retry as the answer to the physical question**; it is
     what makes the channel work while that question is open (`../ndLouvers/` open item 44).
- **All five SDP810s share one fixed I2C address (`0x25`) and cannot be strapped apart.** The mux
  is therefore mandatory, one sensor per channel. The mux does **not** pass pull-ups downstream, so
  every populated channel has its own pair.
- **The BME280's three quantities have three different consumers, and two of them are easy to
  point at the wrong thing.** `bme280Sensor.cxx` reads it and the field names carry the roles;
  `../ndLouvers/thermals-testing.md` §1.4 owns the reasoning and Step 0b owns the requirement.
  1. **Pressure is ENCLOSURE pressure, never a static reference.** The cavity is aerodynamically
     live. **Measured on the first drive: 156 Pa below stationary at a mean 116 km/h (Cp ≈ −0.25)**,
     — below the −0.5…−1.0 that had been estimated, but still 2–3× the 45–90 Pa
     measurands, and **not a single constant Cp**. Tolerable as a density term, disqualifying as a
     reference. Logged as `enclosurePressurePa`.
     **That first drive is the only QUALIFIED cavity measurement of it there is.** **A second cavity record exists from the third drive and is NOT a second Cp point** — right sign and order (−37 Pa mean at 60–100 km/h, r = −0.68) but the route's elevation change is the same order as the signal and the export carries no altitude channel (`../ndLouvers/thermals-testing.md` §3.6). The second drive's box was
     on the passenger seat, so its strongly speed-correlated pressure record is a **cabin** record;
     it was written up as a second Cp point and withdrawn. **This field is named for where the box
     is, not for where it was designed to be** — nothing in the session file says which.
  2. **Temperature is the CAVITY THERMOMETER** (plan item 1c), with Pi SoC temperature a
     cross-check rather than the primary proxy, and item 1d wants it recorded across a full
     session. **It is NOT the inlet density term** — that is `T_ambient`'s DS18B20, a probe in the
     air the car drives through. Logged as `cavityTemperatureC`.
  3. **Humidity is a seal and desiccant diagnostic** for items 1a and 1d, discarded by the
     dry-air approximation. No measurement consumer. Logged as `enclosureHumidityPct`.
  **It is read in forced mode at oversampling ×1 with the IIR filter off, and that is a
  measurement choice rather than a power one:** it is the datasheet's lowest-self-heating setting,
  and self-heating in the part that serves as the cavity thermometer is an error in the very
  quantity it exists to report. Higher oversampling would buy pressure noise this channel has no
  use for.
- **Packet IDs `0x600` and `0x601` are the BME280's, and they no longer mean what the ESP32 rig
  meant by them** (owner decision, 2026-09-10). On the rig they carried synthetic ramp and
  triangle test frames; that rig is spent and the owner released the IDs. **Any RaceChrono channel
  definition written against the rig's `0x600` must be re-entered** — the bytes decode to
  something else now. `0x602`–`0x604` are unchanged and still carry over.
- **A DS18B20 has no positional anchor.** Its 64-bit ROM ID *is* the channel definition, recorded
  once at enrollment. **All four are bound as of 2026-09-10** — `temp0` `28-06254385da1f`,
  `temp1` `28-0625424044b7`, `temp2` `28-062542ac86b6`, `temp3` `28-0625424e16c9` — in
  `~/bin/KnurLogger.ini` on the box, in `build/KnurLogger.ini` in git, and in
  `Hardware/logger-perfboard-wiring.md` §5a as the human record. **An earlier attempt the same day
  bound three in a different order and was discarded** (history §1.11); do not reconcile anything
  against it.
  **The channel → role map IS independently confirmed** (2026-09-10): warming each probe by hand
  in installed order moved `temp0`, `temp1`, `temp2`, `temp3` in that order with clean separation,
  +3.8 to +5.4 K each. Plan open item 42 is closed on it.
- **The `28-*` family filter is mandatory, and it can no longer be exercised on this box.** The
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
  5. **`therm_bulk_read` appears the moment a probe attaches, and it WORKS — since 2026-09-11**
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
        carries the `
`; **do not "tidy" it away.**
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
  6. **A probe pulled off the bus keeps its sysfs entry for ~100 s and answers with NOTHING, and
     that must not count as a read error** (measured at the car, 2026-09-10). The kernel unregisters
     a slave only after `w1_slave_ttl` (10) missed searches at `w1_master_timeout` (10 s), so for
     up to ~100 s after a lead comes out the directory is still there, `w1_slave` still reads, and
     the content carries neither `crc=` nor `t=`. That is a dropped lead, and the requirement below
     — "`absent` must not increment the read-error counter" — applies to it for exactly the same
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
  7. **The fake-sysfs harness CANNOT emulate an attribute whose read value differs from what was
     written**, and `therm_bulk_read` is exactly that: you write `trigger` and read back `-1`/`0`/
     `1`. In the fake tree it is a regular file, so it reads back `trigger` and the state machine
     above is unexercised. Two workarounds, both used: a **FIFO** stands in for a slow read, which
     is how the `conversionMs` fallback was verified; and **`w1_master_add`** attaches a real
     family-0x28 slave with no hardware, which is how the udev rule was verified. **The README's
     claim that the harness covers every branch but a real reading is therefore too strong** — it
     covers every branch that a plain file can represent.
  8. **The whole 1-Wire path IS testable without probes, and this is how** (2026-09-10).
     `unshare -Urm --map-root-user` gives an unprivileged user namespace with a private mount
     namespace, so a fake tree can be bind-mounted over `/sys/bus/w1/devices` — **no root, no sudo,
     no risk to the real box, and nothing to undo** since the namespace dies with the shell.
     Populate it with `w1_bus_master1/`, `28-…/w1_slave` files in the kernel's two-line
     `crc=xx YES` / `t=<millidegrees>` format, and a `00-…` entry to prove the family filter drops
     it. Enrollment order, the ambiguous-step refusal, the store's family and duplicate guards,
     bound-but-absent, CRC failure, the 85.00 °C default, out-of-range rejection and the
     application of a hand-entered offset were all verified this way. **Reach for this before
     concluding a sysfs-driven path is untestable.** What it does not establish: real bus timing,
     real conversion time, or `therm_bulk_read`.

## A SESSION FILENAME MEANS NOTHING, and `taiUs` can jump mid-session

**Measured on the first drive (2026-09-10): the box's clock ran 3 h 11 min behind, and
`systemd-timesyncd` stepped it forward the moment the box regained the network.** The step lands
*inside* a session file. Confirmed from the straddling record pair: `taiUs` jumped **+11,484 s**
while `bootUs` moved **+1.0 s**.

1. **A jump in `taiUs` is a clock step until proven otherwise, not a gap in recording.** Check
   `bootUs` across the same pair. If `bootUs` moved a second and `taiUs` moved hours, nothing was
   missed and no records were lost. Reading it the other way makes a complete session look like a
   dead logger.
2. **Every car session's filename is wrong**, because the name is stamped from that same clock.
   Sorting or selecting sessions by filename picks the wrong file — that happened during this
   analysis, and a session was analysed, then wrongly disowned, then re-confirmed.
3. **Each session is its own boot, so the offset cannot be carried between them.** `bootUs` starts
   near zero in every car session. The offset is whatever real time elapsed since the last bench
   sync, so it differs per boot and there is no single correction to apply.
4. **`bootUs` and `sessionUs` are the trustworthy axes and this is what they are for.** Intra-
   session timing is exact across the step. The plan's item 4 requires the dual clock precisely
   so a stepped wall clock costs nothing, and that requirement earned itself here.
5. **To align against RaceChrono, use the cycle counter if the link was up, and physics if it was
   not.** `0x601` and `0x603` bytes 4–5 carry the worker's per-cycle counter, and the **same
   counter is the `cycle` field of every `enclosure` and `temp` record** — so a RaceChrono recording
   of either channel joins to the session file on cycle number and the alignment is exact, with no
   fitting and no lag. Verified 2026-09-11: logger `sessionUs 166.8` ↔ phone `t = 7.3 s`.
   **The fallback, for a session with no usable link:** cross-correlate cavity
   temperature against GPS speed low-passed at ~180 s — the cavity tracks airflow at **r = −0.97**.
   **Require a large overlap**: an unconstrained search returns a spurious near-perfect fit on a
   few dozen bins at the edge of the window, which it did here before the constraint was added.
6. **Do not "fix" this with a GPS-time fetch** — that is retired, and history §2.5 says so.

## NEVER return an ATT error from the filter callback

**RaceChrono asks EVERY DIY device for the union of ALL packet IDs it has channel definitions
for, not just the ones that device publishes** (measured 2026-09-10). Box 2 is asked for box 1's
CAN frames. This is the single most expensive fault this project has had: it cost a road test.

1. **What the subscription actually looks like.** Entering the logging regime, RaceChrono writes
   `deny all` and then one `allow single` per configured channel, as a burst inside one second.
   Measured verbatim, in this order: `0x7F0`, `0x420`, `0x600`, `0x601`, `0x202`, `0x602`,
   `0x603`, `0x78`, `0x4FA`. **Five of those nine are box 1's, and `0x420` is the CAN ambient
   frame the plan names.** In CAN-bus test mode it writes `allow all` instead, which involves no
   packet IDs at all — which is exactly why test mode worked and logging mode did not.
2. **Answering any one of them with `BLUEZ_ERROR_REJECTED` loses the whole burst.** The first
   command was `0x7F0`, which this logger does not publish; the old code refused it, RaceChrono
   abandoned the rest of the sequence, and `deny all` had already destroyed every notify timer.
   Result: a permanently frozen frame set on the phone and a logger that looked connected and
   healthy. **An unknown packet ID is normal traffic, not an error** — log it and ignore it.
3. **The burst order varies, which is what made the symptom confusing.** Whatever was subscribed
   before the first unknown ID kept updating, so the phone showed a full set, a partial set, or
   nothing depending on ordering. "Most of the data points missing" and "one set then frozen" are
   the same fault.
4. **`0x604` is only notified if a channel is DEFINED for it.** With the filter honoured, a packet
   nobody subscribes to is never sent — correct behaviour. Measured on the bench: `supply` took
   zero notifies through a 57 s subscription while the other four ran at ~1 Hz; **measured again in
   the car on 2026-09-11**, where RaceChrono's burst never asked for `0x604` at all and the logger
   recorded `"supply": 0`. **Defined on the phone 2026-09-11 and CONFIRMED ON THE AIR the same
   evening** — all six of its channels decoded in the third drive's recording, the frame's first
   appearance ever. `../ndLouvers/` open item 45, closed.
   **`0x604` byte 7 is not the only liveness channel, and saying so cost nothing only by luck.**
   `0x601` and `0x603` bytes 4–5 are per-cycle counters with exactly the same property — they
   advance whatever the sensors report — and on the second road test the whole
   dropped-notification measurement was made off them with `0x604` absent throughout. Define
   `0x604` for the supply telemetry, not because nothing else can prove the link is alive.
5. **Do not tighten the command-length checks back to equality.** They were `== 1`/`== 3`/`== 7`
   and every observed command matched exactly, so that was never the fault — but the reference
   implementation uses minima and a future field would break equality for no benefit.
6. **KnurDash had the same defect, it WAS hitting it, and it is now fixed there too**
   (`github.com/chrumck/KnurDash`, 2026-09-11). Its `onCharWrite` also returned
   `BLUEZ_ERROR_REJECTED` for a frame ID it does not carry, and **adding box 2's channels to
   RaceChrono is what broke box 1** — before `0x600`/`0x601` existed every ID in the burst was one
   KnurDash publishes, so nothing was ever refused. Applying the captured burst to its frames
   (`0x7F0` ADC, `0x78`, `0x86`, `0x202`, `0x420`, `0x4FA`) predicts `0x7F0` and `0x420` live and
   `0x202`, `0x78`, `0x4FA` frozen — **and the owner confirmed exactly that from the car, some
   readouts live and some frozen.** Two things to carry from it:
   1. **A fault in one box can be caused by a channel added for the other.** The two boxes share
      one RaceChrono channel set, so they are not as independent as two repositories suggest.
   2. **Its D-Bus/context ordering was deliberately left alone**, and its commit says so. That
      ordering is broken in a headless program and box 2 had to fix it, but KnurDash's `main.c`
      calls `gtk_main()`, which iterates the global default context, so its adapter callbacks
      work. **Do not "fix" KnurDash by copying this repository.**
   **Its fix is now confirmed on a phone** (2026-09-11): with both boxes connected, box 1 was asked
   for box 2's four IDs, refused none of them, and kept **all twelve of its channels alive to the
   last sample** of the recording. It had been compiled and committed but untestable on the bench,
   the CAN hardware being in the car.
7. **The fix is confirmed on box 2 in the field too** (2026-09-11). The burst arrived as
   `deny all` then nine `allow single` inside 400 ms — `0x7F0`, `0x420`, `0x600`, `0x601`, `0x202`,
   `0x602`, `0x603`, `0x78`, `0x4FA` — five of which this logger does not publish and all five of
   which were logged and ignored. 141/141/139/139 notifications went out on
   `0x602`/`0x603`/`0x600`/`0x601` over 140 s connected, and **both per-cycle counters advanced by
   exactly +1 across every sample the phone recorded: zero drops.** `../ndLouvers/thermals-testing.md`
   §3.7 owns the numbers. **That drive's box was on the passenger seat**, so it proves the code and
   nothing about the installed link — ~0.5 m of cabin air to the phone is the best case there is.
   **The third drive, the same evening, WAS in the cavity with the enclosure closed** (owner): 415 s,
   box 1 connected simultaneously, and all three free-running counters — `0x603` and `0x601` bytes
   4–5 and `0x604` byte 7 — stepping exactly +1 on every sample. `../ndLouvers/thermals-testing.md`
   §3.8. **The cavity is not a link problem**; what plan item 5a still asks for is sustained speed,
   a steering-lock and suspension-travel sweep, and a recorded seat occupancy.

## THE PHONE'S CHANNEL LIST IS PART OF THE INSTRUMENT AND THIS CODE CANNOT CHECK IT

**Three RaceChrono channel definitions were found wrong or missing on 2026-09-11, by decoding a
recording against the session file of the same samples.** Nothing in this repository can detect any
of them — the logger's own record is correct in every case — so **a "the logger is sending it"
argument is never an answer to "the phone is showing the wrong number".**

1. **Signedness is the dangerous one, and it hides.** One of `0x602`'s four thermal channels is
   defined `bytesToUint` where `bytesToInt` belongs. A positive temperature decodes identically
   either way, so **the fault is invisible in normal data** and shows only on the `−32768` sentinel
   (as **+327.68** instead of −327.68) or on a sub-zero ambient, which reads ~+655 °C. By field
   order it is the second of the four, the `temp1` slot.
2. **The sentinel check is not a one-off.** "All four read −327.68, therefore the definitions are
   right" was true on 2026-09-09 and false by 2026-09-11 with no code change — the channel list is
   hand-edited. **Re-run it after any edit**, and note it cannot be run at all with probes
   attached.
3. **A missing definition costs the whole packet**, silently, because the filter is honoured.
   That is how `0x604` went unsent for two road tests.

**All three were fixed on the phone on 2026-09-11 and ALL THREE ARE NOW OBSERVED WORKING** —
`0x604` and `0x601` on the air in the third drive's recording (`0x604`'s first appearance ever;
`0x601`'s check values land at 31 and 96), and `Temperature Front 2` by the owner with the sensor
disconnected, where it reads negative. `../ndLouvers/` open item 45 is closed on that.
**The three rules above survive the close** — they are about the fault class, which is permanent,
not about these three instances. **Note which check proved which:** the drive proved two and was
structurally incapable of proving the third, because every reading in it was 15–46 °C, where both
decodes agree exactly.

### Audit the phone's channel list from an export, without the phone

**A RaceChrono `.rcz` names every channel slot that was in force when it recorded**, because a
channel's sample file is named after its numeric id and that id is
`slot * 2**20 + channelType` — `Digital` 70537, `Temperature` 70539, `Pressure` 70541,
`Percent` 70547 being the four types this logger uses. So the phone's list is recoverable from any
session anyone exported, and the recorded values then say whether each equation was typed right.
`Tools/rcz-channels.py` does it:

```bash
python3 Tools/rcz-channels.py session.rcz
```

1. **This is how the unsigned channel was found**, and it is the only way it could have been: the
   tool flags a `Temperature` slot carrying **+327.68**, which is the `−32768` sentinel decoded
   unsigned. It also flags a channel whose samples are all `NaN` — a defined channel that never
   produced a value, which is what two of KnurDash's turned out to be.
2. **The slot numbers in an old export are not the slot numbers in force now.** `0x601`'s five
   were renumbered from 50–53 to 51–55 on 2026-09-11. A disagreement between a recording and
   `README.md`'s slot map means the list changed, not that either is wrong.
3. **Box 1's channels come back as bare type numbers**, because they sit on RaceChrono's standard
   channel types rather than the four DIY ones. Enough to tell them apart and to spot a dead one.
4. **The profile itself can be exported and is now committed**, at `RaceChrono/vehicleProfile.json`
   — the equations as entered, for both boxes, with `localUuid` stripped. Checking it against this
   README found that **`0x600`'s pressure channel carries a `/1000` the README did not document**
   — RaceChrono's Pressure channel takes kPa and that gauge displays bar — which is how a spec and
   a deployment drift without anyone noticing.
5. **AN ALL-EMPTY CHANNEL IS NOT NECESSARILY A BROKEN ONE.** RaceChrono renders a deliberate
   out-of-range marker as no value at all, so a channel that is `NaN` in every sample can be a
   sensor that sat outside its calibration range for the whole session. **Box 1's two are exactly
   that** (owner, 2026-09-11) — `lowPass(E,254)` and `lowPass(F,254)` on `0x7F0`, working as
   designed. The tool reports them and no longer calls them faults. **Do not "fix" a channel on
   this evidence alone**; ask what the sensor was doing.
6. **RaceChrono's parser is case-insensitive** — `bytesToUint`, `bytesTouInt`, `bytestouint` and
   `bytesToUInt` all appear in the profile and all work. **Do not normalise the casing**: there is
   nothing to fix, and `bytestoint` differs from `bytestouint` by the single letter that decides
   signed against unsigned.

## The BLE worker needs its own main context BEFORE the D-Bus connection

**`g_main_context_push_thread_default()` must come before `g_bus_get_sync()` and
`binc_adapter_get_default()`, and it did not until 2026-09-10.** GDBus binds each signal
subscription to whatever context is thread-default when the subscription is made, so an adapter
created first subscribes against the global default context — and **nothing in this process
iterates that context**, `main()` being a plain thread-join.

1. **The symptom is silence, not an error.** `onPoweredStateChanged` and `onCentralStateChanged`
   simply never fire. **Every session file written before 2026-09-10 is missing its BLE connect
   and disconnect events for this reason, not because nothing connected** — do not read those
   files as evidence that no central ever attached.
2. **This is where KnurDash diverges and why copying its order was wrong.** KnurDash is a GTK app
   whose `main.c` calls `gtk_main()`, which iterates the global default context on the main
   thread, so its adapter callbacks fire despite the same ordering. A headless logger has no such
   loop. **This is the one place where following KnurDash's shape was actively incorrect.**
3. **What it broke, beyond the missing records:** on disconnect neither `isNotifying` was cleared
   nor advertising restarted, so a link lost without an explicit unsubscribe left the logger
   invisible until the process restarted.

## When BLE will not advertise, start here

The platform has already been the culprit once and the logger looked guilty (history §1.2), so
**do this before reading `raceChronoBle.cxx`:**

1. **`bluetoothctl advertise on`.** If that fails too, the fault is not in this repository and no
   amount of reading `raceChronoBle.cxx` will find it. This single command separates "our code"
   from "the platform" and it should always be step one.
2. **`journalctl -u bluetooth`** for the `bluetoothd`-side reason, which is more specific than the
   D-Bus error the client sees.
3. **`btmgmt add-adv -c -u 1ff8 1`** (root). This uses the *legacy* MGMT path, so success here
   proves the controller and kernel can advertise and narrows the fault to `bluetoothd`.
4. **`btmon` while triggering an attempt** (root) — the only thing that shows an actual malformed
   command. `btmon -w file` then `btmon -r file`.

- **`SupportedInstances` and `ActiveInstances` on `org.bluez.LEAdvertisingManager1`** are the quick
  check that an advertisement actually registered. `ActiveInstances: 1` while the logger runs is
  the acceptance; `0` means it silently did not.
- **Known-good versions: kernel `6.18.39`, `bluez 5.82-1.1+rpt2`, `firmware-brcm80211
  1:20260519`.** A regression to `bluez 5.82-1.1+rpt1` on kernel `6.18.34` cannot advertise at all;
  read history §1.2 before debugging anything else.

## The box

- **SSH alias `KnurLogger`** — `192.168.118.52`, user `chrum`, key-only.
- **Raspberry Pi OS Lite 64-bit, Trixie**, kernel `6.18.39`, Pi 4B Rev 1.5, 4 GB.
  `/boot/firmware/config.txt` is the boot config path. NetworkManager is the network stack.
- **There is no `hciuart.service` on this image.** The BCM43455 is attached by udev and
  `bluetooth.service` is the only unit involved. Recipes that name `hciuart` predate this.
- **`wpa_supplicant.service` is enabled and running, and NetworkManager drives it over D-Bus.**
  Disabling it because "NetworkManager spawns its own" loses Wi-Fi, which is the only way onto a
  box in a wheel-well cavity. An early `harden-headless.sh` draft did exactly that (history §1.8).
- **Swap is zram (`/dev/zram0`), not a file.** It is RAM-backed and costs no SD wear, so there is
  nothing to gain by turning it off. `dphys-swapfile` does not exist here. The one thing worth
  retiring is `rpi-zram-writeback.timer`, whose job is to push zram pages onto the card.
- **I2C needs a module that nothing loads; 1-Wire does not.** `dtparam=i2c_arm=on` registers the
  adapter but does **not** create `/dev/i2c-1` — the `i2c-dev` module does, and `/etc/modules` is
  empty with no modalias path to pull it in. `raspi-config`'s `do_i2c` does both steps and any
  script replacing it must too; `harden-headless.sh` writes
  `/etc/modules-load.d/knurlogger.conf`. 1-Wire has no equivalent gap because `w1_therm` carries
  the alias `w1-family-0x28`. **A missing `/dev/i2c-1` after a reboot means one of the two halves
  did not take** — it is never "the sensors are not built yet", which only ever explained an
  *empty scan*. Both halves took on 2026-09-09 and `/dev/i2c-1` exists.
  **`/dev/i2c-20` and `/dev/i2c-21` exist too and are not yours** — they are the VC4 display DDC
  buses, which `i2c-dev` exposes against adapters the KMS driver registers. The perfboard is on
  **bus 1** and nothing else. History §1.9 is how that distinction diagnosed a missing bus 1.
- **`w1-gpio`'s `pullup` parameter is ignored** on this firmware — the overlays README says so
  outright. The overlay that drives an external strong pullup is a different one,
  `w1-gpio-pullup`, and this build must not use it: `R11` is a plain 2.2 kΩ resistor to 3V3.
- **The box is fed from CONSTANT 12 V, not the accessory circuit** (owner, as built,
  2026-09-10 — history §2.2 for the accessory-feed assumption this replaced). Four things follow.
  1. **The logger runs the whole day; the fuse is the off switch.** Fitted in the morning, pulled
     at the end. So it is powered through engine-off periods and one session file spans the day.
  2. **`SystemSetup/KnurLogger.service` is REQUIRED, not convenient.** There is no ignition
     event to hand-start the logger around, and the paddock has no network but a phone hotspot, so
     without the unit the owner must SSH in every morning to start it by hand.
  3. **The box is powered during cranking**, which it never was before. The HW-384's 6 V floor is
     well below a normal dip so this should be benign, but build sheet §10 step 3's crank watch was
     bypassed rather than passed. A crank brownout shows up as a latched undervoltage bit, or as
     the session file splitting with a fresh `session` record if the Pi rebooted.
  4. **Battery drain is a new failure mode.** Estimated ~275 mA at 12 V — ~3.3 Ah over a 12 h day
     against the ND's ~45 Ah, which is comfortable, but **~46 Ah over a week with the fuse left
     in, i.e. a flat battery.** Estimated, not measured; plan item 5.3 still owes the real figure.
  **The ~1 s `fsync` requirement is unchanged — only its trigger moved.** A hard cut is the fuse
  being pulled, or a cranking dip. Repeated hard cuts are the durability risk worth knowing: one
  costs at most the last second, but doing it daily for a season is the classic route to a corrupt
  SD card, so `sudo poweroff` before pulling the fuse is free insurance.
  **Both road-test sessions so far ended in a hard cut** — no `BLE stopped` event, no closing
  record, the file simply stops. That is how to recognise one when reading a session back, and it
  is the practice the note above is asking to change.
- **Two logger instances run happily side by side and BOTH advertise — nothing refuses, nothing
  warns** (measured 2026-09-10: `SupportedInstances` is 5, `ActiveInstances` went 1 → 2 with a
  log-mode and an `--enroll` instance up together). **`KnurLogger.service` is now installed and
  enabled, so the logger is ALREADY RUNNING whenever the box is powered** — which turns this from
  a thing to remember before enrolling into a thing to remember before running the binary by hand
  at all. **`sudo systemctl stop KnurLogger` first, every time.** No single-instance guard exists
  — the owner declined one as not worth the code for a one-shot job — so this is discipline, and
  the reasons are worth knowing:
  1. **Both poll the 1-Wire bus, each read triggering its own conversion**, which roughly doubles
     cycle time. That silently corrupts any timing measurement — open item 43 is a timing
     measurement — and adds bus load that can manufacture read errors on a bus that is fine.
  2. **Two advertisements share the name and the service UUID**, so RaceChrono connects to one and
     you cannot tell which. Now that all four channels are bound both instances read the same
     probes and show the same temperatures, so this is no longer the hazard it was while nothing
     was bound — but it still means you do not know which process the phone is talking to.
  3. **A log-mode instance never sees new bindings.** It reads the bindings out of
     `KnurLogger.ini` once at worker start and never re-reads it; only `--enroll` writes them. So
     it must be restarted after enrolling regardless.
  Nothing corrupts: session files carry a mode tag so they never collide, and the store write is
  temp-file plus rename plus directory fsync.
- **The logger is BUILT in `~/KnurLogger/build/` and RUN from `~/bin/`, and those are two
  different `KnurLogger.ini` files** (owner decision, 2026-09-10 — history §2.8 for the single-copy
  arrangement it replaced). `~/KnurLogger/build/KnurLogger.ini` is the git-tracked **seed and backup** — it carries whatever
  was last copied back, which since 2026-09-10 is the four enrolled ROM IDs;
  `~/bin/KnurLogger.ini` is the **production** file carrying the real offsets and the real ROM ID
  bindings. `SystemSetup/deploy-logger.sh` always replaces the binary and only ever *creates* the
  `.ini`, never updates it, and `KnurLogger.service` points at `/home/chrum/bin/KnurLogger`.
  1. **This is what makes the tar-over-ssh loop safe.** It overwrites everything under
     `~/KnurLogger`, which used to mean one sync silently destroyed an offset typed in at the car.
     Now it overwrites a template nothing reads.
  2. **The production `.ini` is not in git, so nothing backs it up.** After enrolling or
     calibrating, `scp KnurLogger:bin/KnurLogger.ini build/KnurLogger.ini` and commit — a
     deliberate act, and the only thing between a wiped card and another trip to the car.
  3. **Do not point anything at the build tree** — not the service, not a cron entry, not a
     runbook step. A binary run from `~/KnurLogger/build/` reads the template's empty bindings and
     logs four unbound channels while the real ones sit in `~/bin/`.
- **Never write `KnurLogger.ini` through GLib's key-file serialiser.** `g_key_file_to_data()`
  re-encodes comments and destroys every non-ASCII character in them — measured 2026-09-10, the
  em-dashes in that file's header came back as `?`, one silent corruption per enrollment. The file
  is hand-maintained and its comment block is the most useful documentation in this repository, so
  `saveChannelStore()` rewrites it **line by line**: only the twelve binding lines are touched and
  every other byte is copied through. GKeyFile is still the right tool for *reading* it.
- **`sudo` requires a password.** Pre-flight runs pipe fine over `ssh host 'bash -s'`; anything
  that changes state must run from a login shell (`ssh -t`), and every mutating script checks this
  up front rather than failing halfway.
- **`/usr/sbin` is not on `PATH`** for a non-interactive SSH session or a non-root Debian login
  shell. `sysctl`, `rfkill`, `swapon` and `i2cdetect` all live there, so `command -v rfkill`
  answers "missing" on a box where rfkill is installed. Every script in `SystemSetup/` prepends
  the sbin directories; do the same in anything new, and distrust any "tool missing" result that
  has not accounted for this.
- **`i2c-tools`, `cmake`, `git` and `libglib2.0-dev` are installed** as of 2026-09-09, by
  `SystemSetup/install-dependencies.sh --execute`. `rfkill`, `build-essential`, `nmcli` and
  `bluetoothctl` were already present. **A pre-flight package list does not bound what apt will
  change** — history §1.10.
- **The Bluetooth soft block is CLEARED and survived a reboot** — `Soft blocked: no`, `hciconfig`
  `UP RUNNING`, BlueZ `Powered: yes` / `PowerState: on`. It *shipped* blocked, and history §1.3 is
  why that state cannot be undone from `bluetoothctl`.
- **Some channels the plan needs will never appear on box 2's SD card.** CAN ambient
  (`0x420` byte 7) and, if it is on the bus, **cooling-fan state** arrive through box 1's CAN
  broadcast into RaceChrono — not through this logger (`../ndLouvers/` §7 open item 35). So the
  record for a session is **split across two devices, and RaceChrono is what reassembles it** —
  which is the whole reason BLE is the primary data path. The consequence: **a box-2 channel that
  never reaches the phone cannot be aligned to the fan state that explains it**, and the fan can
  change state mid-run with no driver input and no speed change. So a BLE gap is not a cosmetic
  loss — it is the loss of the only link between box 2's pressures and the fan behaviour that
  conditions them. (It is *not* reassembled by a session-start time offset; history §2.5.)
- **`fake-hwclock` is not installed, and the clock in the car will be wrong.** A Pi 4B has no RTC.
  `systemd-timesyncd` saves the time to `/var/lib/systemd/timesync/clock` and restores it at boot,
  so a session file is never stamped 1970 — but with no NTP in the car the clock simply resumes
  from the last bench sync and is **wrong by however long ago that was**, while looking perfectly
  plausible. **The car has no network at all**, so there is no NTP to reach even in principle.
  **Do not build a GPS-time fetch to fix this** (history §2.5). What the logger does instead is
  cheap and sufficient: **every local record carries elapsed-since-boot alongside the wall
  clock**, so intra-session timing is exact regardless of what the absolute epoch says.
- **`network-online.target` can no longer be reached** once `harden-headless.sh` masks
  `NetworkManager-wait-online.service`. Nothing needs it today — only cloud-init did, and that is
  disabled too. But the KnurLogger service must **not** use `Wants=`/`After=network-online.target`;
  it would wait on a target that never comes up. `After=multi-user.target` and the box's own
  readiness checks are the right shape. Verified: every unit this script masks is `WantedBy`
  something and `RequiredBy` nothing, so masking breaks no dependency chain.

## Never disable

- **Bluetooth.** BLE is the RaceChrono link and the reason this box exists. `iSitePiLogger`'s
  `setupNotes.txt` sets `dtoverlay=disable-bt` and is otherwise this project's model — that one
  line is not to be copied.
- **Wi-Fi, permanently.** `dtoverlay=disable-wifi` needs an SD card and a text editor to undo.
  Gate it per session with `rfkill block wifi` or `nmcli radio wifi off` instead — **never
  `rfkill block all`**, which takes BLE down with it and persists across reboots, and cannot then
  be cleared from `bluetoothctl` (history §1.3). Whether Wi-Fi needs gating at all is plan item
  5a's installed link check to answer, and that check has not been run.

## Scripts

- **Every script in `SystemSetup/` defaults to pre-flight and needs `--execute`.** This is the
  `iSitePiLogger` `prepare-image.sh` idiom and it is deliberate. Run with no arguments, read the
  `+` lines, then re-run.
- **`harden-headless.sh` masks rather than disables.** apt's timers re-enable themselves on
  package upgrade. `sudo systemctl unmask <unit>` is the rollback.
- **`disable --now` is not a way to stop a unit that has no `[Install]` section.** systemd refuses
  the whole command, so the `--now` half never runs, and masking does not stop a running unit
  either. `retire_unit` therefore issues `disable`, `stop` and `mask` as three separate steps,
  tolerating the first two. Twelve units on this box are active when it runs.
- **`ssh-harden.sh` must not be run under `sudo`** — it reads `$HOME/.ssh/authorized_keys`, and as
  root that is the wrong file entirely. It refuses, but do not work around it.

## Architecture

Follow `iSitePiLogger`, which is the structural model:

- **Single translation unit.** All `.cxx` files are `#include`-d into `main.cxx`. Do not add them
  to `CMakeLists.txt` as independent targets.
- **`initialiseBlePackets()` is called exactly once, from `main`, before any worker starts.**
  Initialise shared packet state in `main`, never in a worker — history §1.5 is the latent bug that
  made this a rule.
- **Procedural workers, no OOP.** Plain `gpointer fn(gpointer)` passed to `g_thread_new()`.
- **`CLOCK_TAI` throughout**, to avoid leap-second discontinuities in sample timestamps and file
  names.
- **The `.ini` sits beside the binary** and its path is resolved from `/proc/self/exe`, not the
  working directory.

Four requirements came from the plan rather than from iSitePiLogger. A fifth — an automatic
session-start thermal sample — was implemented and then **retired whole**; do not rebuild it, and
read history §3.1 before concluding it is missing by accident.

- **BLE is the primary data path; the SD card is the durable raw and diagnostic record**
  (owner decision, 2026-09-09, item 5b — this **reverses** the earlier "SD primary, BLE
  secondary", so do not reinstate that from memory or from git history; history §2.1). The analysis
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
- **Append-only session file, `fsync` on a fixed ~1 s cadence** — not per sample, not only at
  close. The supply vanishes without warning — the fuse pulled at the end of the day, or a
  cranking dip — so the last durable write bounds the loss. Flushing per sample at 10 Hz buys a
  shorter window at the price of write amplification without changing the failure mode.
- **The logger binds `temp0`–`temp3` itself, by discovery order, and persists the binding**
  (owner decision, 2026-09-09). The owner plugs the four DS18B20s in one at a time, lowest channel
  first; the logger notices each new `28-*` ROM ID and writes the binding to a store that survives
  restarts. This replaces reading ROM IDs off a bench rig and typing them into a config by hand.
  Seven things make it correct rather than merely convenient, and skipping any of them produces
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
     record separates `unbound`, `absent`, `crc`, `powerOnDefault`, `outOfRange` and `readFailed`.
     **`absent` must not increment the read-error counter.** A dropped lead and a marginal bus send
     you to different parts of the car, and inflating `0x603` byte 2–3 with absences would bury the
     bus-quality signal it exists to carry.
  7. **Every probe stays plugged in once it is in** (owner procedure, corrected 2026-09-10 after
     the first attempt at the car did the opposite; history §1.11). Unplugging each probe as the
     next goes in *binds correctly*, which is what makes it easy to get wrong, and costs three
     things binding does not: the four-probe star is never loaded, so plan thermal item 1's open
     acceptance criterion is untouched and the run proves nothing the ESP32 bench rig had not
     already proved with single probes; rule 5's warm-one-probe map check needs four live channels
     and becomes impossible; and every unplug leaves a ~100 s tail of a channel present in sysfs
     and answering with nothing.
  8. **Enrollment must keep running after the fourth bind** (owner, 2026-09-10). Requirement 5's
     fallback check *is* "warm one probe and watch which channel moves", and that needs a logger
     still sampling and still notifying. An enroller that exits on the fourth bind silently
     removes the only in-situ verification of the map. It reports a fifth ROM ID once and refuses
     it; `--enroll --reset` is the way to start over.
- **Per-channel offsets are hand-entered in `KnurLogger.ini` and ARE APPLIED to what goes to
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
  `KnurLogger.ini` is NOT in git** — see the dev/production split below — so copying it back into
  the repo after a calibration is the only thing that version-controls it.
  Four further properties, all verified 2026-09-10:
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
  4. **The result is clamped clear of `INT16_MIN`.** A corrected reading that landed exactly on the
     invalid sentinel would be indistinguishable from "no reading at all".
- **Supply health is logged telemetry, read after a run** (owner decision, 2026-09-09), standing
  in for a bench instrument on commissioning item 5.7 — **not** for build sheet §10 step 2's
  meter. What to record, and the traps:
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
