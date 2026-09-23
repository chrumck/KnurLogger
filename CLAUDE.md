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
- **`3DPrinting/` holds every printed part for both boxes** (moved here from `ndLouvers`
  2026-09-20, which now has none). Enclosure, DS18B20 stalks, boom tip, and the
  **calibration bell** for the pressure ladder — `calibrationBell.md` with `calibrationBell.svg`
  and `calibrationBellWeighing.svg`. **The bell is the ADOPTED reference route** (owner,
  2026-09-20), and the same rule applies to it as to the build sheet: living here lets it own the
  fixture's geometry and constants, and lets it own no measurement decision. **Adoption did not
  move that boundary** — what accuracy is demanded of it still belongs to `../ndLouvers/` Step 0b
  and `pressure-testing.md` §2.2. **IT IS NOW BUILT AND HAS DELIVERED PRESSURE** (2026-09-20) —
  79 g, 53 minutes on `P0` at 26–32 Pa, `k_d` measured to 0.5 % of the as-built figure
  (`../ndLouvers/pressure-testing.history.md` §3.3). **Two traps came out of that and both live in
  `calibrationBell.md` §"As built":** the bell that exists is **not** the rev F bell drawn — ID
  150.00 and wall 0.90 against 148.4/0.80, so **every nominal constant in that file is wrong for
  it, `k_d` by 23 %** — and the **bare bell is metacentrically unstable** across its whole
  reachable depth range, having capsized twice and lolled once on the bench, which contradicts that
  file's own "+0.041 N·m/rad, bare, stable" row.
  **A third trap came the same evening:** a leak test with the full line but no sensor showed **zero
  sinking in 10 minutes**, so the whole bleed is the sensor's own bypass and the tubing's loss is a
  first-order term. **What eleven bell sessions then found is restated once in
  `../ndLouvers/pressure-testing.md` §3.3 (rev 113; sessions in `../ndLouvers/pressure-testing.history.md` §3.3–§3.14), which owns the figures:** the bench tubing
  loses 8.2 → 3.8 % of the bell's pressure across 38–386 Pa; the 8.6 m extension is 3.1 × 10⁷; and
  **all five SDP810s, read against the same bell on 2026-09-22/23, are inside their ±3 % spec** —
  `P0` −1.03, `P1` −0.91, `P2` −1.53, `P3` −1.70, `P4` +0.50 %, a 2.2 pp spread.
  > **⚠ THE EARLIER "THREE SENSORS SPREAD TEN PERCENT, `P1` IS OUT OF SPEC, AND THE BELL IS THE
  > BEST ABSOLUTE IN THE ROOM" IS RETIRED — do not restore it.** Those figures were computed with a
  > `k_m` that assumed a perfectly circular bore (it is 1.6 % oval) and **with no barometric term at
  > all**. The bell's `A_eff` is now measured with water at 17 392 mm², `k_m` = **0.5639** and `k_d`
  > = **0.2367**; `calibrationBell.md` §"As built" is the record.
  **AN SDP810 IS A THERMAL MASS-FLOW DEVICE AND ITS READING SCALES WITH ABSOLUTE PRESSURE.**
  Datasheet §2.1 footnote 1 calibrates at **966 mbar**; at the bench's 1006.5 that is **+4.19 %**,
  and across ordinary weather 990 → 1030 mbar is +2.5 → +6.6 %. This is the largest single term in
  the chain and it was missing from every figure before 2026-09-22.
  **`0x3615` is the RIGHT command** — datasheet §6.3.1 lists it as *differential pressure*
  temperature compensation, not the mass-flow variant — so **do not switch to `0x3603`/`0x3608`**,
  whose compensation is what §5.3 means by "no absolute pressure compensation is required".
  **A logger requirement follows and is NOT yet implemented** (owner regime, 2026-09-22,
  `../ndLouvers/pressure-testing.md` §2.4 step 4): a per-channel pressure correction configured in
  `KnurLogger.ini` from the bench curves and the measured route length, **multiplied by a live
  `P_abs`/96 600 factor read from the BME280**, applied to the RaceChrono feed only, raw counts
  always logged, parameters — including the `P_abs` in force — written into the session header, the
  same shape as the thermal offsets. The
  "5.1 % rising to 9.5 %" line-loss curve of rev 110–111 was `P0`'s deficit with its span inside;
  **do not quote it as a property of the tubing**, and do not reinstate the three revisions of it
  withdrawn at rev 111 either. **`k_d` 0.2372 and the 0.891 mm wall are withdrawn as of 2026-09-23**
  — the slope was read through a sensor and the wall inverted from it; `calibrationBell.md` §"As
  built" owns 0.2367 and 0.90 mm. **The add-mass
  check in `calibrationBell.md` cannot be performed on this build** (pan unreachable afloat) and is
  retired; a hand-placed bell needs two minutes after release before its first mark.
  **The `tempSensorHolder*.stl` here supersede the `Long`/`Short`
  pair that used to be in `ndLouvers/3DPrinting`** — those were deleted rather than moved,
  because they were older files under colliding names; git history still has them.
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
    (2026-09-10) — the ROM IDs are in [`one-wire-probes.md`](one-wire-probes.md). The plan owns
    the positions and the reasoning; this is a pointer, not a second copy.
