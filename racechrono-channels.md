# RaceChrono channel and packet reference

This file owns packet layouts, equations and the phone slot map.
[ble-protocol.md](ble-protocol.md) owns subscription behavior and export auditing;
[RaceChrono profile restoration](RaceChrono/README.md) owns importing the saved profile.

## RaceChrono channels — what to type into the phone

RaceChrono channel definitions are entered by hand, so they live here. Add the device from inside
RaceChrono (**Settings → other devices → add a DIY device**), **not** from the phone's Bluetooth
pairing screen: this is a BLE GATT peripheral with no bonding, advertising `BR/EDR Not Supported`,
so the OS pairing list will never show it.

**Every payload field is big-endian. Only the 4-byte packet ID is little-endian**, per the DIY API.

> **A packet with no channel definition is never sent.** The logger honours RaceChrono's
> subscription, so entering the logging regime means only the packet IDs you have defined channels
> for get notified. Measured 2026-09-10 on the bench and again in the car on 2026-09-11, when
> `0x604` took **zero** notifications because no channel was defined for it while the other four
> ran at ~1 Hz. The first five packets were defined on 2026-09-11 and **`0x605`–`0x607` on
> 2026-09-19**, so all eight are now subscribed.
> In CAN-bus test mode RaceChrono asks for everything instead, which is why test mode shows
> channels that logging mode does not.
>
> **Define at least one free-running counter, whichever it is.** `0x604` byte 7 is the heartbeat,
> but `0x601` and `0x603` bytes 4–5 are per-cycle counters with the same property, and the second
> road test's zero-drop measurement was made off those two with `0x604` absent.

### The slot map — what to re-enter after a phone wipe

**RaceChrono channels are hand-typed into the phone and exist nowhere else.** The tables below say
which bytes carry what; this one says which predefined channel slot each field was put in, because
a slot is what a gauge, a lap chart or an exported CSV is named after. **Losing it means re-picking
36 slots, and re-picking slots is how one of them ended up decoding signed data as unsigned.**

Slots as of **2026-09-19**, and **verified against RaceChrono's own exported profile**, which is
committed at `RaceChrono/vehicleProfile.json` — that file is the authority on what is entered, this
table is the readable view of it. Packet IDs are decimal, which is what the CAN-ID field takes.

| Packet | Bytes | RaceChrono slot | Carries |
|---|---|---|---|
| 1536 `0x600` | 0–3 | `Pressure Front 50` | enclosure pressure — `/1000` for kPa, displayed in bar |
| 1536 | 4–5 | `Temperature Front 50` | cavity temperature |
| 1536 | 6–7 | `Percent Front 50` | enclosure humidity |
| 1537 `0x601` | 0 | `Digital Front 51` | BME280 status bits, expect 31 |
| 1537 | 1 | `Digital Front 52` | chip ID, expect 96 |
| 1537 | 2–3 | `Digital Front 53` | read errors |
| 1537 | 4–5 | `Digital Front 54` | sample cycles — free-running |
| 1537 | 6–7 | `Digital Front 55` | last read, ms |
| 1538 `0x602` | 0–1 | `Temperature Front 1` | `temp0` |
| 1538 | 2–3 | `Temperature Front 2` | `temp1` |
| 1538 | 4–5 | `Temperature Front 3` | `temp2` |
| 1538 | 6–7 | `Temperature Front 4` | `temp3` |
| 1539 `0x603` | 0 | `Digital Front 1` | probes enumerated |
| 1539 | 1 | `Digital Front 2` | valid-this-cycle mask |
| 1539 | 2–3 | `Digital Front 3` | read errors |
| 1539 | 4–5 | `Digital Front 4` | sample cycles — free-running |
| 1539 | 6–7 | `Digital Front 5` | last conversion, ms |
| 1540 `0x604` | 0 | `Digital Front 11` | logger status bits |
| 1540 | 1 | `Digital Front 12` | sticky throttle bits |
| 1540 | 2 | `Digital Front 13` | undervoltage comparator |
| 1540 | 3–4 | `Digital Front 14` | SoC core mV |
| 1540 | 5–6 | **`Temperature Front 15`** | SoC temperature |
| 1540 | 7 | `Digital Front 16` | **heartbeat** |
| 1541 `0x605` | 0–1 | `Pressure Front 1` | `P0` |
| 1541 | 2–3 | `Pressure Front 2` | `P1` |
| 1541 | 4–5 | `Pressure Front 3` | `P2` |
| 1541 | 6–7 | `Pressure Front 4` | `P3` |
| 1542 `0x606` | 0–1 | `Pressure Front 5` | `P4` |
| 1542 | 2–3 | **not defined** — see note 5 | `P5` |
| 1542 | 4–5 | `Digital Front 21` | sample cycles — free-running |
| 1542 | 6–7 | `Digital Front 22` | last cycle, ms |
| 1543 `0x607` | 0 | `Digital Front 23` | channels enabled, bitmask |
| 1543 | 1 | `Digital Front 24` | valid-this-cycle mask |
| 1543 | 2–3 | `Digital Front 25` | read errors |
| 1543 | 4–5 | `Digital Front 26` | CRC failures |
| 1543 | 6 | **`Temperature Front 20`** | sensor temperature, lowest enabled channel |
| 1543 | 7 | `Digital Front 27` | mux channel selected |

