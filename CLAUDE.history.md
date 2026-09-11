# CLAUDE.history.md — KnurLogger

**Companion to `CLAUDE.md`.** That file states what is true now and what to do about it. This
file is the audit trail behind it: faults that were found and fixed, the diagnostics that found
them, decisions that were reversed, and requirements that were implemented and then deliberately
removed.

**Nothing here is a live instruction.** If something here contradicts `CLAUDE.md`, `CLAUDE.md`
wins. If something here looks live and is absent from `CLAUDE.md`, it was removed on purpose —
find out why before reinstating it.

**Why the file exists.** Two reasons, and the second is the load-bearing one.

1. A fault whose *diagnosis* was expensive is worth recording even after the fault is gone, so the
   next silent bus or unregistered advertisement is not diagnosed from scratch. The reusable half
   of each of those — the command that splits the search space — stayed in `CLAUDE.md`; the
   incident is here.
2. **Git history and the plan's older revisions still describe retired requirements as live.**
   Deleting a requirement leaves nothing to read; a note saying "this was built, measured failing
   and dropped, and here is why" is what stops the next agent rebuilding it from a transcript.

Keeping all of that inside `CLAUDE.md` was burying the live traps in narrative, which is why the
file was split on 2026-09-10.

**When you add to it:** put the new conclusion in `CLAUDE.md` and the superseded one here, with
the reason and the date. Do not leave "this used to say" narrative in `CLAUDE.md`.

---

## 1. Resolved faults, and the diagnostics that found them

### 1.1 The mux was silent because `~RESET` went to the wrong header pin

**2026-09-09, resolved** — resoldered and verified answering at `0x70`, control register `0x00`.

`~RESET` was soldered to header **pin 9 instead of pin 11**. Pin 9 is a ground pin and pin 11 is
GPIO17, and they are adjacent in the same row, so this is a one-position off-by-one onto the worst
possible neighbour: a PCA9548A held in reset does not degrade or partly work, it goes **completely
silent**, which is indistinguishable from an absent or dead part.

**The diagnostic that localised it** is the part worth keeping. `gpio=17=op,dh` is live in
`/boot/firmware/config.txt` and `pinctrl get 17` reads `17: op -- pd | hi` — so the Pi was
provably driving `~RESET` high while the mux end measured low, which puts the fault on the wire
rather than on the host or the part.

Two rules came out of it and are stated live in `CLAUDE.md`: check the Pi side with
`pinctrl get <n>` before suspecting a silent bus device, and do not try to fix a reset problem in
software — GPIO17 was already high, so no `pinctrl`/libgpiod write could have helped and one would
only have masked the diagnosis.

