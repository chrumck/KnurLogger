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
ROM-ID-keyed. Do not "restore" ROM-ID keying. `channels.ini` no longer has an `offsetC` key at
all. The live consequence — an offset is a property of one particular DS18B20, so re-enrollment in
a different order or a swapped probe strands the calibration — is in `CLAUDE.md`.

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