Five things this table is carrying rather than repeating:

1. **`Temperature Front 15` is a Temperature slot on purpose.** It was a Digital one until
   2026-09-11. It is the only `0x604` field with decimals; the rest are integers, and **SoC core
   millivolts stays Digital because RaceChrono has no voltage slot** — no precision is lost, the
   gauge simply carries no unit.
2. **Changing a slot's type changes its identity.** `Digital Front 15` and `Temperature Front 15`
   are different channels with different ids, so a retyped field leaves the old slot behind unless
   it is deleted — still subscribed, still decoding the same bytes under the old name.
3. **The pressure channels are `Pressure Front 1`–`5` and their status fields are `Digital
   Front 21`–`27` plus `Temperature Front 20`** — chosen to sit clear of every slot above, so no
   existing channel is disturbed and a `.rcz` from before 2026-09-19 can be told apart from one
   after it by their presence alone.
   **`0x607` byte 6 is `Temperature Front 20`, not the 21 this table specified until 2026-09-19.**
   The slot entered on the phone won, as the profile always does; 20 was free, satisfies the same
   clear-of-everything property, and `Temperature Front 21` and `Digital Front 21` would have been
   distinct channels anyway, so nothing was at stake either way. **Do not re-pick it to match an
   older table** — deleting and re-entering a channel by hand is the operation that produced the
   `bytesToUint` fault on `Temperature Front 2`, and it would buy nothing.
4. **`0x601` was renumbered from 50–53 to 51–55 on 2026-09-11**, when byte 6–7 was added. **Any
   recording made before that carries the old slot numbers**, so a session and this table can
   disagree without either being wrong. `Tools/rcz-channels.py` prints what a given recording
   actually used — **and a session that was paused and resumed carries one fragment per stretch**,
   which the tool reports separately. Reading only the archive root silently discards every later
   stretch; that is half of the 2026-09-13 track day.
5. **`P5` has no channel on the phone, and that is a decision** (owner, 2026-09-19). Mux channel 5
   is unpopulated — its pull-ups are footprints only and addressing it hangs the bus — so the
   worker never enables it and `0x606` bytes 2–3 carry the `−32768` sentinel permanently. An
   undefined field inside a *subscribed* packet costs nothing: `0x606` is sent for its other three
   channels and the phone simply discards these two bytes. **This is not the open item 47 mistake
   and must not be filed as one** — that was an undefined byte on a frame nobody could see was
   missing, carrying data that existed. Here the packet is visible, the field is a constant, and
   the logger's own `0x607` byte 0 states that channel 5 is disabled. **If mux channel 5 is ever
   populated, define `Pressure Front 6` as `bytesToInt(raw,2,2)/10000` before the first session
   that uses it** — the data is unrecoverable afterwards.

