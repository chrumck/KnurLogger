# RaceChrono vehicle profile — the channel list, backed up

**`vehicleProfile.json` is RaceChrono's exported vehicle profile, with `localUuid` removed.**
It is the only copy of the phone's channel definitions outside the phone, and it covers **both
boxes**: the two devices share one RaceChrono channel set, which is why a channel added for one of
them can break the other.

**Why this is here at all.** Channel definitions are hand-typed into the app. The byte-level
specification is in `../README.md`; this file is the actual configuration, equations and predefined
channel slots included. Re-picking slots by hand is what produced the `bytesToUint`-where-
`bytesToInt`-belongs fault on `Temperature Front 2` (`../ndLouvers/` open item 45), so a backup that
can be re-imported is worth more than a table that has to be re-typed.

**This directory holds the PROFILE, not recordings.** Session `.rcz` exports live outside both
repositories, in `C:\_claude\RaceChrono\` — `../ndLouvers/thermals-testing.md` §2.5 item 4 owns
that and says how they differ from the SD session files. Sessions are cited by `.rcz` filename in
`../CLAUDE.history.md`; do not look for them here.

**Exported 2026-09-11**, from the profile named `AE30`. It is a snapshot, not a live mirror —
**the app is the authority**, and this file is stale from the moment a channel is edited.
Re-export after any change.

> **⚠ IT IS STALE NOW, AND DELIBERATELY SO.** The logger has published `0x605`, `0x606` and
> `0x607` — the six pressure channels and their status fields — since 2026-09-19, and **none of
> them is in this file, because none of them is on the phone yet.** Until they are entered, the
> three packets are never sent at all: RaceChrono's subscription burst never asks for them, which
> is exactly how `0x604` went unsent through two road tests. `../README.md` §"The slot map" and the
> `0x605`–`0x607` sections are what to type. **Re-export and re-commit this file once they are in**,
> and run the sentinel check with the sensors disconnected before trusting any of them.

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

35 custom channels across ten packet IDs. Box 2's are `0x600`–`0x604` (23 channels; `../README.md`
§"The slot map" is the human-readable version). Box 1's are `0x78`, `0x202`, `0x420`, `0x4FA` and
`0x7F0`. **Box 2's `0x605`–`0x607` are missing** — see the warning above; adding them takes it to
38 packets' worth across thirteen IDs, 14 channels more.

**No ID in this file collides with `0x605`–`0x607`**, which is what was checked before those three
were chosen: across both boxes the only IDs claimed are `0x78`, `0x202`, `0x420`, `0x4FA`,
`0x600`–`0x604` and `0x7F0`.

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
`lowPass(F,254)-50` are `NaN` in every sample of every recording so far, and **that is by design**
(owner, 2026-09-11): those sensors were outside their calibration range throughout, and the marker
is how that is signalled. They are working. `../Tools/rcz-channels.py` reports such channels without
calling them faults — **ask what the sensor was doing before changing an equation.**