Build sheet §2 note 2 and the §3a.7 check table carry the meter checks, including the one that
**passes on a board with this fault** (`MUX_RST` ↔ `+3V3` still reads `R12`'s 10 kΩ).

### 1.2 `bluez 5.82-1.1+rpt1` on kernel `6.18.34` could not advertise at all

**Cost a day. Fixed by `apt full-upgrade` on 2026-09-09** → `bluez 5.82-1.1+rpt2`, kernel
`6.18.39`, `firmware-brcm80211 1:20260519`. If a future image regresses to those versions, this is
the explanation for a logger that starts, registers its GATT application and is never seen by
RaceChrono.

**The symptom** is `binc` logging `failed to register advertisement (error 36:
GDBus.Error:org.bluez.Error.Failed)` and `bluetoothd` logging
`add_client_complete() Failed to add advertisement: Invalid Parameters (0x0d)`.

**The cause is a userspace/kernel structure mismatch**, visible only in an HCI trace: the
`Add Extended Advertising Data (0x0055)` MGMT command arrives with `plen 14` while the fields it
declares — instance, `adv_data_len: 3`, `scan_rsp_len: 0` — account for 6 parameter bytes. The
kernel validates that length and rejects the mismatch. `Available adv data len` was 31, so it was
never a capacity problem.

The diagnostic sequence that found it is live in `CLAUDE.md`. Its step 3 (`btmgmt add-adv`, the
*legacy* MGMT path) succeeded throughout, which is what proved the controller and kernel could
advertise and narrowed the fault to `bluetoothd`'s extended-advertising path.

**Three plausible-sounding explanations were tested and were all wrong.** Recorded so nobody
spends the time again: it was **not** advertising-data overflow (a 4-character device name failed
identically), **not** `max_adv_data_len` (31, ample), and **not** connectability, daemon config or
stale daemon state (`ControllerMode = le`, `Experimental = true` and a `bluetooth` restart each
changed nothing).

### 1.3 The Bluetooth radio shipped soft-blocked, persistently

**Cleared 2026-09-09** by `harden-headless.sh` phase 7, and the unblock **survived the reboot**:
`rfkill list bluetooth` reads `Soft blocked: no`, `hciconfig` reads `UP RUNNING`, BlueZ reads
`Powered: yes` / `PowerState: on`. `systemd-rfkill` now restores *unblocked* from the same state
directory that used to restore the block.

As it shipped, `rfkill list` reported `Soft blocked: yes` and BlueZ `PowerState: off-blocked`.
**In that state there is no BLE and therefore no product.** Two traps, which is why `CLAUDE.md`
still carries the `rfkill block wifi` rule:

1. **`bluetoothctl power on` cannot clear it.** rfkill sits below BlueZ. The service runs, the
   controller enumerates, and it still refuses to power.
2. **It survives reboots.** `systemd-rfkill` saves per-device state under
   `/var/lib/systemd/rfkill/` and restores it at boot. `sudo rfkill unblock bluetooth` clears it
   and is persisted the same way.

### 1.4 The bare 1-Wire bus invented phantom devices, the set churned, and it stopped

With the overlay loaded and **the sensor zone unbuilt**, `/sys/bus/w1/devices/` held
`w1_bus_master1` plus a varying number of `00-*` entries whose IDs changed from scan to scan.
Measured 2026-09-09 across 35 s: first `00-800000000000` alone, then `00-dc0000000000` +
`00-3c0000000000`, then `00-3c0000000000` + `00-bc0000000000`, with `w1_master_slave_count`
reading `1`, `2`, `2` and `w1_master_attempts` already past 250 with nothing attached.

**With the sensor zone assembled, three scans over 36 s returned the master alone,
`slave_count = 0`, and no `00-*` at all.** `R11`'s 2.2 kΩ to `+3V3` is in the sensor zone, so
`GPIO4` no longer floats on the SoC's internal pull-up and the bus search reads a terminated line
instead of noise. Well-supported but not proven — nobody re-floated the line to confirm.

The consequence for the code is live in `CLAUDE.md`: the `28-*` family filter and its rules stay
mandatory, but they **can no longer be exercised on this box**, so their correctness now rests on
the parser rather than on an observation.

### 1.5 `initialiseBlePackets()` was called twice

**Removed 2026-09-10.** `raceChronoBleLoop` called it a second time after `main` already had. It
was harmless only while nothing produced a thermal reading: it re-runs `g_mutex_init` on live
mutexes and resets `0x602` to the all-invalid sentinel, so once `oneWireProbes` existed a real
reading taken before the BLE worker finished starting would have been silently clobbered back to
−327.68 °C. The rule that came out of it — initialise shared packet state in `main`, never in a
worker — is live in `CLAUDE.md`.

### 1.6 The box shipped accepting SSH passwords, and the public record of that is permanent

**Closed 2026-09-09** — `ssh-harden.sh --execute` ran and the box is key-only
(`passwordauthentication no`, verified by a forced password-only attempt being refused).

This is the worked example behind `CLAUDE.md`'s rule about measured-state notes. The statement
that the box accepted passwords was accurate and useful when written, and **git history is not
retractable**, so a public repository now carries a permanent record that this host once accepted
password authentication. It took a day to close; the next such note might not.

### 1.7 The BME280 answered at an address the build sheet forbade

The first scan of the assembled sensor zone found the BME280 at **`0x77`**, not the specified
`0x76`. It is a genuine BME280, not a mux at a strapped address — chip-ID register `0xD0` reads
`0x60`. The build sheet used to say "**Do not use `0x77`**" and put `U3.SDO` on `GND_SIG`; the
**owner amended the document rather than the board** (2026-09-09), so `0x77` is the specified
address now and is no longer free for anything else. No collision results, because the mux is
strapped to `0x70` alone and the "keep `0x77` clear of the mux range" reasoning only bites if a
mux is strapped upward.

### 1.8 An early `harden-headless.sh` draft disabled `wpa_supplicant`

Caught by the first real audit. `wpa_supplicant.service` is enabled and running on this image and
**NetworkManager drives it over D-Bus**; disabling it because "NetworkManager spawns its own"
loses Wi-Fi, which is the only way onto a box in a wheel-well cavity. The fact is stated live in
`CLAUDE.md`; this is the record that the mistake was actually made and shipped in a draft.

### 1.9 How the two halves of the I2C setup were told apart mid-run

`/dev/i2c-20` and `/dev/i2c-21` are the VC4 display DDC buses, and their appearance is what
diagnosed a missing `/dev/i2c-1`: loading `i2c-dev` before the reboot produced 20 and 21 but no 1,
because the `i2c_arm` adapter does not exist until the firmware re-reads `config.txt`. **Both
halves took on 2026-09-09** and `/dev/i2c-1` exists.

### 1.10 The apt pre-flight list was not a statement of what apt would change

`SystemSetup/install-dependencies.sh --execute` (2026-09-09) installed `i2c-tools`, `cmake`, `git`
and `libglib2.0-dev`; `rfkill`, `build-essential`, `nmcli` and `bluetoothctl` were already
present. That run also dragged the whole util-linux family forward from `2.41-5` to
`2.41.5-0+deb13u1` as a dependency — **`rfkill` among them**, despite the script correctly
reporting it present and skipping it. Nothing broke, but a four-package pre-flight list does not
bound the change.

---

### 1.11 The first enrollment at the car unplugged each probe as the next went in

**2026-09-10, at the car.** `--enroll` bound `temp0` = `28-06254385da1f`, `temp1` =
`28-0625424044b7` and `temp2` = `28-0625424e16c9`, and was then abandoned; the bindings were
deleted afterwards. They are in the `enrollment` records of
`2026-09-10T09-29-23.594215Z-enroll.ndjson` and nowhere else.

**Binding worked. Everything binding is *for* did not.** The procedure had said "plug them in one
at a time" and meant plug-in-and-leave; it did not say so, and unplugging as you go binds each
probe perfectly well, which is precisely why nothing complained. What it cost:

1. **The four-probe star was never loaded.** Plan thermal item 1's open acceptance criterion is
   all four enumerating together with CRC-clean reads on the 4 × 5 m bus, and the run has no
   bearing on it — three probes read singly is what the ESP32 bench rig had already done in
   2026-09-08. `temp2` alone was on the bus for the last 703 cycles.
2. **The warm-one-probe map check was impossible**, needing four live channels.
3. **It produced §1.12's 186 phantom read errors.**

`temp0`'s bind reading came back `INVALID (readFailed)`, which the run also had no way to follow up
on. The README now states plug-and-leave and gives all three reasons; `CLAUDE.md` requirement 3
rule 7 carries the rule.

### 1.12 A dropped lead was charged to the bus-quality counter for ~100 s per unplug

**2026-09-10, found by reading the session file from §1.11.** The channels showed contiguous runs
of `unparseable` — `temp0` cycles 360–434, `temp1` 387–462, `temp2` 410–444 — and each channel's
`readErrors` matched its `unparseable` count exactly: 75, 76 and 35, 186 in total, on a bus that
had not failed a single read.

**The diagnostic that identified it was the read duration, not the reason string.** `temp0`'s last
good read took 790 ms — a real conversion — and every read after it took 37–40 ms. Too long for a
cached value and far too short for a conversion, i.e. the kernel attempting a transaction and
getting nothing. That timing plus the run structure gives the mechanism: the kernel unregisters a
1-Wire slave only after `w1_slave_ttl` (10) missed searches at `w1_master_timeout` (10 s), so for
up to ~100 s after a lead comes out the sysfs directory is still there, `w1_slave` still reads, and
the content carries neither `crc=` nor `t=`.

`readProbe()` set `isPresent` the moment the file read succeeded, before looking at the content, so
the counting site's `isBound && isPresent && !isValid` charged all of it to `readErrors` and to
`0x603` byte 2–3 — the exact failure the code comment three lines above it forbids for `absent`,
arriving through the one path that still had a directory to read.

**Fixed** by splitting `notAnswering` out of `unparseable` on the presence of the two markers,
counting it in its own field, and recording the raw content (`unparsed`) so the next occurrence can
be told apart from a marginal bus without a trip to the car. `CLAUDE.md`'s family-filter list item
6 is the live rule.

### 1.13 `therm_bulk_read` existed for the first time, and refused the write 863 times

**2026-09-10, at the car.** `enroll.log` carried 863 identical
`OneWire: bulk read trigger refused` lines at 1 Hz and nothing else, burying the two `BOUND`
messages the run existed to produce.

**What the timing established.** The first warning is at 11:33:55 and the first bind is at
11:33:55 — the same second — with none before. `w1_therm` registers `therm_bulk_read` as a master
attribute only once a slave of its family attaches, so this was the attribute appearing for the
first time on this box, and the code path having its first execution ever. `CLAUDE.md` had recorded
it as untestable and shipped-unexercised on exactly that reasoning; the reasoning was right and the
path failed the moment it ran.

**The cause was not recoverable from the artefacts**, because `writeSysfsValue()` discarded
`errno`. The expected reason was `EACCES` — every writable attribute in
`/sys/bus/w1/devices/w1_bus_master1/` is `root:root` and the logger runs as `chrum`, which
`CLAUDE.md` had already predicted in the sentence about `w1_master_timeout` not being shortenable
without root. The fix reported `errno` once, into the session file as well as the console, and a
udev rule was deliberately **not** written against the guess.

**The next run confirmed it: `errno 13`, `EACCES`** (2026-09-10, second enrollment). Worth keeping
because the discipline paid nothing and cost nothing — the guess was right, and waiting one run to
check it turned a plausible fix into a known one for the price of a single log line. **It was also
not the whole fault: with the permission fixed the bulk read still does nothing — §1.14.** The rule that
followed is in `SystemSetup/`, and `CLAUDE.md` carries the two non-obvious things about it: it has
to match the slave's add event rather than the master's, and it is testable without a probe via
`w1_master_add`.

**The measured cost is real either way.** A per-probe read is 790 ms, so four probes is ~3.2 s per
cycle: plan thermal item 2's "start around 1 Hz" is **0.31 Hz** until the bulk path works.

### 1.14 The udev rule fixed the permission and the bulk read still did nothing

**2026-09-10, at the car, four real probes, 331 cycles.** With `therm_bulk_read` now writable the
trigger is accepted every cycle — and the cycle time did not move: **3190/3213/3309 ms** against
3198/3213/3281 before the rule, with **every probe still reading in ~800 ms** (790/799/860). If the
bulk path were working, at most the first probe would pay a conversion and the other three would
return the scratchpad in single-digit milliseconds. None of them did.

**The permission was necessary and not sufficient.** §1.13 diagnosed `EACCES` correctly and the
fix was right; it simply was not the whole fault. Worth stating plainly because the udev rule
passed its own test and the natural conclusion was that the job was done.

**What the owner spotted, and why it was the tell.** `0x603` bytes 6–7 — last conversion, ms — read
**0** on the phone. That is a field whose entire job is to show conversion cost, reading zero
during the most expensive part of the cycle. The cause was in this repository:

```
lastConversionMs = bulkWaitMs.has_value() ? *bulkWaitMs : slowestReadMs;
```

`bulkWaitMs` had a value whenever the **write** succeeded, so the moment the udev rule landed, the
0 ms bulk wait started masking the ~800 ms the probes were actually taking. **The fix that made the
write succeed is what made the reporting lie**, and without that 0 on the phone the dead
optimisation would have been recorded as working.

**Why the wait is 0 ms.** `triggerBulkConversion()` polled until `therm_bulk_read` read anything
other than `-1`, and broke on the first poll. The attribute reads `-1` while a conversion is
running, `1` when results are ready, and `0` when no device on the bus supports bulk reading —
so **breaking on "anything but `-1`" cannot distinguish "finished instantly" from "nothing was
triggered"**, and the code had no way to tell which had happened. It does now: the readback is
recorded as `bulkState` on every `temp` record.

**Not yet diagnosed, and the first suspect.** A bulk conversion of parasite-powered probes needs a
strong pullup that this bus does not provide, and `w1-gpio`'s `pullup` parameter is ignored on this
firmware. So `ext_power` per probe is the first thing to read — it is now dumped once into a
`probeCapabilities` record on the first cycle that sees any probe, alongside `resolution`,
`conv_time` and the master's `features`. **Read that record before theorising further.**

**Three fixes shipped, all of them reporting rather than behaviour:** `conversionMs` reports what
the cycle actually paid; `bulkConversion` means a conversion the kernel confirmed rather than a
write that succeeded; and a one-shot warning names the readback value when the trigger is accepted
but converts nothing. **The logger has always been correct and slow — none of this ever produced a
wrong temperature**, which is why it survived a day of being wrong about itself.

**Verification is worth recording because the usual harness could not do it.** The fake-sysfs tree
represents attributes as plain files, so `therm_bulk_read` reads back the `trigger` that was
written to it and the state machine cannot be exercised at all. The `conversionMs` fallback was
verified instead with a **FIFO** standing in for `w1_slave`, which makes a read block for a
controlled time: cycle 1 reported `conversionMs 385` against a slowest read of 385 ms, where the
old code reported 0.

## 2. Superseded decisions

### 2.1 "SD primary, BLE secondary" → BLE is the primary data path

**Reversed by owner decision, 2026-09-09** (plan item 5b). Do not reinstate it from memory or from
git history. The analysis record is RaceChrono's consolidated log, because RaceChrono is what
collects box 1's CAN broadcast, box 2's channels and the phone's GPS and stamps them into one
frame set on one timebase. The three narrower grounds on which the local file remains mandatory
are live in `CLAUDE.md`.

### 2.2 The accessory feed → constant 12 V

**Superseded by the box as built, 2026-09-10** (owner). Every document previously assumed an
accessory feed where **ignition-off is the power cut**. It is a constant feed: the logger runs the
whole day and the fuse is the off switch. The four consequences are live in `CLAUDE.md`; the one
worth restating here is that the ~1 s `fsync` requirement never changed — only its trigger moved.

### 2.3 "Never auto-apply a thermal offset" → offsets are applied to the RaceChrono feed

**Reversed by owner decision, 2026-09-10.** The objection was that an offset applied silently
cannot be un-applied later. It was put to the owner and answered on its own terms: it is neither
silent nor irreversible, because the session record carries the raw reading (`centiC`), the offset
in force (`offsetC`) and the value that actually went on the air (`sentCentiC`) side by side. A
mistyped offset costs a reprocess, not a session.

**That record-both property is the entire basis on which the reversal is safe** — anything that
later drops the raw value re-opens the original objection. `CLAUDE.md` states that as a live
constraint.

### 2.4 ROM-ID-keyed offsets in the store → slot-keyed in `KnurLogger.ini`

**Owner decision, 2026-09-10**, taken after the trade-off was put to them; they were briefly
ROM-ID-keyed. Do not "restore" ROM-ID keying. The binding store had no `offsetC` key after this,
and §2.7 later that day removed the store itself. The live consequence — an offset is a property
of one particular DS18B20, so re-enrollment in a different order or a swapped probe strands the
calibration — is in `CLAUDE.md`.

### 2.5 A session-start clock offset against the phone's GPS time

**Superseded twice over**, and its stated mechanism does not exist. RaceChrono stamps every source
on arrival, so box 1 and box 2 are never aligned against each other's clocks; and the RaceChrono
DIY protocol is device→phone notifications plus a filter-write channel, carrying no time transfer
to fetch a GPS time with. **Do not build a GPS-time fetch.** What survives is cheap, sufficient
and live in `CLAUDE.md`: every local record carries elapsed-since-boot alongside the wall clock.

The related misreading went with it: the two boxes' records are not reassembled by a session-start
time offset, they are reassembled by RaceChrono. The consequence that survived is sharper than the
one that went — see `CLAUDE.md` on what a BLE gap actually costs.

### 2.6 "The sensor zone is unbuilt, so an empty I2C scan and zero `28-*` devices are correct"

**Superseded 2026-09-09**: the zone is assembled, minus the pressure sensors. Every document in
both repositories said the zone was unbuilt and that those were the correct results, and the
acceptance criteria moved with the build. A missing `/dev/i2c-1` was never explained by "the
sensors are not built yet" either — that only ever explained an *empty scan*.

---

### 2.7 A separate `channels.ini` binding store → the bindings live in `KnurLogger.ini`

**Owner decision, 2026-09-10.** The store had been deliberately placed in the data directory,
beside the session files, on the reasoning that it was field state that had to outlive a rebuild
and `build/` is where a rebuild lands. The owner reversed it on a simpler ground: the logger's
configuration should not be scattered across several files, and the bindings being in git is
acceptable rather than a problem.

`channels.ini` no longer exists in any form. The bindings are `temp<N>RomId`,
`temp<N>BoundTaiUs` and `temp<N>BoundIso` in the `[thermal]` section of `KnurLogger.ini`,
machine-written by `--enroll` alongside the hand-entered offsets. **Do not rebuild the separate
store**, and do not restore the "field state must outlive a rebuild" argument from this file or
from git history — §2.8 is what answers it.

Two things the move gained that were not the reason for it. The slot-keyed offsets and the
bindings that decide which probe each slot holds are now three lines apart, so §2.4's live
consequence — a re-enrollment strands the calibration — is visible where it bites rather than
filed in another file. And a ROM ID can be typed in by hand, which the machine-written store never
invited; the `28-*` family filter and the one-probe-one-channel rule apply on load either way.

The one thing it cost is that enrollment now rewrites a file a human maintains. It does so line by
line rather than through `g_key_file_to_data()`, which destroys non-ASCII characters in comments —
measured the same day, the em-dashes in that file's header came back as `?`. That is a live rule in
`CLAUDE.md`, not history.

### 2.8 One `KnurLogger.ini` on the box → a git-tracked template and a production copy in `~/bin`

**Owner decision, 2026-09-10, taken as the consequence of §2.7.** With the bindings in
`KnurLogger.ini`, a single copy on the box would have meant the `tar`-over-ssh build loop —
which overwrites everything under `~/KnurLogger` — silently discarding a trip to the car, not just
a hand-typed offset. That trap had been documented and worked around by discipline
(`--exclude=build/KnurLogger.ini`, or copying the file back before the next sync); the stakes made
discipline the wrong answer.

The logger is now built in `~/KnurLogger/build/` and run from `~/bin/`, installed by
`SystemSetup/deploy-logger.sh`, which always replaces the binary and only ever creates the `.ini`.
`KnurLogger.service` was pointed at `/home/chrum/bin/KnurLogger`.

**Do not reinstate the single-copy arrangement, and do not reinstate the `--exclude` workaround** —
there is nothing left to exclude, because the file the sync overwrites is now a template nothing
reads. The cost of the split is that the production `.ini` is outside git, so §2.4's "the
calibration ends up version-controlled" now depends on copying it back into the repo deliberately
rather than on the file being tracked where it sits.

## 3. Retired requirements — do not rebuild these

### 3.1 The session-start cold-soak thermal sample

**Retired whole by owner decision, 2026-09-10, after being implemented.** The
`commonTemperature` record, the `settling*` config keys and the drift fit are all gone. Git
history and plan revisions up to rev 71 still describe it, so this note exists to stop the next
agent rebuilding it from either.

**Why it went, because the reasoning is worth more than the feature.** The requirement was to
*flag* whether the car looked settled rather than assert it, and settledness was inferred from
each probe's drift rate across a 90 s window. That inference does not work, and it was measured
failing: a car parked ~5–6 h drifts about 11 mK/min, which over 90 s is ~18 mK against the
DS18B20's 62.5 mK code step — so **zero code transitions, a fitted drift of exactly 0.0 mK/min,
and `spreadUsable: true`** while the bay still held an 810 mK real gradient that would have gone
straight into ΔT_preheat.

Note what that rules out: **no threshold fixes it**, because the reported drift is exactly zero
rather than merely small, and resolving 11 mK/min needs a 20–30 minute window, which is not a
session start. Drift *rate* and level *gradient* are independent quantities, and inferring "no
gradient" from "no drift" was the error.

**Nothing is lost by dropping it.** Every session already logs all four probes' absolute readings
at 1 Hz, so a cold soak the owner *knows* was a cold soak is still fully derivable from the
ordinary `temp` records by hand. What went was the automatic 90 s summary and its unreliable
verdict, not the calibration.

### 3.2 The binding store's per-probe `offsetC` key

Carried from the store's first version, on the reasoning that thermal item 1 needed somewhere to
put offsets and retrofitting the field later would be a format change. **Retired by owner
decision, 2026-09-10**: the key is gone from the store rather than left dead, because two fields
that look like an offset, one of which does nothing, is worse than one. See §2.4 for where the
offsets went.

## 2026-09-10 — the first road test, and the frozen phone

The first road test connected RaceChrono, received one set of frames — sometimes only a partial
set — and then froze: the app held that frame set indefinitely and nothing further arrived.

**Root cause: the logger answered RaceChrono's subscription burst with an ATT error and lost the
whole burst.** Entering the logging regime, RaceChrono writes `deny all` and then one
`allow single` per configured channel. Recorded verbatim on the bench, in order: `0x7F0`, `0x420`,
`0x600`, `0x601`, `0x202`, `0x602`, `0x603`, `0x78`, `0x4FA`. Five of the nine are **box 1's CAN
frames** — RaceChrono asks every DIY device for the union of every packet ID it has channels for,
not for that device's own. The first command was `0x7F0`, which box 2 does not publish; the code
returned `BLUEZ_ERROR_REJECTED`; RaceChrono abandoned the remaining eight; and `deny all` had
already destroyed every notify timer. The phone then held its last frame forever, and the logger
looked connected and perfectly healthy from every angle.

**Why CAN-bus test mode worked and logging mode did not**, which is what the owner spotted and it
was the key to the whole diagnosis: test mode writes `allow all`, which carries no packet IDs and
therefore cannot be refused. The two regimes exercise different code, and only one of them had
ever run.

**Why nothing caught this earlier.** The ESP32 rig was the only prior proof of the protocol and its
own source says the filter is *deliberately not enforced* — it always notified everything. The
logger is the first implementation in this project that honours the subscription, so the path ran
for the first time on a moving car.

**The diagnosis was slower than it should have been, in a way worth recording.** A rejected filter
command produced no journal line and no session record, so the road-test file showed `deny all`
followed by nothing at all — indistinguishable from RaceChrono having gone quiet. Two hypotheses
were entertained and both were wrong: that the command lengths were mismatched (every observed
command matched the expected length exactly), and that the incoming packet ID was parsed with the
wrong endianness (`00 00 07 F0` big-endian is correct and matches the rig). **The fix that made the
diagnosis possible was recording every filter command raw before validating it**; the fault was
then visible in one bench connection.

**Comparing against KnurDash was decisive and also misleading for an hour.** KnurDash has the same
strict length checks and the same reject-on-unknown-frame, and it works daily — which briefly
argued the rejection could not be the cause. It works because the packet IDs RaceChrono asks for
are mostly its own; **it carries the same latent defect** and will fail identically the moment
box 2's IDs are requested ahead of its own.

**A second, independent defect found in the same comparison.** The BLE worker created its D-Bus
connection and adapter *before* pushing its main context thread-default, copying KnurDash's order.
KnurDash gets away with it because `gtk_main()` iterates the global default context; a headless
logger has no such loop, so `onPoweredStateChanged` and `onCentralStateChanged` never fired at all.
Every session file before this date is missing its BLE connect and disconnect records for that
reason — not because nothing connected. It also meant a link lost without an explicit unsubscribe
left `isNotifying` set and advertising unstarted. Fixed by pushing the context first; the connect
and disconnect records appeared for the first time on the next run.

**Measured after the fix:** RaceChrono subscribed to `0x600`, `0x601`, `0x602` and `0x603` by
packet ID and each notified at 0.98–1.00 Hz across a 57 s session, with the five unknown IDs logged
and ignored. `0x604` took zero notifications because no channel is defined for it — correct
behaviour under an honoured filter, and the reason the heartbeat channel needs defining before it
can serve as the liveness indicator it exists to be.

## 2026-09-11 — the same defect in KnurDash, and what it says about the two boxes

The reject-on-unknown-packet-ID fault was fixed in `KnurDash` as well
(`github.com/chrumck/KnurDash`, commit made on the box itself). It is recorded here because of
what it revealed rather than because this repository owns it.

**Box 1 was already suffering from it and the cause was a change to box 2.** RaceChrono keeps one
channel set and asks every DIY device for all of it, so the moment `0x600` and `0x601` were defined
for the logger, KnurDash started being asked for two IDs it does not publish — and refusing the
first one cost it every subscription behind it. Applying the captured burst to KnurDash's frames
predicts `0x7F0` and `0x420` live with `0x202`, `0x78` and `0x4FA` frozen, and the owner confirmed
from the car that some readouts were live and some frozen. **Neither box's symptom was traceable
to its own code changes**, which is the part worth remembering: the two repositories are
independent, the phone's channel set is not.

**One thing was deliberately not carried across.** KnurDash creates its D-Bus connection and
adapter before pushing its worker's main context thread-default, exactly as this repository did
before 2026-09-10 — but it is a GTK app whose `main.c` calls `gtk_main()`, so the global default
context is iterated and its adapter callbacks fire. Copying this repository's fix there would
change working behaviour for no reason. Its commit message says so, in case a future reader
notices the asymmetry and tries to "align" them.

**KnurDash's fix was compiled and committed but unproven** when this was written. Its CAN hardware
stayed in the car, so the binary restarted continuously on the bench and could not be exercised.
Both boxes went to the car together, and the entry below is the result.

## 2026-09-11 — the second road test: the fix holds, and the next three faults are on the phone

Both boxes connected to RaceChrono on a 143 s, 825 m drive (peak 80.6 km/h, 46 % of samples moving).
Box 2's session is `2026-09-10T22-47-40.448829Z-log.ndjson` — the name is stamped from the box's
wrong clock as usual and means nothing.

**The subscription fix holds on both boxes.** Box 2 was asked for the same nine IDs as before, in
the same shape (`deny all`, then nine `allow single` inside 400 ms), published four and **logged and
ignored the other five**. It sent 141/141/139/139 notifications on `0x602`/`0x603`/`0x600`/`0x601`
across 140 s connected; RaceChrono recorded 137/137/135/135, the shortfall being the tail after the
phone stopped recording, 4.5 s before the disconnect. **Both per-cycle counters advanced by exactly
+1 across every recorded sample — zero drops.** Notify intervals median 1001–1006 ms, max 1084 ms.
One connect, one disconnect, no supervision drops. **Box 1 kept all twelve of its channels alive to
the last sample**, which is the first time its fix has run against a phone.

**What the session proved about this code, beyond the filter.** `throttled` live and sticky both 0
over 559 s, no undervoltage — the supply worker's first run in a moving car, though in a cabin and
unloaded, so the SoC figures are not comparable with anything the enclosure will produce. The I2C
retry behaved exactly as characterised on the bench: **550 first-attempt failures, 550 recovered, 0
exhausted over 550 cycles**, one per sample cycle, with zero BME280 read errors. And `0x600`,
`0x601` and `0x603`'s channel equations are confirmed end to end, by decoding the RaceChrono
recording against this logger's record of the same samples.

**Three faults found, all of them on the phone.** `0x604` has no channel and was never asked for;
one of `0x602`'s four is `bytesToUint` where `bytesToInt` belongs; `0x601` bytes 6–7 has no channel.
`CLAUDE.md` §"The phone's channel list is part of the instrument" carries the live rules. The one
worth restating here is **how the signedness fault stayed hidden**: the same sentinel check passed
on 2026-09-09 reading −327.68 on all four, and the channel list is hand-edited between sessions, so
a check that passed once was quietly no longer true. Nothing in this repository can see it.

**Two method notes earned here.** The session-cycle counter appears in both records, so a
BLE-connected session aligns to RaceChrono exactly by joining on cycle number — no
cross-correlation, no lag, verified as logger `sessionUs 166.8` ↔ phone `t = 7.3 s`. And **the
session ended in a hard cut** — no `BLE stopped` event, no closing record — which is what a pulled
fuse looks like when reading a file back.

**What it did not do, and this is the part with a lesson in it.** No probes were attached, so there
is no `probeCapabilities` record and nothing for the bulk-read question. And **the box was on the
passenger seat** (owner, asked after the write-up): every reading from the session is therefore
void, `enclosurePressurePa` and `cavityTemperatureC` having measured the cabin, and the clean link
is a best case at ~0.5 m rather than evidence about a wheel-well cavity. Commissioning items 5a and
5.7 do not move.

**The code results survive that intact and the readings do not, which is the useful line.** The
filter behaviour, the notification accounting, the three phone-side channel faults, the equation
confirmation and the cycle-counter alignment are all properties of software and protocol; the cabin
exercises them as well as the cavity would. **A cabin pressure record was written up as a cavity Cp
point and withdrawn within the same session** — it was strongly speed-correlated and entirely
plausible. Nothing this repository logs distinguishes the two cases, and nothing it could log
would: `../ndLouvers/` Step 0b commissioning item 2 now requires the mounting state to be recorded
by hand, per session.

## 2026-09-11 — the phone's channel list, written down and made auditable

The three faults found by the second road test were fixed on the phone the same day: `0x604`
defined, `0x601` bytes 6–7 defined with that packet's slots renumbered 50–53 → 51–55, and the
unsigned thermal channel corrected. **None has been seen working yet**; `../ndLouvers/` open item 45
carries the verification.

**How the channel ids decode, which is what made the audit possible.** RaceChrono names each
channel's sample file after the channel's numeric id, and that id is `slot × 2²⁰ + channelType` —
Digital 70537, Temperature 70539, Pressure 70541, Percent 70547 for the four types this logger
uses. The owner supplied the slot names he had picked, and the decode matched field for field
across all sixteen channels, which turned rev 86's inference ("the second of the four, by field
order") into an identification: **`Temperature Front 2`**, i.e. `0x602` bytes 2–3, the `temp1` /
`T_core_in` slot.

**`Tools/rcz-channels.py` is that decode.** It flags a `Temperature` slot reading +327.68, which is
the sentinel decoded unsigned, and it *reports* a channel whose samples are all `NaN`. Run it on the
first recording after any edit to the phone's channel list.

**The all-`NaN` report was written as a fault flag and had to be demoted the same day.** Two of
KnurDash's channels come back empty in every recording, and the assistant read that as a
frame-length fault — `lowPass(E,254)` and `lowPass(F,254)` reference bytes 4 and 5 of `0x7F0` while
the four working channels on that packet use bytes 0–3, so "the frame is four bytes long" fitted
every observation. **It was wrong** (owner, 2026-09-11): RaceChrono renders a deliberate
out-of-calibration-range marker as no value, those sensors were out of range throughout, and the
channels are working as designed. **An empty channel and a mistyped equation are indistinguishable
in an export**, so the tool now reports rather than accuses. The general form of the mistake is
worth keeping: a hypothesis that explains every observation is not thereby correct, and this one
was reachable only by asking what the sensor was doing.

**Two things about slots that are easy to get wrong.** Changing a slot's *type* changes its id, so
`Digital Front 15` and `Temperature Front 15` are different channels and the old one survives unless
deleted — still subscribed, still decoding the same bytes under the old name. And **an old recording
carries the slot numbers that were in force when it recorded**, so a disagreement with `README.md`'s
slot map means the list has moved since, not that either is wrong.