**`0x602` and `0x603` are transcribed byte for byte from the ESP32 rig in
`../ndLouvers/step0b-rig/racechrono_ble_test/`, so definitions written against that rig carry over
unchanged. `0x600` and `0x601` do NOT** — they were the rig's synthetic test frames and were
released for real use (owner decision, 2026-09-10), so a rig-era `0x600` definition decodes
garbage here and must be re-entered.

### `0x600` — enclosure conditions

**These two IDs used to be the ESP32 rig's synthetic test frames and no longer are** (owner
decision, 2026-09-10). That rig is spent, so the IDs were released for real use. **A channel
definition written against the rig's `0x600` decodes garbage here and must be re-entered.**
`0x602`–`0x604` are unchanged and still carry over.

| Bytes | Channel | Equation as deployed | Invalid |
|---|---|---|---|
| 0–3 | enclosure pressure, **kPa on the wire, displayed in bar** | `bytesToUint(raw, 0, 4) / 1000` | `4294967295` → `4294967.295`, i.e. ~42 950 bar |
| 4–5 | cavity temperature | `bytesToInt(raw, 4, 2) / 100` | `-32768` → −327.68 °C |
| 6–7 | enclosure humidity, % | `bytesToUint(raw, 6, 2) / 100` | `65535` → 655.35 % |

**The logger sends pressure in whole pascals and the `/1000` is the phone's, because RaceChrono's
Pressure channel takes kPa and this gauge is set to display bar** (owner, 2026-09-11). So 101 319 Pa
goes out, RaceChrono stores 101.319 and renders **~1.013 bar**. This table documented the undivided
form until the saved vehicle profile showed what was actually entered. The divide is fine and the
invalid marker survives it — ~42 950 bar is no more plausible than 4294967295 Pa — but **the packet
and the channel must be read together**, which is the reason the profile is now backed up in
`RaceChrono/`.

**Temperature is signed — use `bytesToInt`.** Pressure and humidity are unsigned and use
`bytesToUint`; pressure needs the full four bytes because absolute pressure does not fit in two
at 1 Pa resolution. Each invalid marker is absurd after the divide for the same reason `0x602`'s
is: RaceChrono holds the last value it received indefinitely.

**Name these three channels for their roles, not for the part**, because two of the three are
easy to point at the wrong thing:

1. **Pressure is ENCLOSURE pressure and never a static reference.** The cavity is
   aerodynamically live: **measured at 156 Pa below stationary at a mean 116 km/h on the first
   drive (Cp ≈ −0.25)**, which is still 2–3× the 45–90 Pa measurands, is not a single constant Cp,
   and is speed-correlated so it does not average out of a speed sweep. **That is not the only cavity
   record, and the records disagree**: a second cavity record from the third drive
   corroborates the sign and order without being a Cp point, because the route's elevation change
   is the same order as the signal and no altitude channel was exported. **Two track days then add
   SEVEN sessions and they CONTRADICT the first drive** — Cp −0.108 to −0.144, flat to within ±0.006
   from 40 to 200 km/h, on circuits with 8–14 m of altitude span and the altitude channel present;
   `../ndLouvers/` open item 49 owns the disagreement, where the plan's rev 92 review arithmetic
   (12 Pa per metre of GPS altitude error against a 94 Pa signal at 45 km/h) recommends the track
   result, and it is not yet decided. The second drive's box
   was on the passenger seat, so its
   speed-correlated pressure record is a cabin record and was withdrawn.
   **This field is named for where the box is designed to sit, not for where it actually sat**, and
   nothing in the session file says which. Record the mounting state per session. It is tolerable as a
   density term and disqualifying as a reference (`../ndLouvers/thermals-testing.md` §3.6).
2. **Temperature is the cavity thermometer** (plan item 1d, the hot-day envelope), with Pi SoC temperature on `0x604`
   as a cross-check rather than the primary proxy. **It is not the inlet density term** — that is
   `T_ambient`'s DS18B20 on `0x602` byte 0–1.
