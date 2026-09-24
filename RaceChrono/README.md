# RaceChrono vehicle profile — the channel list, backed up

**`vehicleProfile.json` is RaceChrono's exported vehicle profile, with `localUuid` removed.**
It is the only copy of the phone's channel definitions outside the phone, and it covers **both
boxes**: the two devices share one RaceChrono channel set, which is why a channel added for one of
them can break the other.

**Why this is here at all.** Channel definitions are hand-typed into the app. The byte-level
specification is in `../racechrono-channels.md`; this file is the actual configuration, equations and predefined
channel slots included. Re-picking slots by hand is what produced the `bytesToUint`-where-
`bytesToInt`-belongs fault on `Temperature Front 2` (`../../ndLouvers/` open item 45), so a backup that
can be re-imported is worth more than a table that has to be re-typed.

**This directory holds the PROFILE, not recordings.** Session `.rcz` exports live outside both
repositories, in `C:\_claude\RaceChrono\` — `../../ndLouvers/thermals-testing.md` §2.5 item 4 owns
that and says how they differ from the SD session files. Sessions are cited by `.rcz` filename in
`../CLAUDE.history.md`; do not look for them here.

**Exported 2026-09-19**, from the profile named `AE30`. It is a snapshot, not a live mirror —
**the app is the authority**, and this file is stale from the moment a channel is edited.
Re-export after any change.

**This export is the one that carries `0x605`–`0x607`.** The pressure channels and their status
fields were entered on 2026-09-19 and verified the same day — the sentinel check with the sensors
disconnected, every channel negative, and `0x606`'s free-running counter stepping +1 — so all eight
of box 2's packets are now subscribed. It also carries the `0x420` byte 7 channel that
`../../ndLouvers/` open item 47 had been waiting for.

> **⚠ A `git diff` OF THIS FILE IS NOT A REVIEW OF IT.** RaceChrono emits `customChannels` in an
> order that shifts as entries are added, so a re-export rewrites lines that did not change — this
> one showed 184 insertions and 86 deletions for 14 added channels. **Diff it by `(pid, channelId)`
> instead**, decoding each id as `slot × 2²⁰ + channelType`, or an equation edited by hand will pass
> review unseen. That is the fault class this whole directory exists to catch.

## Restoring it

RaceChrono imports a `.rcz`, which is a zip with the profile at the archive root:

```bash
cd RaceChrono && zip ../vehicle.rcz vehicleProfile.json
```

**`localUuid` was removed before committing** — it is a 64-hex-character identifier tied to the
owner's install, not a channel definition, and this repository is public. Re-import has **not been
tested without it**; if the app refuses the file, paste the value back from the original export or
from an export taken after this one.

## What is in it

49 custom channels across thirteen packet IDs. **Box 2's are `0x600`–`0x607`, 36 channels**
(`../racechrono-channels.md` §"The slot map" is the human-readable version). **Box 1's are `0x78`, `0x202`,
`0x420`, `0x4FA` and `0x7F0`, 13 channels.**

**The IDs claimed across both boxes are therefore `0x78`, `0x202`, `0x420`, `0x4FA`, `0x600`–`0x607`
and `0x7F0`.** Check a proposed new packet ID against that list here, in this file, rather than
against either box's source: the two devices share one channel set, so an ID free on one of them is
not necessarily free.

Two of those 49 are not in the slot map and are worth naming, because each is a decision rather
than an omission:

1. **`P5` has no channel** — `0x606` bytes 2–3. Mux channel 5 is unpopulated, so the field is a
   permanent sentinel inside a packet that is subscribed anyway. `../racechrono-channels.md` slot-map note 5
   owns it, including what to do if channel 5 is ever populated.
2. **`0x420` byte 7 is now defined**, as `H-40` on predefined channel 10031, alongside byte 0's
   existing `A-40` on 10026. That is the outside-air temperature of `../../ndLouvers/` open item 47.
   Defining it fixes nothing retrospectively — no past recording can be reprocessed — so it counts
   only from the first session recorded after this export.

**Two things worth knowing before editing an equation by hand.**

1. **RaceChrono's equation parser is case-insensitive**, and this file proves it: `bytesToUint`,
   `bytesTouInt`, `bytestouint` and `bytesToUInt` all appear and all work. **Do not "normalise" the
   casing** — there is nothing to fix, and an edit is a chance to introduce the fault below.
2. **`bytestoint` and `bytestouint` differ by one letter, and that letter is the signed/unsigned
   decision.** In lowercase, scanning for it by eye is unreliable. `../Tools/rcz-channels.py`
   checks it from a recording instead — on `Temperature` slots at +327.68 and, since 2026-09-19,
   on `Pressure` slots at +3.2768. **The pressure channels need that check more than the thermal
   ones did**: a negative differential is normal on half of them, depending only on which port the
   tube lands in, so a wrong sign there corrupts ordinary data rather than only the marker.
   `Pressure Front 50` is exempt and must stay `bytesToUint` — `0x600`'s enclosure pressure is
   unsigned by design.

**`A`–`H` (or `a`–`h`) in an equation are payload bytes 0–7.**

**An all-empty channel is not necessarily a broken one.** RaceChrono renders a deliberate
out-of-range marker as no value at all. Box 1's `0x7F0` channels `lowPass(E,254)*4` and
`lowPass(F,254)-50` are `NaN` whenever their sensors are outside their calibration range, and
**that is by design** (owner, 2026-09-11): the marker is how that is signalled. They are working —
on the 2026-09-13 track day both read normally once the car was warm and were empty only for the
first 200–550 s of each session, so "empty" is a property of the session, not of the channel. `../Tools/rcz-channels.py` reports such channels without
calling them faults — **ask what the sensor was doing before changing an equation.**