- **`P` names a logger channel only.** The two pitot probes are `T1`/`T2`, never `P1`/`P2`.

## Hardware facts that surprise people

- **The sensor zone is ASSEMBLED** (owner, 2026-09-09), and **all five SDP810s are fitted and read
  correctly** (2026-09-19) — one per mux channel, each channel needing its own
  pull-ups. **ALL FIVE ARE HARD-SOLDERED, THE ±125 Pa INCLUDED** (owner, 2026-09-23). **This
  document, the build sheet and the plan all said the ±125 Pa was "connectorised rather than
  hard-soldered" — that was wrong about the board**, and plan open item 38 was closed partly on the
  strength of it. A sensor cannot be moved between mux channels with a plug, and no part can be
  separated from its channel for fault-finding without desoldering. **The ±125 Pa is on `P2`, not the specified `P4`** — the board won and the build sheet
  was corrected; §5 there is the record and carries all five serials. **An empty I2C scan is therefore no longer the correct
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
     directly. They arrived 2026-09-17; the first was brought up on the bench 2026-09-18.
- **THE FIRST I2C TRANSFER AFTER AN IDLE BUS IS REFUSED ON SOME BOOTS, AND A RETRY FIXES IT**
  (measured 2026-09-10 against the BME280 at `0x77`; **"every time" corrected to "some boots"
  2026-09-18**). This is the single most expensive thing to
  not know on this board, because it presents as *the device is dead* and it is not.
  0. **It is bimodal per boot — on or off for the whole life of a boot, never in between.**
     Reading `i2cFirstAttemptFailures` across all 32 sessions on the card: most show **exactly one
     failure per sample cycle**; **eight show exactly zero**, the 38-hour two-day track session
     among them at 0 in 135 146 cycles. A bench sweep on 2026-09-18 found 0 refusals in 300 BME280
     reads and 150 SDP810 reads at gaps of 2–1000 ms, and 0 in 40 interleaved mux-select-plus-read
     cycles. **So a clean run is evidence about that boot and about nothing else**, and the
     paragraphs below describe the refusing mode, which is still the common one. Whatever causes
     it is latched at initialisation. `../ndLouvers/` open item 44.
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
     `recoveredTransfers` and `exhaustedTransfers` land in every `enclosure` record. **On a
     refusing boot, one recovered transfer per sample cycle is the normal; on a non-refusing boot
     the normal is zero** (item 0). A retry that hid this would have turned a hardware
     characteristic into folklore — and it is also the only thing that revealed the bimodality.
     **`i2cExhausted` moving off zero is NOT by itself a degraded bus — check `present` first.**
     Two sessions carry it (67 on 2026-09-17, 41 on 2026-09-18) and both are confined to the first
     ~90 s with `present=false` and reason `deviceUnavailable`/`notAnswering`: the BME280 was
     physically off the board during assembly work, with the service running. The counter was
     right and the alarming reading of it would have been wrong. **A degraded bus is `i2cExhausted`
     climbing while the device is present**, which has never been seen.
  5. **Ten attempts, not four, and the difference was measured.** A chain long enough for one
     isolated register read is not long enough for a cycle of eight transfers: at four attempts
     **2 of 64 cycles still lost every channel**; at ten, **64 of 64 and then 300 of 300
     over a five-minute run came back clean**, nothing exhausted. The attempts are free when
     unused — cycle read time was 15-16 ms either way.
  6. **This is unqualified hardware and it applies to the five SDP810s too**, which sit on the
     same `SDA_MAIN`/`SCL_MAIN` behind the mux. The cause is not established — the main bus has
     no added pull-up (net list rows 10 and 11) and the BME280 breakout's own pull-ups are what
     the bus relies on. **Do not treat the retry as the answer to the physical question**; it is
     what makes the channel work while that question is open (`../ndLouvers/` open item 44).