3. **Humidity is the enclosure's condensation diagnostic** — the enclosure is sealed with no
   desiccant (plan item 1c, closed). Its measurement consumer is the dewpoint margin
   (`../ndLouvers/thermals-testing.md` §3.10).

### `0x601` — BME280 health

| Bytes | Content | Equation | Expected |
|---|---|---|---|
| 0 | bit 0 present, bit 1 calibration read, bit 2 pressure valid, bit 3 temperature valid, bit 4 humidity valid | `bytesToUint(raw, 0, 1)` | **31**; **3** on a skipped measurement |
| 1 | chip ID as read | `bytesToUint(raw, 1, 1)` | **96** (`0x60`); `88` would be a BMP280 |
| 2–3 | cumulative read errors, saturating | `bytesToUint(raw, 2, 2)` | **0** |
| 4–5 | sample cycles | `bytesToUint(raw, 4, 2)` | +1 per second, wraps every **18.2 h** |
| 6–7 | last read, ms | `bytesToUint(raw, 6, 2)` | **~15** |

**This is `0x603`'s argument applied to the BME280, and it matters more here.** A sealed cavity's
temperature and pressure legitimately sit still for minutes, so a frozen `0x600` is not by itself
evidence of anything. Byte 4–5 advances every cycle regardless of what the part reports, which is
what separates a dead worker from a still cavity.

> **⚠ BYTE 0 IS WHAT CATCHES A SKIPPED MEASUREMENT, AND BYTES 2–3 DELIBERATELY DO NOT.** Measured
> once in the field, 2026-09-14: one cycle sent all three `0x600` values as their invalid sentinels
> together — which is the `t_fine` rule, a skipped temperature invalidating pressure and humidity —
> and **byte 0 dropped 31 → 3**, keeping only *present* and *calibration read*. **The read-error
> counter correctly stayed at 0**: the transfer succeeded and `lastReadMs` was normal, so nothing
> failed to read. **A cumulative read-error count of 0 is therefore not evidence that every sample
> was valid.** First time byte 0 has read anything but 31 in the field.

### `0x602` — the four thermal channels

| Bytes | Channel | Equation |
|---|---|---|
| 0–1 | `temp0` | `bytesToInt(raw, 0, 2) / 100` |
| 2–3 | `temp1` | `bytesToInt(raw, 2, 2) / 100` |
| 4–5 | `temp2` | `bytesToInt(raw, 4, 2) / 100` |
| 6–7 | `temp3` | `bytesToInt(raw, 6, 2) / 100` |

**Signed — use `bytesToInt`, not `bytesToUint`.** Sub-zero ambient is a real reading and would
otherwise decode as ~655 °C. A channel with no trustworthy reading sends `-32768`, i.e. **−327.68 °C**,
deliberately absurd rather than plausible because RaceChrono holds the last value it received
indefinitely and an invalid marker has to be visible.

> **⚠ `Temperature Front 2` was defined `bytesToUint`; it is fixed and VERIFIED** (owner,
> 2026-09-11 — with the sensor disconnected the channel reads negative, so the signed decode is in
> force). Keep the whole entry: the fault class is permanent even though this instance is closed.
> Found 2026-09-11: three of the four sentinels decoded as −327.68 and that one as **+327.68**. It
> is `0x602` bytes 2–3, i.e. **`temp1`, the `T_core_in` slot — the probe ΔT_preheat is measured
> from**. **The fault is invisible in normal data**, because a positive temperature decodes
> identically either way, and **nothing on the logger can detect it**. The only ways it shows are
> the sentinel and a sub-zero ambient, so the check below is the check — and it only works with no
> probes attached, which is the condition to confirm the fix in.

