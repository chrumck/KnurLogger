# KnurLogger

Headless data logger for the ND Miata hood-louver instrumentation ("box 2"), running on a
Raspberry Pi 4B in the cavity behind the left wheel well.

It publishes differential pressure, temperature and enclosure conditions over Bluetooth LE as a
[RaceChrono DIY BLE device](https://github.com/aollin/racechrono-ble-diy-device), which is the
**primary data path**, and writes the raw readings and diagnostics to the SD card, which is the
durable record and the only thing that can prove a sample was missing rather than held.

**Status.** All five workers are written, the box has driven three times, and it has run **two
track days at Poznań (2026-09-13 and 2026-09-14)** — the first runs under sustained thermal load,
and the first to exercise the loaded 1-Wire star hot, vibrating and for a useful duration. Across
117 minutes and 10 118 thermal cycles in seven sessions: **zero dropped notifications on three
free-running counters independently, in every session**, 4 probes enumerated on every sample,
BME280 read errors 0, and **two single-cycle CRC failures on one probe**, each flagged three
independent ways.

**The logger ran continuously across both days** — 21.7 h of it unattended overnight with the fuse
left in, and the BME280 cycle counter lands 29 cycles (0.04 %) from the prediction, so there was no
restart. Seven further thermal read errors appeared during 2026-09-14's pit breaks and **none
during any recorded running**; together with 2026-09-13's two, which fell inside its stationary pit
soak, every read error on record so far happened with the car parked.

**One thing is not clean:** `0x604` byte 1 reads **5** on every sample of both sessions —
`undervoltage` and `throttled` latched since boot — while the live bits and the `rpi_volt`
comparator read 0 throughout. Nothing happened *during* either session, and **nothing in a `.rcz`
can date it**; `recordStickyTransitions` and `stickyAtSessionStart` can, and both live only in the
SD session file, which is still on the box. `../ndLouvers/` open item 48.

What each outing settled, because they are not interchangeable:

1. **First, 2026-09-10.** Its BLE observations are void — the logger refused RaceChrono's
   per-frame subscription burst with an ATT error and the phone froze on one frame set. Fixed;
   `CLAUDE.md` §"Never return an ATT error from the filter callback" owns it. Its SD record was
   unaffected and carries the first thermal analysis.
2. **Second, 2026-09-11.** Confirms that fix in the field on both boxes — the whole nine-command
   burst accepted, the five IDs box 2 does not publish ignored, 141/141/139/139 notifications over
   140 s with zero drops; box 1 (KnurDash), which had the same defect, kept all twelve of its
   channels alive. It also closed the `0x600`/`0x601`/`0x603` equation check and exposed three
   phone-side channel faults. **Its box was on the PASSENGER SEAT**, so every *reading* from it is
   void, the BME280's included — a cabin record (`../ndLouvers/thermals-testing.md` §3.7). Only the
   code and protocol results survive, and those do not depend on where the box sat.
3. **Third, 2026-09-11.** The first from the installed position — in the wheel-well cavity,
   enclosure closed, four probes attached, box 1 connected at the same time — and clean on every
   count it could be read on: 415 s, zero dropped notifications, valid-mask 15 throughout, cavity
   ~10 K over ambient. It is what closed `../ndLouvers/` commissioning item 5a.
4. **Track day, 2026-09-13.** The load case, above. It also supplies what item 5a was closed
   without — sustained speed to 190 km/h and a circuit's worth of steering and suspension
   loading — and the first matched plate-on/plate-off comparison
   (`../ndLouvers/thermals-testing.md` §3.9). **The cavity rise turns out to be an airflow
   effect:** ~2 K over ambient at track speed against the third drive's ~10 K on an urban route.
5. **Track day, 2026-09-14.** Six fragments, wet drying to dry, 07:30–14:05. Zero invalid thermal
   samples all day, and **one invalid BME280 sample that described itself exactly as specified** —
   all three values to their sentinels in the same cycle and the `0x601` status byte dropping
   **31 → 3**, with the read-error counter correctly unmoved, because a skipped measurement is not
   a failed transfer. **`0x601` byte 0 is the channel that catches this case, not bytes 2–3**, and
   this is the first time it has read anything but 31 in the field.
   Its enclosure result belongs to the plan rather than here, and it is the largest one so far:
   **the cavity reached 100 % RH with a zero dewpoint margin for 4.8 minutes**
   (`../ndLouvers/thermals-testing.md` §3.10).

**All three phone-side channel faults are fixed and all three are now observed working** — `0x604`
defined and decoding for the first time, `0x601` completed and renumbered, and `Temperature
Front 2` corrected from `bytesToUint`. Nothing in this repository can detect a fault in the phone's
channel list; `CLAUDE.md` §"The phone's channel list is part of the instrument" owns the rule,
which outlives these three instances.

**Sampling is 0.977 Hz since 2026-09-11**, via `therm_bulk_read`: one 762–790 ms conversion for all
four probes, 1023 ms mean cycle, zero CRC failures across 492 reads. Two conditions were needed and
neither alone was enough — `SystemSetup/grant-w1-bulk-read.sh` for the `EACCES`, and an **eight-byte**
trigger write, because `therm_bulk_read_store` gates on `size == sizeof("trigger")`. Parasite power
was the standing suspect and was wrong. `CLAUDE.history.md` §1.15.
**Every thermal reading taken before 2026-09-11 is at 0.31 Hz** and no reprocessing changes that.

**The four DS18B20s are enrolled** (2026-09-10, at the car) and the loaded 4 × 5 m star reads
CRC-clean — 63 consecutive cycles, valid-mask 15, zero read errors — which closes the plan's
thermal item 1. The map is independently confirmed by warming each probe in installed order.
**The cold-soak calibration is done and the answer is no offsets**: spread 0.193 K over 12.9
undisturbed minutes, ~3 % of a ~6 K ΔT_preheat signal and inseparable from a real spatial gradient,
so all four `temp<N>OffsetC` stay 0.0 — **a result, not an oversight**.

**A 7.53 h unattended bench run holds up**: 27,123 cycles, worst-case gap 1004 ms, zero gaps over
2 s, zero dropped records, `throttled` 0 throughout, 1457 B/s (~72 MB for a 12 h day against 108 GB
free). It was open-air and idle with no probes bound and `0 notifications sent`, so it exercises
neither BLE load nor the sealed enclosure.

**The host setup under `SystemSetup/` has been applied** and the box is key-only over SSH, on
kernel `6.18.39` / `bluez 5.82-1.1+rpt2` — the upgrade is what made BLE advertising work at all
(`CLAUDE.history.md` §1.2; the symptom points at the logger and the cause is not in it).
**The perfboard's sensor zone is assembled**, minus the five undelivered SDP810s, so an empty I2C
scan is no longer the correct result: the BME280 answers at **`0x77`**, now its specified address,
the **PCA9548A** mux answers at `0x70` since its `~RESET` was resoldered from header pin 9 to
pin 11, and the 1-Wire phantoms have stopped. `CLAUDE.md` has the facts to code against;
`CLAUDE.history.md` has the diagnoses behind them.

---

## What this is for

The measurement requirements, acceptance criteria and channel definitions live in
`../ndLouvers/CFD-Learning-Plan.md` Step 0b, which is the authority. This repository owns the
software and the host configuration that satisfy them, and owns no measurement decision.

Read, in this order, before changing anything here:

1. `../ndLouvers/CFD-Learning-Plan.md` — Step 0b, especially commissioning items 2, 4, 5a and 5b.
1a. `../ndLouvers/thermals-testing.md` — the thermal measurement companion: probe siting, the
   cavity BME280, the cold-soak method, clock alignment and every recorded thermal result. Read it
   before changing anything that touches `oneWireProbes.cxx` or `bme280Sensor.cxx`. It owns no
   requirement; Step 0b does.
1b. `../ndLouvers/pressure-testing.md` — the pressure measurement companion. **Nothing in it is
   built yet** (the SDP810s are undelivered), but it is what the pressure worker will have to
   satisfy when there is one, and it records that the five SDP810s will share the I2C bus whose
   first-transfer refusal is documented above.
1c. `one-wire-probes.md` — the 1-Wire subsystem's traps and standing requirements: the ROM-ID
   bindings, the `28-*` family filter, `therm_bulk_read`, the ~100 s tail a pulled probe leaves,
   the fake-sysfs harness, and the enrollment and offset requirements. **Read it before touching
   `oneWireProbes.cxx`.** Split out of `CLAUDE.md` on 2026-09-15.
2. `Hardware/logger-perfboard-wiring.md` — pinouts, I2C addresses, mux channel numbering and
   bring-up order. Its §3a net list is the authority on every connection. It is subordinate to the
   plan's Step 0b, which it lived alongside until 2026-09-10.
3. `SystemSetup/pi-headless-setup.md` — the host runbook.
4. `RaceChrono/README.md` — the phone's channel definitions as exported, for **both** boxes, and
   the traps in editing one by hand. Read it before touching any channel equation; the byte-level
   specification below is the requirement, that file is what is actually entered.

## Hardware it drives

| Part | Interface | Channels |
|---|---|---|
| 5 × Sensirion SDP810 (4 × ±500 Pa, 1 × ±125 Pa) | I2C `0x25` behind a PCA9548A mux at `0x70` | `P0`–`P5` |
| 4 × DS18B20 | 1-Wire on GPIO4, addressed by 64-bit ROM ID | `temp0`–`temp3` |
| BME280 | I2C `0x77` on the main bus (amended from `0x76`, 2026-09-09) | enclosure pressure, cavity temperature, humidity |

**Channel names are positional and carry no meaning.** `P0`–`P5` are fixed by mux position;
`temp0`–`temp3` are fixed by ROM ID at enrollment. The mapping from these to measurement roles
(`T_ambient`, `T_core_in`, `U`, `X`, `C` …) is a per-session record and is logged at boot. Do not
rename a channel after a role. **Pressure is still deliberately undecided. Thermal is decided
and applied** — installing the probes on the car pinned it, and enrolled in installed order it
is temp0=`T_ambient`, temp1=`T_core_in`, temp2=`T_core_out`, temp3=`T_aft`. **All four are bound**
(2026-09-10); `Hardware/logger-perfboard-wiring.md` §5a has the ROM IDs. **The role map is
independently confirmed** — warming each probe in installed order moved `temp0`–`temp3` in that
order, +3.8 to +5.4 K each.

All five SDP810s answer at the same fixed I2C address and cannot be strapped apart, which is why
the mux is mandatory rather than a convenience.

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
> ran at ~1 Hz. All five packets are defined as of 2026-09-11.
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
23 slots, and re-picking slots is how one of them ended up decoding signed data as unsigned.**

Slots as of **2026-09-11**, and **verified against RaceChrono's own exported profile**, which is
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

Three things this table is carrying rather than repeating:

1. **`Temperature Front 15` is a Temperature slot on purpose.** It was a Digital one until
   2026-09-11. It is the only `0x604` field with decimals; the rest are integers, and **SoC core
   millivolts stays Digital because RaceChrono has no voltage slot** — no precision is lost, the
   gauge simply carries no unit.
2. **Changing a slot's type changes its identity.** `Digital Front 15` and `Temperature Front 15`
   are different channels with different ids, so a retyped field leaves the old slot behind unless
   it is deleted — still subscribed, still decoding the same bytes under the old name.
3. **`0x601` was renumbered from 50–53 to 51–55 on 2026-09-11**, when byte 6–7 was added. **Any
   recording made before that carries the old slot numbers**, so a session and this table can
   disagree without either being wrong. `Tools/rcz-channels.py` prints what a given recording
   actually used — **and a session that was paused and resumed carries one fragment per stretch**,
   which the tool reports separately. Reading only the archive root silently discards every later
   stretch; that is half of the 2026-09-13 track day.

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
   and is speed-correlated so it does not average out of a speed sweep. **That is the only cavity
   QUALIFIED measurement of it there is**, and a second cavity record from the third drive
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
2. **Temperature is the cavity thermometer** (plan item 1c), with Pi SoC temperature on `0x604`
   as a cross-check rather than the primary proxy. **It is not the inlet density term** — that is
   `T_ambient`'s DS18B20 on `0x602` byte 0–1.
3. **Humidity is a seal and desiccant diagnostic** for the condensation risks in plan items 1a
   and 1d. It has no measurement consumer.

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
§[Calibration offsets](#calibration-offsets). The session file records the raw reading beside the
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
| 1 | sticky throttle bits, latched since **boot** not since session start — **observed at 5 on the 2026-09-13 track day**, `undervoltage` + `throttled` | `bytesToUint(raw, 1, 1)` |
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
the SD session file only. That is exactly the case the 2026-09-13 track day produced
(`../ndLouvers/` open item 48).

**Byte 7 is the channel to watch while the link is live.** Every other field here is a physical
quantity allowed to sit still — SoC core voltage reads a constant 840 mV on an
idle box for hours — so a frozen value proves nothing about the link. A counter freezing means the
link died; a counter skipping means a notification was dropped, which is exactly what commissioning
item 5a asked to be logged. **It is not the only counter with that property**: `0x601` and `0x603`
bytes 4–5 advance every cycle regardless of what their sensors report, and the second road test's
zero-drop measurement was made off those two.

## Layout

Single translation unit: every `.cxx` is `#include`-d into `main.cxx`, in the order below, and only
`main.cxx` is named in `CMakeLists.txt`.

```
main.cxx              argument parsing, worker startup, thread joins
dataContracts.hpp     includes, constants, the config and state structs
appData.cxx           the two globals
helpers.cxx           TAI and boot clocks, sysfs and subprocess readers
config.cxx            the .ini, resolved from /proc/self/exe
sessionWriter.cxx     append-only NDJSON, record queue, ~1 s fsync cadence
blePackets.cxx        RaceChrono packet wire format — before every producer
supplyMonitor.cxx     vcgencmd + rpi_volt hwmon at 1 Hz, sticky-bit transitions
oneWireProbes.cxx     DS18B20 enrollment, the bindings in the .ini, 0x602 and 0x603
i2cBus.cxx            I2C_RDWR transport with the mandatory retry — the mux will share it
bme280Sensor.cxx      BME280 forced-mode reads, compensation, 0x600 and 0x601
raceChronoBle.cxx     bluez_inc: adapter, advertisement, GATT, notify timers
bluez_inc/            submodule, github.com/weliem/bluez_inc

build/KnurLogger.ini  config TEMPLATE; the deployed copy is ~/bin/KnurLogger.ini on the box

one-wire-probes.md    the 1-Wire subsystem's traps and standing requirements — read before
                      touching oneWireProbes.cxx

Hardware/             box-2 hardware; nothing here is logger code
  logger-perfboard-wiring.md  the perfboard build sheet — §3a's net list is the authority
                              on every connection. Subordinate to the plan's Step 0b.

Tools/                offline diagnostics; nothing here runs on the box
  rcz-channels.py       decode a RaceChrono .rcz's channel slots and flag a mistyped
                        equation — the phone's channel list, audited without the phone

RaceChrono/           the phone's configuration, which lives nowhere else
  vehicleProfile.json   RaceChrono's exported vehicle profile, localUuid stripped;
                        both boxes' channels, since the two share one channel set
  README.md             why it is here, how to re-import it, and its traps

SystemSetup/          host configuration; nothing here is logger code
  pi-headless-setup.md    the runbook — start here
  audit-boot.sh           read-only survey of what the box runs at boot
  install-dependencies.sh packages the build needs
  harden-headless.sh      boot-time service reduction and bus configuration
  ssh-harden.sh           key-only SSH
  deploy-logger.sh        installs the production binary into ~/bin, never its .ini
  grant-w1-bulk-read.sh   udev rule for therm_bulk_read — one of the two conditions for 1 Hz
  60-knurlogger-w1-bulk-read.rules  what that script installs
  KnurLogger.service      systemd unit — INSTALLED and enabled, survives a reboot
```

## Relationship to the other two loggers

| | KnurDash | iSitePiLogger | KnurLogger |
|---|---|---|---|
| Display | 4.3" DSI touchscreen, GTK under `startx` | none | none |
| Primary record | RaceChrono on the phone | files uploaded over HTTP | **RaceChrono over BLE**; SD card is the durable raw/diagnostic record |
| Radios | BLE | both disabled | **BLE required**, Wi-Fi gated per session |
| Language | C | C++, single translation unit | C++, single translation unit |

**KnurDash** contributes the RaceChrono BLE protocol implementation and its `bluez_inc` usage.
It is not the structural model: it is a GUI dash that boots into `startx`, and this box has no
display.

**iSitePiLogger** is the structural model — `SystemSetup/` layout, pre-flight-by-default scripts,
single-translation-unit CMake build, procedural GLib workers, an `.ini` beside the binary,
`CLOCK_TAI` throughout. Two things in it must **not** be copied, and both are load-bearing:

1. Its `setupNotes.txt` disables Bluetooth. BLE is this box's product.
2. Its `setupNotes.txt` disables Wi-Fi. This box lives in a wheel-well cavity with no Ethernet,
   so Wi-Fi is the only way back in; it is gated per session instead.

## Repository, deployment and where the code is built

Upstream is **https://github.com/chrumck/KnurLogger** (`origin`), and it is **public** — see the
note at the end of this section before adding anything host-specific.

The box has the full toolchain (`git 2.47.3`, `cmake 3.31.6`, `gcc`/`g++`, `libglib2.0-dev`), so
**the code is built on the Pi**, not cross-compiled. First time:

```bash
ssh KnurLogger 'git clone https://github.com/chrumck/KnurLogger.git'
```

Thereafter `git -C ~/KnurLogger pull` on the box. The `build/` directory is gitignored except for
its tracked `KnurLogger.ini`, which is the **template**, not the deployed file.

### The dev copy and the production copy are two different files

The logger is **built** in `~/KnurLogger/build/` and **run** from `~/bin/`, and the two are kept
apart deliberately (owner decision, 2026-09-10):

| | Path | Owned by | Carries |
|---|---|---|---|
| dev | `~/KnurLogger/build/KnurLogger.ini` | git, overwritten by every sync | the seed, and the **version-controlled backup** of whatever was last copied back |
| production | `~/bin/KnurLogger.ini` | the box and `--enroll` | the live ROM ID bindings and offsets |

The dev copy is not permanently empty: it carries the four ROM IDs enrolled on 2026-09-10, copied
back after the trip. That is what makes it a backup rather than only a template — a fresh box
seeded from it starts with the current bindings, which is right, since the probes it will read are
the same four.

**The reason is that the configuration and the source now share a file.** `KnurLogger.ini` carries
the DS18B20 bindings as well as the calibration offsets; the bindings can only be made at the car;
and the `tar`-over-ssh loop below overwrites everything under `~/KnurLogger`. With one copy, one
sync from the workstation silently discards a trip to the car — which is exactly the trap this
README used to document and ask you to remember your way around.

```bash
ssh KnurLogger 'bash ~/KnurLogger/SystemSetup/deploy-logger.sh --execute'
```

`deploy-logger.sh` **always replaces the binary and only ever creates the `.ini`, never updates
it.** Run it with no arguments first, like every script in `SystemSetup/`. `KnurLogger.service`
points at `/home/chrum/bin/KnurLogger`, not at the build tree.

> **⚠ The production `.ini` is not in git, so nothing else backs up a calibration or a binding.**
> After enrolling or entering offsets at the car, copy `~/bin/KnurLogger.ini` into the repo as
> `build/KnurLogger.ini` and commit it. That is a deliberate act rather than a side effect, which
> is the point — but it is also the only thing standing between a wiped SD card and another trip:
>
> ```bash
> scp KnurLogger:bin/KnurLogger.ini build/KnurLogger.ini
> ```

**The box is fed from constant 12 V, not the accessory circuit** (as built, 2026-09-10), so the
logger runs the whole day and the fuse is the off switch. Two consequences worth having here:
`SystemSetup/KnurLogger.service` is **required** rather than convenient, because nothing
hand-starts the logger when the fuse goes in; and the hard cut the ~1 s `fsync` protects against
is now the fuse being pulled, or a cranking dip, rather than ignition-off. `CLAUDE.md` has the
rest, including the battery-drain arithmetic.

**Do not put push credentials on the box.** Commit and push from the workstation; the box pulls
only. A logger in a wheel-well cavity is physically exposed — if the car is broken into or the SD
card is pulled, any PAT or writable deploy key on it becomes an attacker's write access to this
repo. A public repo needs no credential to clone or pull, so read-only costs nothing.

For a tight edit-build loop, round-tripping through GitHub is slow; copy the tree to the box
instead and keep GitHub for durable commits. The loop in use is:

```bash
tar czf - --exclude=.git --exclude=build/CMakeCache.txt --exclude=build/CMakeFiles . | ssh KnurLogger 'tar xzf - -C ~/KnurLogger && cd ~/KnurLogger && cmake --build build -j4'
```

**That sync overwrites `~/KnurLogger/build/KnurLogger.ini`, and that is now harmless** — it is the
template, and the running logger does not read it. Nothing under `~/bin/` is touched until
`deploy-logger.sh` is run, and even then only the binary. This is what the dev/production split
above bought; before it, one sync destroyed a hand-entered offset silently.

**The `../ndLouvers/...` links throughout this repository are broken on github.com and that is
deliberate.** `ndLouvers` is a separate repository that happens to sit alongside this one on disk.
The links resolve locally, which is where the work happens. Do not "fix" them by copying
requirements across — a duplicated requirement is one that will drift, and `CLAUDE.md` says so.

## Getting on the box

SSH alias `KnurLogger` (`192.168.118.52`, user `chrum`), key-only.

```bash
ssh KnurLogger
```

## Enrolling the four DS18B20s

`temp0`–`temp3` mean nothing until a ROM ID is bound to each. Binding is a deliberate mode and
never happens during a logging run, so a probe that drops out and comes back mid-session cannot
re-label a channel.

**Stop the service first — it is installed and enabled, so the logger is always already running:**

```bash
ssh -t KnurLogger 'sudo systemctl stop KnurLogger'
```

Two instances both poll the bus and both advertise, and nothing warns. See `CLAUDE.md`.

```bash
ssh KnurLogger '~/bin/KnurLogger --enroll'
```

1. Start with **no probe connected**, then plug them in **one at a time, lowest channel first** —
   the installed order the plan fixes is `temp0` = `T_ambient`, `temp1` = `T_core_in`,
   `temp2` = `T_core_out`, `temp3` = `T_aft`.
2. **Every probe stays plugged in once it is in. Do not unplug one to make room for the next.**
   Binding works either way, which is what makes this easy to get wrong — it was got wrong at the
   car on 2026-09-10 — but unplugging as you go costs three things that binding does not:
   1. **The four-probe star is never loaded**, so the run proves nothing the ESP32 bench rig had
      not already proved with single probes. All four together on the 4 × 5 m bus, enumerating
      with CRC-clean reads, is the open acceptance criterion in plan thermal item 1, and it is
      only met with all four connected at once.
   2. **Step 5's map check becomes impossible**, because warming one probe and watching one
      channel move needs four live channels.
   3. **Every unplug leaves a ~100 s tail** of a channel that is present in sysfs and answering
      with nothing — see `CLAUDE.md` on `w1_slave_ttl`.
3. **Allow ~10 s per probe.** `w1_master_timeout` is 10 s, so that is the hot-plug latency; nothing
   can shorten it without root.
4. Each bind prints `OneWire: BOUND temp0 <- 28-…, reading 21.50 C` and writes an `enrollment`
   record. Check the ROM ID against the lead you just connected. **A bind whose reading comes back
   `INVALID` is a warning, not a failure** — the binding is still written, but the probe answered
   badly on the very read that is meant to confirm it, so re-seat that lead and watch the channel
   before trusting it.
5. **Two unbound probes in one scan are refused, not guessed.** sysfs order is not arrival order,
   so there is no fact available that says which came first. Unplug one and re-seat it alone.
6. **Enrollment keeps sampling after the fourth bind.** With all four bound, warm one probe by hand
   and watch one channel move on the phone — four warmings confirm the whole map in situ and double
   as a liveness test. Stop it with `pkill -TERM -x KnurLogger`.
7. `--enroll --reset` discards every binding and starts from `temp0`. The bindings it throws away
   are written into the session file first, because they are otherwise unrecoverable.

The bindings are written into the `[thermal]` section of the **`KnurLogger.ini` beside the
binary**, alongside the calibration offsets, as `temp<N>RomId` / `temp<N>BoundTaiUs` /
`temp<N>BoundIso` (owner decision, 2026-09-10 — this replaced a separate `channels.ini` in the data
directory; see `CLAUDE.history.md` §2.7 and do not rebuild that file). An empty `temp<N>RomId` is
an unbound channel and a perfectly good state. Three consequences:

1. **Enrollment rewrites a file you hand-maintain**, so it does it line by line: only the twelve
   binding lines are touched and every other byte, comments included, is copied through. It is not
   written through GLib's key-file serialiser, which destroys non-ASCII characters in comments —
   measured 2026-09-10, this file's em-dashes came back as `?`.
2. **The offsets are now in front of you when you change a binding**, which is the point: an offset
   is keyed to the slot, so re-enrolling in a different order strands it. See
   §[Calibration offsets](#calibration-offsets).
3. **This is the file `SystemSetup/deploy-logger.sh` refuses to overwrite.** The production copy is
   `~/bin/KnurLogger.ini` and the git-tracked template is `build/KnurLogger.ini`; see
   §[Repository, deployment and where the code is built](#repository-deployment-and-where-the-code-is-built).

**Editing a ROM ID by hand is allowed and guarded.** Anything that is not a 15-character `28-…` is
refused and logged as an event, and one ROM ID appearing on two channels is refused on the second
— a duplicate would otherwise produce two channels tracking each other perfectly, which is a
mislabelling that looks like agreement.

`Hardware/logger-perfboard-wiring.md` §5a is the human record of the resulting table, and it is
**filled** as of 2026-09-10. **Channel → role is a per-session record, never a channel name** — it
is in every session file's `thermalBaseline`, which is where an analysis should take it from.

## Calibration offsets

Hand-edited in `KnurLogger.ini`, one per channel, applied to the value sent to RaceChrono:

```ini
[thermal]
temp0OffsetC=0.0
temp1OffsetC=-0.375
temp2OffsetC=0.0
temp3OffsetC=0.0
```

**How to get the numbers.** Park the car long enough to reach equilibrium — engine cold, no sun, no
residual heat — and run one stationary logging session. At equilibrium all four probes sit in the
same air, so their differences are sensor error and nothing else: read the four values out of the
session file's 1 Hz `temp` records and enter each probe's deviation from the four-probe mean. Only
*relative* offsets mean anything, because there is no reference thermometer on the car and
ΔT_preheat depends on the probes' differences rather than their absolute accuracy. **Entering
nothing is a legitimate outcome** if the differences are small against a ~6 K signal.

Five things to know:

1. **They are keyed to the channel slot, not to the probe.** An offset is really a property of one
   particular DS18B20, so if the probes are ever re-enrolled in a different order, or one is
   swapped, the offsets stay with the slots and no longer describe the parts in them — **re-check
   them after any re-enrollment.** In exchange they live in the config beside every other setting,
   including the bindings that say which probe each slot holds.
2. **The session file records all three values** — `centiC` (raw), `offsetC` (in force) and
   `sentCentiC` (what went on the air) — so a mistyped offset costs a reprocess, not the session.
3. **All four keys must be present.** A missing one is a startup failure, not a silent zero: an
   offset that quietly stopped being applied would be invisible in the data it corrupts.
4. **Past ±5 °C it warns** about a likely decimal-point slip and applies the value anyway; **past
   ±50 °C it refuses to start.** The offsets in force are printed at startup and recorded in the
   `thermalBaseline` record.
5. **The ROM ID → channel bindings are in this same section**, as `temp<N>RomId`, machine-written
   by `--enroll` (2026-09-10). That is deliberate rather than incidental: item 1's consequence —
   an offset keyed to a slot stops describing the part in it after a re-enrollment — is only
   noticeable if the two are read together. A changed `temp2RomId` is the signal to re-check
   `temp2OffsetC` three lines above it.

**The logger takes no automatic session-start sample and makes no judgement about whether the car
was settled** — see `CLAUDE.history.md` §3.1 for why that was tried, measured failing, and
dropped.

## Testing the 1-Wire path without probes

The probes are on the car and the phantom `00-*` devices stopped once `R11` terminated the line, so
neither a real reading nor the family filter can be exercised on this box. The whole path is
nonetheless testable, because the code reads sysfs and sysfs can be replaced:

```bash
unshare -Urm --map-root-user /bin/bash -c 'mount --bind /tmp/fake-w1 /sys/bus/w1/devices && ...'
```

An unprivileged user namespace gives a private mount namespace, so a fake tree can be bind-mounted
over `/sys/bus/w1/devices` with **no root and no risk to the real box**. Populate it with a
`w1_bus_master1/` directory, `28-…/w1_slave` files carrying the kernel's two-line format, and a
`00-…` entry to prove the filter drops it. This is how enrollment, the ambiguous-step refusal, the
store's own family and duplicate guards, bound-but-absent, CRC failure, the 85.00 °C power-on
default, out-of-range rejection and the application of a hand-entered offset were all verified.

**Its one limitation, found the hard way:** a plain file cannot represent a sysfs attribute whose
read value differs from what was written, and `therm_bulk_read` is exactly that — you write
`trigger` and read back `-1`, `0` or `1`. In the fake tree it reads back `trigger`, so that state
machine is unexercised. Two things fill the gap and are worth reaching for before concluding
something is untestable:

1. **A FIFO in place of `w1_slave`** makes a read block for a controlled time, which is how the
   `conversionMs` fallback was verified — a read forced to 385 ms produced `conversionMs 385`
   where the old code produced 0.
2. **`w1_master_add`** attaches a real family-0x28 slave with no hardware present, so `w1_therm`
   binds and its master attributes appear. This is how the udev rule was verified. It needs root,
   `w1_master_remove` undoes it, and the id is parsed as `%02x-%012llx` — the hyphen matters.

## Session files are the only copy until you take one

`filesDir` on the box is the sole home of every session until it is copied off. The workstation
backup lives at `C:\_claude\KnurLoggerData\sessions\` — **deliberately outside both git
repositories, because this one is public and session files are data.**

```bash
scp -r KnurLogger:KnurLoggerData/sessions/*.ndjson /c/_claude/KnurLoggerData/sessions/
```

**Three things to know before reading one.** A session filename is stamped from the box's wall
clock, which in the car is hours wrong — see `CLAUDE.md`, "A session filename means nothing". And
`taiUs` can jump mid-file when `timesyncd` corrects the clock; `bootUs` and `sessionUs` are the
axes that survive it. And **copy a session only once the logger has stopped writing it** — a
mid-write copy is a truncated file that parses perfectly and says the session ended early. One
lived in the backup that way until it was replaced; `systemctl is-active KnurLogger` is the check.

## Next

**Needing a drive, not code.** Commissioning item 5.7's under-load supply telemetry still wants
duration, a hot ambient, and the five SDP810s actually drawing. Items 1c and 1d — ventilated versus
sealed, and the thermal envelope — want a **hot day**; the sealed configuration has cool-evening
data only. **Record where the box was mounted on every session.** Without it a run cannot serve
either, because a cabin record reads exactly like a cavity one.

**Needing a part.** The SDP810 readers and the mux, once the five sensors arrive. Expect the retry
in `i2cBus.cxx` to matter for them, and expect a mux channel switch plus a sensor read to be two
transfers that must not be interleaved with anything else on the bus.

**Not outstanding, and not to be reopened.** The installed BLE link (commissioning item 5a,
closed 2026-09-15 on the third drive), the sample rate (plan open item 43), the thermal cold-soak
calibration and the channel → role map (open items 41 and 42), the subscription-filter fault, and
all three phone-side channel definitions (open item 45). All four `temp<N>OffsetC`
staying 0.0 **is the result** — do not "fix" it.

**What is testable on the box today:** the build, config loading, the session writer and its fsync
cadence, the supply-telemetry worker, a BLE advertiser against a phone, the BME280 at `0x77`, the
mux at `0x70`, and — via the fake-sysfs harness above — every branch of the 1-Wire worker except a
real reading, real bus timing and `therm_bulk_read`. **What is not:** anything requiring a real
SDP810, which is undelivered, or a real DS18B20, the four being installed on the car.

**Where the rest lives.** `SystemSetup/pi-headless-setup.md` §Work Progress is the authority on
host state. `CLAUDE.md` carries the four architecture requirements the plan imposes — BLE as the
primary data path with the SD card as the durable raw/diagnostic record, an append-only file with a
~1 s `fsync` cadence, DS18B20 channel enrollment, and supply-health telemetry — plus every hardware
trap. Read it before writing code. `CLAUDE.history.md` is its audit trail: resolved faults with the
diagnostics that found them, reversed decisions, and the retirement notes that exist to stop the
next agent rebuilding what was deliberately removed.

## Licence

MIT — see [LICENSE](LICENSE), which also states what it does not cover and the absence of any
warranty of fitness for vehicle use. **`bluez_inc` is a submodule, not a copy**, so its own MIT
licence travels with it rather than being restated here. The sibling
[ndLouvers](https://github.com/chrumck/ndLouvers) repository carries the same licence.