- **A FAULTY DOWNSTREAM MUX CHANNEL HANGS THE WHOLE MAIN BUS, AND IT LOOKS PERFECT AT IDLE.**
  Hit twice on channel 1: **2026-09-18** with the channel empty and `R3`/`R4` unfitted, and
  **2026-09-19** with it populated and **`SD1` shorted to `SC1` at the mux** (owner found and fixed
  it). Identical symptom both times — every subsequent transfer on the **main** bus fails with
  `ETIMEDOUT`, mux and BME280 alike.
  1. **Every cheap check says the bus is fine.** `i2cdetect` still lists `0x70` and `0x77`, because
     quick-write probes still get ACKs; and `pinctrl get 2`/`3` show `SDA` and `SCL` **both
     idle-high**, so the stuck-low signature is absent. **Clean scan + both lines high + every
     transfer failing is the fingerprint of the two lines shorted to each other downstream** — at
     idle both are pulled up and look right, and it only bites once `SCL` toggles and drags `SDA`.
     **Check continuity `SDA_CH<n>` ↔ `SCL_CH<n>` before suspecting the sensor.**
  2. **Recovery is a `~RESET` pulse on GPIO17** — `pinctrl set 17 op dl`, pause,
     `pinctrl set 17 op dh` — which is what `R12` and net list row 12 are for.
  3. **Reset between channels when probing, or one bad channel masks every channel after it.** The
     first sweep on 2026-09-19 stopped at channel 1 and reported the remaining three as absent;
     isolating each channel behind a reset found all four of them healthy.
  4. **Only channel 5 is unpopulated now**, and its `R13`/`R14` are footprints only, so it is the
     one channel never to address. **The pressure worker must iterate a configured list of
     populated channels and never sweep**, and a bring-up scan must do the same. Build sheet §5.
- **All five SDP810s share one fixed I2C address (`0x25`) and cannot be strapped apart.** The mux
  is therefore mandatory, one sensor per channel. The mux does **not** pass pull-ups downstream, so
  every populated channel has its own pair.
  **Identify a sensor by its product number, never by which header it is in** — `0x03020A01` is the
  ±500 Pa part and returns 60 counts/Pa, `0x03020B01` is the ±125 Pa and returns 240. That check is
  what caught the ±125 Pa being on `P2` rather than the specified `P4`. **Retain the returned scale
  factor per sensor; never hard-code 60.**
- **`0x3615` IS NAK'D IF THE SENSOR IS ALREADY IN CONTINUOUS MODE** (measured 2026-09-19). Start
  continuous measurement **once** per sensor and then only read; a harness that re-armed it every
  cycle lost **145 of its 150 start-continuous commands** and looked exactly like a failing bus —
  150 being 30 cycles × 5 sensors, so **the denominator is the COMMANDS, not the run's transfers**
  (owner, 2026-09-19, settling a figure six documents had stated two ways).
  **Selecting a different mux channel does not take a sensor out of continuous mode**, so the worker
  must track per-sensor state rather than re-arming defensively on each visit.
  **`pressureSensors.cxx` does this, and the shape is worth knowing before changing it**
  (2026-09-19): `isContinuousStarted` is set when a read succeeds and cleared when one fails, so a
  sensor that browned out is re-armed on the next visit and one that did not is left alone. The
  re-arm costs `SDP810_START_SETTLE_US` and is paid only on that path. **Do not "simplify" it into
  an unconditional re-arm**, and do not treat a NAK there as a fault — it is the expected answer
  from a part that never lost the mode.