**These values carry the per-channel calibration offset** from `KnurLogger.ini`, if one is set — see
§[Calibration offsets](operations.md#calibration-offsets). The session file records the raw reading beside the
value sent, so the two can always be reconciled.

**With no probe on the bus all four send the sentinel, and the packet keeps being republished once
per sample cycle.** Unbound, bound-but-absent and read-badly all send the same sentinel, which is
right — none of them is a temperature — and the session file is where the three are told apart. To
see *which* it is from the phone alone, watch `0x603`: byte 0 is how many probes are on the bus and
byte 1 is which channels read cleanly.

### `0x603` — 1-Wire bus health

| Bytes | Content | Equation | Expected with four probes |
|---|---|---|---|
| 0 | probes enumerated | `bytesToUint(raw, 0, 1)` | **4** |
| 1 | valid-this-cycle bitmask, bit *n* = `temp<n>` | `bytesToUint(raw, 1, 1)` | **15** |
| 2–3 | cumulative read errors, saturating | `bytesToUint(raw, 2, 2)` | **0**, and staying there |
| 4–5 | sample cycles | `bytesToUint(raw, 4, 2)` | +1 per second, wraps at 65535 — i.e. every **18.2 h** |
| 6–7 | last conversion, ms | `bytesToUint(raw, 6, 2)` | **~780 with four probes** — one bulk conversion covering all four. It read ~3200 before 2026-09-11, when each probe paid its own |

Transcribed byte for byte from the ESP32 rig's `0x603`, so channel definitions written against that
rig carry over. **This is what makes the thermal channels checkable rather than merely present:**
four plausible numbers on `0x602` say nothing about whether they are being read, and bytes 0–3 are
where a probe that dropped off the bus or a bus that is retrying shows up. Byte 4–5 is the thermal
equivalent of `0x604`'s heartbeat — it advances every cycle regardless of what the probes read, so
it separates a dead worker from four steady temperatures.

> **⚠ BYTE 2–3 COUNTS *UNTRUSTED SAMPLES*, NOT BUS FAULTS.** A CRC failure, unparseable content,
> a failed read and an out-of-range value all increment it, and only the session record's `reason`
> separates them, so **a rising counter is a reason to read the session file, not a diagnosis.**
> **In sessions recorded before 2026-09-23 it also counted every 85.000 °C reading** as
> `powerOnDefault` — every one of the 17 it counted across the two track days was `temp3` at exactly
> 85.000 °C with a **good CRC**, while the loaded star recorded **zero** CRC failures in 133 894
> cycles. The logger now accepts 85.00 °C as a reading (`one-wire-probes.md`).

**Byte 2–3 counts only probes that answered and read badly.** A bound channel whose probe is *absent*
does not increment it, because a dropped lead and a marginal bus send you to different parts of the
car; absence shows up as byte 0 falling and byte 1 losing a bit.

**A probe that has just been unplugged is `absent` for this purpose too, for ~100 s before it looks
it.** The kernel keeps an unregistered slave's sysfs entry for `w1_slave_ttl` (10) missed searches
at `w1_master_timeout` (10 s), and reads of it succeed and return nothing — so byte 0 stays up,
byte 1 loses its bit, and byte 2–3 stays put. That last part was a bug until 2026-09-10, when
unplugging probes during enrollment charged 186 read errors to a bus that had not failed once
(`CLAUDE.history.md` §1.12). The session file counts these separately as `notAnswering`, so a bus
that really is dropping out is still visible; it is just not confused with one that read badly.

**Byte 6–7 reads ~780 with four probes bound**, because one bulk conversion now covers all four;
it read ~3200 while the reads were sequential. **It read 0 for one session**, which is how the
first fault was found: the field reported the bulk wait whenever the *write* succeeded, so a 0 ms
wait masked ~800 ms of real conversion. It now reports what the cycle actually paid — timed from
**before** the write, because the kernel sleeps the whole conversion inside it — and the `temp`
records carry `bulkState` (the raw `therm_bulk_read` readback, `"1"` when healthy) beside it.
**A ~0 ms reading with cycles still near 1 s means the bulk path ran and its bus reset failed:**
the kernel marks every slave ready regardless, so check byte 2–3 before trusting the temperatures.

**Confirmed on the phone 2026-09-11.** All five channels are defined and decoded correctly against
the logger's own record of the same samples, with no probes attached: probes 0, valid-mask 0, read
errors 0, sample cycles ramping at 1 Hz, conversion 0 ms.

### `0x604` — supply health and logger liveness

| Bytes | Content | Equation |
|---|---|---|
| 0 | low nibble = live throttle bits; bit 4 = records dropped; bit 5 = enrollment mode | `bytesToUint(raw, 0, 1)` |
| 1 | sticky throttle bits, latched since **boot** not since session start — **observed at 5 across both track days**, `undervoltage` + `throttled`, dated from the SD file to the first 13.2 s after boot | `bytesToUint(raw, 1, 1)` |
| 2 | undervoltage comparator; **255 = could not be read**, not "no alarm" | `bytesToUint(raw, 2, 1)` |
| 3–4 | SoC core millivolts — **NOT the supply rail** | `bytesToUint(raw, 3, 2)` |
| 5–6 | SoC temperature | `bytesToInt(raw, 5, 2) / 100` |
| 7 | **heartbeat, +1 per second, wraps at 255** | `bytesToUint(raw, 7, 1)` |

> **⚠ This frame went unsent through two road tests** — no channel was defined for it, so
> RaceChrono's subscription burst never asked for it and the logger recorded `"supply": 0`
> notifications. **Defined 2026-09-11 and CONFIRMED the same evening** on the third drive — all six
> channels decoded, and byte 7's heartbeat stepped exactly +1 across all 410 samples. The slot map
> above has where the six rows went. Re-confirm from either end after any edit to the phone's
> channel list: byte 7 ticking +1 per second on the phone, or `bleNotifiesByPacket` in any
> `supply` record showing `supply` climbing instead of 0.

**Byte 1 is the one to read after a run, and byte 7 during it.** Byte 1 latches since **boot**, so
a session that starts with it non-zero is reporting something that happened before the recording
opened, and **no BLE record can ever date that** — `recordStickyTransitions` timestamps the first
transition of each bit and `stickyAtSessionStart` says whether it was inherited, and both are in
the SD session file only. **That is exactly the case the track days produced, and reading the SD
file is what settled it:** `stickyAtSessionStart` was already 5 at `bootUs` 13.17 s, so the latch
is a power-on transient inside the first 13 seconds (`../ndLouvers/` open item 48, closed). The general
lesson is the one to keep — **a non-zero byte 1 is a question the phone cannot answer.**

**Byte 7 is the channel to watch while the link is live.** Every other field here is a physical
quantity allowed to sit still — SoC core voltage reads a constant 840 mV on an
idle box for hours — so a frozen value proves nothing about the link. A counter freezing means the
link died; a counter skipping means a notification was dropped, which is exactly what commissioning
item 5a asked to be logged. **It is not the only counter with that property**: `0x601` and `0x603`
bytes 4–5 advance every cycle regardless of what their sensors report, and the second road test's
zero-drop measurement was made off those two.

### `0x605` and `0x606` — the six pressure channels

**Nothing on these channels is a pressure measurement.** They carry what the five SDP810s report
with every port open to whatever air is around them; the pneumatic rig — wands, tubing, filters,
drainage — is unbuilt, and `../ndLouvers/pressure-testing.md` §2 is the qualification none of this
discharges. What exists is the channel a measurement will one day travel down.

| Packet | Bytes | Channel | Equation | Invalid |
|---|---|---|---|---|
| 1541 `0x605` | 0–1 | `P0` | `bytesToInt(raw, 0, 2) / 10000` | `-32768` → **−3.2768 kPa** |
| 1541 | 2–3 | `P1` | `bytesToInt(raw, 2, 2) / 10000` | as above |
| 1541 | 4–5 | `P2` | `bytesToInt(raw, 4, 2) / 10000` | as above |
| 1541 | 6–7 | `P3` | `bytesToInt(raw, 6, 2) / 10000` | as above |
| 1542 `0x606` | 0–1 | `P4` | `bytesToInt(raw, 0, 2) / 10000` | as above |
| 1542 | 2–3 | `P5` | **no channel defined** — slot-map note 5 | as above |
| 1542 | 4–5 | sample cycles | `bytesToUint(raw, 4, 2)` | free-running, wraps at 65535 |
| 1542 | 6–7 | last cycle, ms | `bytesToUint(raw, 6, 2)` | **~16** for five sensors |

**The wire carries signed decipascals — 0.1 Pa/LSB — on all six channels** (owner decision,
2026-09-19; [ndLouvers instrumentation-spec.md](../ndLouvers/instrumentation-spec.md) commissioning item 4a). `int16` at 0.01 Pa/LSB overflows
at 327 Pa and cannot carry a ±500 Pa channel, so centipascals would have meant **two decode rules**
on a list nothing in this code can check — which is exactly how `bytesToUint` survived on
`Temperature Front 2`. One rule covers all six instead. The resolution thrown away on the ±125 Pa
part is affordable **because the session file carries raw counts and the returned scale factor at
full resolution**, so any session can be reprocessed; only a `.rcz` cannot, which is the asymmetry
that governs everything on this link.

> **⚠ THE `/10000` IS CORRECT AND THE NUMBER ON THE GAUGE IS THEREFORE SMALL. DO NOT SHORTEN IT.**
> RaceChrono's Pressure channel stores **kPa** — `Pressure Front 50` divides by 1000 for exactly
> that reason — and decipascals reach kPa through ten thousand. Full scale is then 0.5 kPa and the
> 45–90 Pa measurands this project is built around are 0.045–0.090, which a gauge set to bar
> renders as near zero.
>
> **The obvious shortcut does not work, and it was tried** (2026-09-19, withdrawn the same day).
> `/10` looks like it puts pascals on the gauge. It does not: RaceChrono still believes the stored
> number is kPa and applies **its own** unit conversion on top, so 500 Pa stored as "500 kPa"
> displays as **5 bar**. That is wrong twice — wrong unit *and* wrong magnitude — where `/10000` is
> merely small. **A divide that fights the app's unit handling produces a number that means
> nothing.** Keep `/10000`.
>
> **The divide has never been about resolution, and that was measured rather than assumed.**
> RaceChrono stores every sample as a **float64**: `Pressure Front 50`'s samples in the 2026-09-14
> export come back as `101.151`, `101.154`, `101.15000000000001` — an IEEE-754 double carrying full
> precision, 8 bytes per sample. A double would need a divide around 10¹² before it noticed
> anything. **`/10000` costs nothing in the recording**, which is the whole of what the logged and
> exported data depends on.
>
> **What it does cost is the at-the-car eyeball**, and that cost is real: these six channels will
> not be readable on a bar-scaled gauge. Two things soften it. **The sentinel check survives**,
> because it turns on the *sign* — −0.0033 bar against +0.0033 bar — not the magnitude. And if
> RaceChrono's pressure display unit can be set per gauge, setting these six to kPa or Pa makes
> them readable with no change to the equation; the channel definition carries no unit field, so
> the unit is an app setting and not something this document can fix. **The SD session file is the
> authority regardless** — raw counts, the returned scale factor and full-resolution pascals, none
> of which pass through the phone.

**Signed — use `bytesToInt`.** A negative differential is the normal case on half these channels,
depending only on which port the tube lands in, so an unsigned decode corrupts ordinary data rather
than only the sentinel. **`-32768` is the no-trustworthy-reading marker** and decodes to **−3.2768
kPa** — 6.5× the ±500 Pa part's full range and therefore impossible, which is the property an invalid
marker needs, because RaceChrono holds the last value it received indefinitely.

> **⚠ ZERO IS A COMPLETELY PLAUSIBLE DIFFERENTIAL PRESSURE, WHICH MAKES THE SENTINEL MATTER MORE
> HERE THAN ON THE THERMAL CHANNELS.** All six are published as `-32768` before the worker has read
> anything — five of them reaching the phone, `P5` having no channel — so a phone that connects
> first sees unmistakable non-readings rather than believable readings of nothing. A thermal channel
> reading 0 °C at least looks like weather; a pressure channel reading 0 Pa looks like a correct
> answer. **The sentinel check passed on all five with the sensors disconnected** (owner,
> 2026-09-19): every one read negative.

**`0x606` bytes 4–5 advance every cycle whatever the sensors report**, which is what separates a
dead worker from five steady pressures — and five steady zeroes is the resting state of a healthy
rig, so this counter is more necessary here than on `0x601` or `0x603`. At the measured 9.594 Hz it
wraps at 65535 every **1.9 h** rather than the 18.2 h those two take at 1 Hz.

**Channel names are positional and mean nothing.** `P0`–`P5` are fixed by mux position. **The
role→channel mapping is deliberately undecided** — `../ndLouvers/` open item 30a — and these
channels are published before it exists on purpose, so the phone-side definitions can be built and
verified **before** the rig does, which is the only way to stop open item 47 repeating on the
pressure side. Recording which *part* sits on which channel (the `pressureBaseline` record, and
`SystemSetup/logger-perfboard-wiring.md` §5) is not deciding which *role* it serves.

### `0x607` — pressure channel health

| Bytes | Content | Equation | Expected with five sensors |
|---|---|---|---|
| 0 | channels enabled, bitmask, bit *n* = `P<n>` | `bytesToUint(raw, 0, 1)` | **31** — channels 0–4 |
| 1 | valid-this-cycle bitmask, bit *n* = `P<n>` | `bytesToUint(raw, 1, 1)` | **31**, and staying there |
| 2–3 | cumulative read errors, saturating | `bytesToUint(raw, 2, 2)` | **0** |
| 4–5 | cumulative CRC failures, saturating | `bytesToUint(raw, 4, 2)` | **0** |
| 6 | sensor temperature of the lowest enabled channel, °C; **−128** when that channel has no valid reading this cycle | `bytesToInt(raw, 6, 1)` | bench ambient |
| 7 | mux control byte: **0** deselected, bit *n* set while `P<n>` is being read, **255** if the end-of-cycle deselect failed | `bytesToUint(raw, 7, 1)` | **0** |

**Byte 1 is how an invalid channel is seen from the phone without decoding the sentinel**, exactly
as `0x603` byte 1 works for the probes. Byte 0 beside it separates "that channel is switched off in
the config" from "that channel failed": a bit in 0 and not in 1 is a sensor that did not read.

> **⚠ BYTES 2–3 AND BYTES 4–5 ARE TWO COUNTERS ON PURPOSE, AND COLLAPSING THEM WOULD REPEAT A FAULT
> THIS PROJECT HAS ALREADY PAID FOR.** Read errors count *transfers that did not complete*; CRC
> failures count *frames that arrived and could not be trusted*. They send a reader to different
> parts of the box — the bus and the mux for one, the sensor and its wiring for the other. On the
> thermal side one counter conflating the two is what left a probe sitting at 85.000 °C
> misclassified as a bus fault across two track days.

**Byte 7 should read 0, and anything else is a stuck worker or a bridged bus.** The worker
deselects the mux at the end of every cycle and publishes 0. **A cycle that has not finished after
1 s publishes nothing of its own**, so the BLE worker sends `0x607` with byte 7 set to the channel
the pressure worker is stuck on — 1, 2, 4, 8 or 16 for `P0`–`P4` — while `0x606`'s cycle counter
stops. **255 means the deselect itself failed**, which is the state in which a faulty downstream
segment may still be bridged onto the main bus. The session record's `muxControl` field carries the
same value (−1 for 255).

**Byte 6 is the lowest enabled channel's sensor temperature only** — the same part every cycle, so
a trend on the phone always means one sensor — and it is a diagnostic for the part rather than a
measurement of anything: the SDP810's compensation spans −20…85 °C and this says whether it is
inside it. **−128 means that part gave no valid reading this cycle**; the byte never falls back to
another channel. Per-sensor temperature at full resolution is in the session file; there is
no room for five of them here, and no consumer for them on the phone.