- **`0x3615`'s "AVERAGING" IS NOT AN AVERAGE OF THE READ INTERVAL, AND THE DATASHEET'S "PREVENTS
  ALIASING" IS ABOUT A FASTER READER THAN THIS ONE** (datasheet §5.2, read 2026-09-19). `average
  till read` returns the arithmetic mean of every internal sample since the last read **only for
  reads faster than 25 ms**. Past that it switches to exponential smoothing, `S_n = α·x_n +
  (1−α)·S_n−1` with **α = 0.05** applied to the ~2000 Hz internal samples — a **10 ms** time
  constant. At the worker's 100 ms period each sample therefore describes the **preceding ~10 ms**,
  not the preceding 100 ms, and the 90 ms in between is not represented in the record at all.
  1. **This is a measurement property, not a defect, and the decision is not this repository's.**
     `../ndLouvers/` open item 54 owns it. Do not change the read rate or add software averaging to
     "fix" it without that decision.
  2. **Do not conflate it with the bus-warmth item above.** Reading faster would incidentally keep
     the bus warm, and that must never become the argument for doing it — open item 44 is a hardware
     question and reaching for a faster rate would bury it. They are separate items on purpose.
- **A 10 Hz PRESSURE CYCLE DOES NOT KEEP THE I2C BUS WARM** (2026-09-19). A five-sensor cycle costs
  16–18 ms, so at a 100 ms period the bus is **idle for ~85 ms between cycles** — well past the
  10 ms that triggers the first-transfer refusal. So on a refusing boot the pressure worker meets it
  **every cycle**, exactly as a 1 Hz sampler does, and the retry is as load-bearing at 10 Hz as at
  1 Hz. A faster sample rate is not a workaround for the deviation and must not be reached for as
  one. `../ndLouvers/` open item 44.
- **READ THE MUX CONTROL REGISTER BACK AFTER WRITING IT.** `selectMuxChannel` writes `1 << n` and
  then reads the register and compares. This is not belt-and-braces: **a write that appears to
  succeed onto a faulty segment is exactly the failure that hangs the bus**, and the mux answers
  from the main side, so it still replies while the segment it just connected is dragging the
  downstream lines together. The read-back is where that is caught, and it costs one transfer.
- **EVERY COUNTER IN A `pressure` RECORD IS A CUMULATIVE SESSION TOTAL, NOT THAT CYCLE'S COUNT**
  (2026-09-21, and it cost a false hardware alarm). `readErrors`, `crcFailures`,
  `i2cFirstAttemptFailures`, `i2cRecovered` and `i2cExhausted` are `appData.pressure.*` and
  `appData.i2c.*` running totals, written into **every** record so that any single record says
  which mode the boot was in. The per-channel `readErrors`/`crcFailures` inside `channels[]` are
  the same: `channel.readErrors` is cumulative for that channel.
  1. **Summing them across records multiplies them by roughly the record count.** A session with
     **3** read errors read back as **2 972**, and a clean 54 000-cycle run was reported as a
     degrading bus that needed a `~RESET` and a continuity check. It did not; a bench probe found
     all five sensors answering with the right product numbers and serials and 300 round-robin
     cycles with zero failures.
  2. **The right reads.** Session total = the **last** record's value. Per-cycle delta = the
     difference between consecutive records. **Number of affected cycles = count records where the
     channel's own `valid` flag is false**, which is the only field that is per-cycle.
  3. **A non-zero counter on a record does not mean that cycle was bad.** Once the total moves off
     zero it stays there, so every later record looks "errored" to a truthiness test. That is the
     specific mistake: `if r["readErrors"]:` is true for the rest of the session.
  4. **This is why `i2cExhausted` looked like it was climbing with the device present** — the
     signature this file says has never been seen. It still has not been seen.
  5. **The same shape applies to `enclosure` and `temp` records** — `appData.bme280.readErrors`,
     `appData.thermal.readErrors` and `channel.readErrors` are all cumulative. `validMask` and each
     channel's `valid`/`reason` are the per-cycle fields.
  6. **`i2cFirstAttemptFailures`, `i2cRecovered` and `i2cExhausted` are `appData.i2c.*`, which is
     SHARED BY ALL THREE I2C WORKERS.** The figure in a `pressure` record includes the BME280's
     transfers and vice versa, so a count from one worker's records is a whole-bus count, not that
     worker's. It is the bus that open item 44 is about, so this is the right scope — but do not
     attribute a failure to a sensor on the strength of which record it appeared in.

- **A SKIPPED BME280 MEASUREMENT DOES NOT MOVE THE READ-ERROR COUNTER, AND THAT IS CORRECT.**
  Measured once in the field, 2026-09-14. All three `0x600` values go to their sentinels in the
  same cycle — a skipped temperature invalidates pressure and humidity through the shared `t_fine`
  intermediate — and **`0x601` byte 0 drops 31 → 3**, keeping only *present* and *calibration
  read*. `bmeReadErrors` stays put and `lastReadMs` stays normal, because the transfer succeeded
  and the part simply reported no measurement. **So `bmeReadErrors == 0` is not evidence that every
  sample was valid**; byte 0 is the channel that says so, and this was the first time it read
  anything but 31 in the field. `../ndLouvers/thermals-testing.md` §3.10.
- **The BME280's three quantities have three different consumers, and two of them are easy to
  point at the wrong thing.** `bme280Sensor.cxx` reads it and the field names carry the roles;
  `../ndLouvers/thermals-testing.md` §1.4 owns the reasoning and Step 0b owns the requirement.
  1. **Pressure is ENCLOSURE pressure, never a static reference.** The cavity is aerodynamically
     live. **Measured on the first drive: 156 Pa below stationary at a mean 116 km/h (Cp ≈ −0.25)**,
     — below the −0.5…−1.0 that had been estimated, but still 2–3× the 45–90 Pa
     measurands, and **not a single constant Cp**. Tolerable as a density term, disqualifying as a
     reference. Logged as `enclosurePressurePa`.
     **That first drive is the only QUALIFIED cavity measurement of it there is.** **A second cavity record exists from the third drive and is NOT a second Cp point** — right sign and order (−37 Pa mean at 60–100 km/h, r = −0.68) but the route's elevation change is the same order as the signal and the export carries no altitude channel (`../ndLouvers/thermals-testing.md` §3.6). **Two track days add SEVEN sessions and they now CONTRADICT the first drive** — Cp −0.108 to −0.144, and flat when banded by speed (−0.122 to −0.134 from 40 to 200 km/h), which is a measured absence of the first drive's structure rather than an averaging artefact. `../ndLouvers/` open item 49 owns the disagreement and it is not settled. **The disqualification as a reference survives either answer**, which is the only part this repository needs. The second drive's box was
     on the passenger seat, so its strongly speed-correlated pressure record is a **cabin** record;
     it was written up as a second Cp point and withdrawn. **This field is named for where the box
     is, not for where it was designed to be** — nothing in the session file says which.
     **IT NOW HAS A SECOND CONSUMER, AND THAT ONE IS LOAD-BEARING** (2026-09-23): it is **the five
     SDP810s' density correction**. Their reading scales with absolute pressure because they are
     thermal mass-flow devices calibrated at 966 mbar, so `P_abs`/96 600 is a multiplicative term on
     every pressure channel — +4.19 % at the bench and swinging 4 pp across ordinary weather
     (`../ndLouvers/pressure-testing.md` §2.3a). **So "disqualified as a static reference" is still
     true and is no longer the whole story**: this channel must be logged with every session,
     bench or road, or that session cannot be reduced afterwards. It is the only reason the
     2026-09-20/21 bench sessions could be re-derived at all.
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
- **A DS18B20 AT EXACTLY 85.00 °C IS INDISTINGUISHABLE FROM ONE THAT HAS JUST RESET, AND `T_aft`
  GOES THERE** (measured 2026-09-18 from the two-day SD record). 85.00 °C is the power-on
  scratchpad default *and* a real temperature, and the scratchpad bytes are identical
  (`50 05 4b 46 7f ff 0c 10 1c`) in both cases, so `oneWireProbes.cxx`'s `powerOnDefault` check
  cannot tell them apart. All 17 flagged samples in that session are `temp3` transiting 85.000 °C
  on a smooth ramp with a good CRC — **not one is a bus fault**, and the "two CRC failures"
  reported for the 2026-09-13 track day are withdrawn. `T_aft` peaks at **101.25 °C** and puts
  2 351 samples above 85 °C, so this is a band the aft probe lives in rather than an edge case.
  **The reason code sends a reader to the power and the pull-ups, and that is the cost** — it is a
  diagnosis the data cannot support. `one-wire-probes.md` owns the requirement; **`../ndLouvers/`
  open item 53 owns the remaining decision, and open item 52 was DROPPED on 2026-09-20** — the aft
  probe's band-edge accuracy will not be bounded and its assembly rating will not be checked, the
  owner having accepted the risk of losing it. **So treat every `T_aft` sample above 85 °C as an
  indication rather than a measurement, permanently**, and do not open a task to characterise it.
- **The whole 1-Wire path is in [`one-wire-probes.md`](one-wire-probes.md)**, split out of this
  file on 2026-09-15: the ROM-ID bindings, the mandatory `28-*` family filter, `therm_bulk_read`
  and the two conditions that make it convert, the ~100 s tail a pulled probe leaves behind, the
  fake-sysfs harness and its one limitation, and the enrollment and calibration-offset
  requirements. **Read it before touching `oneWireProbes.cxx`** — that file is the authority on
  this subsystem, and what remains here mentions the probes only where another subject touches
  them.

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
   §3.8. **The cavity is not a link problem, and plan item 5a is closed on that run** (owner,
   2026-09-15). It closed without sustained speed, a steering-lock and suspension sweep, or a
   recorded seat occupancy, so a link problem in any of those is a reopening rather than a
   surprise.

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
4. **For a CAN frame the cost is worse, because the frame arrives anyway and the byte is thrown
   away.** A DIY packet nobody subscribes to is never sent; a **CAN** frame is forwarded whole by
   box 1 and RaceChrono then stores only the channels it has definitions for. **There is no
   raw-frame layer in a `.rcz`**, so an undefined byte is discarded on arrival and no recording can
   be reprocessed to recover it. Found 2026-09-15: `0x420` byte 7, the outside-air temperature the
   plan's thermal item 4a is built on, had never had a channel defined, and five sessions of it
   are gone. `../ndLouvers/` open item 47. **"Box 1 already broadcasts the frame" is a statement
   about cost, never about whether the data exists.**
   **The channel was defined on 2026-09-19** — `H-40` on predefined channel 10031 — so the byte is
   recorded from the next session onward. **That changes nothing about the five lost sessions and
   nothing about the rule**, which is why this entry keeps its present tense about the mechanism:
   the loss is silent on both boxes and in the export, so only a channel defined *before* a session
   ever helps.

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
   **It checks `Pressure` slots the same way since 2026-09-19, at +3.2768**, the same sentinel
   through the pressure channels' `/10000`. That check matters *more* than the thermal one: a
   negative differential is normal on half the pressure channels, depending only on which port the
   tube lands in, so an unsigned decode there corrupts **ordinary data** rather than only the
   marker. **`Pressure Front 50` is deliberately exempt** — `0x600`'s enclosure pressure is
   unsigned by design and its marker is 4294967.295, which no signed decode produces.
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
   > **⚠ A `git diff` OF THAT FILE IS NOT A REVIEW OF IT** (2026-09-19). RaceChrono emits
   > `customChannels` in an order that shifts as entries are added, so a re-export rewrites lines
   > that did not change: adding 14 channels produced 184 insertions and 86 deletions. **Compare two
   > exports by `(pid, channelId)`**, decoding each id as `slot * 2**20 + channelType`, or an
   > equation edited by hand passes review unseen — which is the one fault class this file exists to
   > catch. Do **not** sort the array to make the diff readable: the committed file is the export
   > verbatim minus `localUuid`, and re-import has never been tested against any other shape.
5. **AN ALL-EMPTY CHANNEL IS NOT NECESSARILY A BROKEN ONE.** RaceChrono renders a deliberate
   out-of-range marker as no value at all, so a channel that is `NaN` in every sample can be a
   sensor that sat outside its calibration range for the whole session. **Box 1's two are exactly
   that** (owner, 2026-09-11) — `lowPass(E,254)` and `lowPass(F,254)` on `0x7F0`, working as
   designed. The tool reports them and no longer calls them faults. **Do not "fix" a channel on
   this evidence alone**; ask what the sensor was doing.
   **The 2026-09-13 track day proves it from the other side:** both channels read normally once
   the car is warm and are empty only for the first 200–550 s of each session. So "empty" is a
   property of the session, not of the channel — and a single recording can never settle it.
6. **RaceChrono's parser is case-insensitive** — `bytesToUint`, `bytesTouInt`, `bytestouint` and
   `bytesToUInt` all appear in the profile and all work. **Do not normalise the casing**: there is
   nothing to fix, and `bytestoint` differs from `bytestouint` by the single letter that decides
   signed against unsigned.
7. **A resumed session has one fragment per stretch**, the first at the archive root and the rest
   under `resume_<n>/`, and the tool reports them separately. It used to read the root only, which
   silently dropped half of the 2026-09-13 track day. A channel edited between stretches
   legitimately differs across fragments, which is why they are not merged.

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
  4. **Battery drain is a new failure mode.** Estimated ~275 mA at 12 V, plausibly 330–430 mA once
     BLE and the workers are counted — ~3.3–5 Ah over a 12 h day
     against the ND's ~45 Ah, which is comfortable, but **~46 Ah over a week with the fuse left
     in, i.e. a flat battery.** Estimated, not measured; plan item 5.3 still owes the real figure.
     **That estimate predates the five SDP810s, which were drawing nothing in every session on
     record**, and it predates the sixth worker. **Neither will be measured** — `../ndLouvers/`
     Step 0b item 5.3 was dropped on 2026-09-20 with the rest of the electrical qualification
     programme, so this stays an estimate permanently. **Label it as one wherever it is quoted**;
     the practical rule it supports — a fuse left in for a week is a flat battery — does not need
     a figure to be true.
  **The ~1 s `fsync` requirement is unchanged — only its trigger moved.** A hard cut is the fuse
  being pulled, or a cranking dip. Repeated hard cuts are the durability risk worth knowing: one
  costs at most the last second, but doing it daily for a season is the classic route to a corrupt
  SD card, so `sudo poweroff` before pulling the fuse is free insurance.
  **A hard cut is recognisable when reading a session back** — no `BLE stopped` event, no closing
  record, the file simply stops. Every road-test session through 2026-09-11 ended that way.
  **The two-day track session did NOT**: it closes with `BLE stopped, 55447 notifications sent`,
  so the `poweroff`-before-pulling-the-fuse practice was followed and the note above is now
  describing something that happens rather than something to start doing.
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
  (`0x420` byte 7) and, if it is on the bus, **cooling-fan state** can only arrive through box 1's
  CAN broadcast into RaceChrono — not through this logger (`../ndLouvers/` §7 open item 35).
  **"Can arrive" is not "does": the ambient one has no channel definition and therefore has never
  arrived at all** (trap above; `../ndLouvers/` open item 47). So the
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
  5a's installed link check to answer. **That item is now closed and it did not answer this
  question**: no session records the radio state, so a clean link says nothing about whether gating
  was doing any work. Gate Wi-Fi for any link measurement regardless.

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

Four requirements came from the plan rather than from iSitePiLogger. Two of them — DS18B20 channel
enrollment and the calibration offsets — moved to [`one-wire-probes.md`](one-wire-probes.md) with
the rest of that subsystem, along with a fifth that was implemented and then **retired whole**.

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
- **DS18B20 channel enrollment and the calibration offsets** — both in
  [`one-wire-probes.md`](one-wire-probes.md) §"Requirements this path carries from the plan",
  with the retired session-start thermal sample recorded there too.
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
