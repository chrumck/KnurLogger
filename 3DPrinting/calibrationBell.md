# Liquid-sealed pressure bell — printed reference source

**Status: ADOPTED, BUILT, AND PROVEN TO DELIVER** (owner decision 2026-09-20; first session the
same day). Step 0b's pressure qualification will use this bell rather than the water manometer,
which is now the documented fallback (`../../ndLouvers/pressure-testing.md` §2.2 carries the
decision and the reasoning).

**A bell exists, it is weighed at 79 g, and it ran 53 minutes on `P0` at 26–32 Pa**
(`../../ndLouvers/pressure-testing.history.md` §3.3). The force balance holds: the measured `k_d` is
**0.2367 Pa/mm** (2026-09-23), from geometry on an `A_eff` measured with water — see §"As built".
**The earlier 0.2372, and the 0.891 mm wall inverted from it, are WITHDRAWN**: that slope was read
*through a sensor*, so it carried the sensor's span and the barometric term, and inverting a wall
from it made the pair circular.
**An earlier figure of 0.2615 ± 0.013, from eyeballed depths on an unballasted bell, is
withdrawn.**
**It still produces no calibration and closes no open item**: one channel, one source, no ladder,
no repeats, no independent reference. What it produced was the fixture's own constants and four
error terms this file did not carry.
**A ladder on 2026-09-22/23 settled the fixture and overturned its constants.** `A_eff` was measured
with water (above), so **`k_m` moved 0.5551 → 0.5639 and `k_d` 0.2372 → 0.2367**, and with the
SDP810's barometric dependency applied (`../../ndLouvers/pressure-testing.md` §2.3a) **all five
sensors read within ±1.7 % of this bell** — spread 2.2 pp, every part inside its ±3 % spec.
**The earlier "three sensors spread ten percent and the bell sits inside them to ~1–2 %" is
retired**: `P2`'s −3.5 % was the only figure below the bell and it is now −1.53 %, alongside the
rest. There was no straddle — there was a 1.6 % bore error and a 4.2 % barometric term.
The "residual" §"As built" item 4 carried was those two, not a bell term.
The add-mass test below cannot be performed on this build and is retired.

> **⚠ THE BELL THAT EXISTS IS NOT THE BELL DRAWN BELOW.** It was printed from
> `calibrationBell.stl`: plain skirt, **six radial lid ribs**, small central boss, **no collar, no
> graduations, no rod and no pan** — with an **ID of 150.00 and a wall of 0.90** against rev F's
> ID 148.4 / wall 0.80. (**"No ribs" was wrong and is corrected 2026-09-23** — a ray-cast of the
> mesh finds six ribs on the lid's inner face, ~3 mm tall and tapering, reaching r ≈ 73.) **Every nominal constant in this file is therefore
> wrong for it**, `k_d` by 23 %. Use §"As built" below, and read the rev F constants as what a rev F
> bell would give.

> **⚠ LIVING IN THIS REPOSITORY CHANGES NOTHING ABOUT WHAT IT MAY DECIDE.** `CLAUDE.md` here is
> explicit that this repository owns software, host configuration and box-2 hardware, and **owns
> no measurement decision** — `../../ndLouvers/CFD-Learning-Plan.md` Step 0b is the authority on
> calibration and acceptance, exactly as it is for the perfboard build sheet. This file owns the
> fixture: its geometry, its constants and how to use it. It does not own whether the fixture is
> adopted, what accuracy is required of it, or what counts as a qualified reference.
>
> Section numbers like §2.2 and §2.9 below refer to `../../ndLouvers/pressure-testing.md`; the bell
> sessions §3.3–§3.14 cited below are in `../../ndLouvers/pressure-testing.history.md` (rev 114),
> and the current tubing, span and bell figures are in `pressure-testing.md` §2.2a and §3.3.
> Cross-repo links resolve on disk and 404 on the public git host; that is accepted here, as
> `CLAUDE.md` says.

[`calibrationBell.svg`](calibrationBell.svg) is the drawing, at **rev F**, and
[`calibrationBellWeighing.svg`](calibrationBellWeighing.svg) is the rig for the one calibration
weighing it needs. This file is what the drawings cannot carry: why the thing is shaped this
way, and what it can and cannot do.

Printed parts live beside them: `calibrationBell.stl`, `calibrationBellPan.stl`,
`calibrationBell.3mf` and `pipeStand.stl`.

## As built — the bell that exists, 2026-09-20

**These supersede every nominal figure in this file and on the drawing, for this part.** Geometry
read directly from `calibrationBell.stl`; wall and ID confirmed on the printed part with calipers;
`k_d` confirmed independently from the pressure record.

| | STL | printed, measured | rev F nominal |
|---|---|---|---|
| OD | 151.80 | 152 | 150.0 |
| **ID** | **150.00** | **150.0 ± 0.2** | 148.4 |
| **wall** | 0.90 | **1.00** | 0.80 |
| skirt, lid to rim | 113.5 | — | 115 |
| CG above the rim | **71.65** | — | — |
| features | plain skirt, 0.803 lid, **six lid ribs (2.2 cm³)**, Ø10 × 7 boss (0.5 cm³) | as STL | collar, 6 ribs, Ø16 × 25 tapped hub |
| graduations | **none** | **none** | every 5, zero at the rim |
| mass | 80.7 g at 1.24 | **79 g** weighed, **78 g** slicer | 88 g rod-less |

| constant | **as built** | superseded | rev F rod-less nominal |
|---|---|---|---|
| `A_eff` | **17 392 mm² — MEASURED WITH WATER** | 17 671 (from ID 150.00) | 17 296 mm² |
| `A_wall` | **421 mm²** (perimeter 467.5 × 0.90) | 474 | 375 mm² |
| **`k_m`** | **0.5639 Pa/g** | 0.5551 / 0.5549 | 0.5670 Pa/g |
| **`k_d`** | **0.2367 Pa/mm** | 0.2372 measured, 0.2337 rod-less | 0.2122 Pa/mm |
| `ΔP/Δm` at constant volume | **0.5405 Pa/g** (`g/A_o`) | — | — |

> **⚠ `A_eff` IS MEASURED, NOT COMPUTED FROM THE ID — AND IT IS 1.6 % BELOW π/4·150²**
> (2026-09-22). A **datum-free two-fill difference**, 1215 g of water between the 30 mm and 100 mm
> marks, gives **17 392 mm²**; a 50 mm single fill corroborates it to **0.08 %**. The equivalent
> circular bore is 148.8 mm, i.e. about **150.0 × 147.6 as an oval** — which is what a 0.89 mm-walled
> printed cylinder does, and a caliper reports whichever axis it is handed. **Measure the ID on two
> axes before trusting a single number.**
>
> **Fill with the M3 rod fitted — that is correct, not a tolerated error.** `A_eff` in the force
> balance is the bore *minus* the rod, and the water occupies exactly that, so the two match with no
> correction. The six lid ribs and the central boss total **2.73 cm³** and sit below the 30 mm mark,
> so the two-fill difference cancels them; they matter only to a single fill referenced to the lid.
>
> **`k_d` = 0.2372 is WITHDRAWN as a measurement.** It was the slope of *reading* against depth, so
> it carried the reading sensor's span and the barometric term of `../../ndLouvers/pressure-testing.md`
> §2.3a. **And the 0.891 mm wall was inverted from it, which makes that pair circular** — do not
> re-derive either from the other. 0.2367 comes from geometry on the measured `A_eff` and the STL's
> 0.90 mm wall.

1. **The print over-extruded ~0.1 mm on the wall, outward**, OD 151.80 → 152. **But "`A_eff` and
   `k_m` are unaffected" was wrong** (corrected 2026-09-23): `A_eff` is **1.6 % below** π/4·150²
   because the bore is **oval**, not because of the wall. Use the water-measured 17 392 mm².
   **And the 0.891 mm wall inverted from the old slope is withdrawn** — that inversion assumed the
   sensor was exact and the barometer irrelevant, and it produced a wall that was then used to
   justify the slope it came from. Take **0.90 mm from the STL**; the caliper's 1.00 was read at
   the rim, the first layer of a part printed rim-down, which is squished wider. Measure a printed
   wall away from the sealing edge, and **measure the ID on two axes**.
2. **Three independent mass figures agree to 2.5 g** — 80.7 g from STL volume at 1.24, 78 g from
   the slicer, 79 g on the scale. The slicer's does not come from the scale, so **this bounds the
   scale's span error at about ±1 g = ±0.55 Pa** at the bare-bell working point.
3. **No graduations were printed**, so `d` is read by eye against a plain skirt. history §3.3c measured that
   as **±1 mm of scatter plus a ~1.7 mm systematic**, against the ±1 mm this file's budget assumes.
   **Print the graduations, or set `d` against the grounded position**, which is the only repeatable
   datum this bell currently has.
4. **A consistent −0.42 ± 0.19 Pa residual remains** after all of the above. It is exactly
   equivalent to reading `d` 1.6 mm shallow, to 0.75 g of mass, or to a skirt meniscus force at a
   contact angle near 97°, and **those three cannot be separated by any bare-bell run.** The
   add-mass test in §"Use it differentially" is what separates them.
   **Seen a third time on 2026-09-21, at +3.3 Pa = 6 g on a 347 g bell, with the masses, the
   scale (500 g of reference weights, within 1 g), the ID and the rig — standpipe, nothing touching
   — all confirmed** (`../../ndLouvers/pressure-testing.history.md` §3.9, plan open item 57). The
   sightings are **−0.4 Pa bare, +0.6 Pa light, +3.3 Pa at 347 g and +14 Pa at 726 g (history §3.10)**:
   it grows with the pressure, so **it is not a zero error and this item has been mis-filed since it
   was written** — the shape is a span, and the two spans in the chain are this bell's `k_m` and the
   SDP810's own ±3 % of reading, which no derivation has budgeted. **The reference weights now
   exist but the add-mass test cannot be performed on this build** (below); what told the reference
   from the instrument was a second and third sensor on the bell. **Done the same night** (history §3.11): the two ±500 Pa parts disagree by 2.5 % and both
   read above the model. **Then the length-change pair (history §3.12) measured the extension and the record
   re-solved with NO bell residual** — the sensors read high, `P0` by ~2–4 % and `P1` by ~6 %, and
   the residual this item has carried since it was written was their span at every load it was
   seen. What the bell still owes is a ≤ 1 % common span (`A_eff`, displacement) that two identical
   sensors cannot exclude; `P2`'s independent scale factor at the light load is the check — **and
   it came in 3–4 % LOW (history §3.14)** — **and that reading is now retired**. The ladder of
   2026-09-22/23 put `P2` at **−1.53 %** alongside every other part: there was never a straddle,
   there was a **1.6 % bore error** (fixed by measuring `A_eff` with water) and a **4.2 % barometric
   term** (`../../ndLouvers/pressure-testing.md` §2.3a). All five parts are inside ±3 %.
   **The fixture did do what it was built for** — but only once its own area was measured rather
   than assumed.
   **THE ADD-MASS TEST CANNOT BE PERFORMED ON THIS BUILD** (owner, 2026-09-21): the pan is not
   reachable with the bell afloat, and changing weights means lifting the bell out, which resets
   the trapped volume the derivation in §"Use it differentially" depends on. **Retired as
   impracticable, not as unwanted.** The substitute is two settled placements at different loads
   compared at matched `d`, which cancels tare, datum and meniscus equally but returns
   `(1 + ε)(1 − f̄)` rather than `(1 + ε)`, because the line loss changes with load. A rebuild that
   wanted the check back needs a load path reachable from outside the skirt — a hook below the rim,
   or a pan that clears the tub floor by a hand's width. Plan open item 57.

> **⚠ REVS A–E ARE SUPERSEDED. DO NOT BUILD ANY OF THEM.**
> **Rev A** floated the bell free with the masses on a tray on top. It capsizes — confirmed on the
> bench 2026-09-20 and explained below. **Rev B** fixed that with a central guide rod and linear
> bearings; it works, but it puts friction into the one place that must stay free.
> **Rev C hung the load underneath instead**, which removes the instability and the friction at
> the same time, but at Ø180 it needs 1.35 kg of washers. **Rev D is rev C at Ø150** — 921 g,
> a 4 L tub instead of 6.5 L, and a 43 Pa floor instead of 37 Pa. **Rev E swaps the Ø6 aluminium
> rod for M3 stainless threaded**, which is lighter, drops a galvanic pair, and — because a
> tapped hub is removable — turns the rod-less point into a grub screw instead of a tape patch.
> **Rev F puts the PLA pan back** in place of rev E's nut and backing washer (owner preference);
> it costs 1.3 Pa of floor and adds a second material to the apparent-mass arithmetic.
> The retired revisions are described here only so nobody rebuilds one from an old screenshot.

## What problem it solves

An SDP810 measures Δp by passing gas through an internal bypass, so **it is itself a flow path
between its two ports** and it drains any static reference. A 50 cm head on a 6 mm U-tube collapsed
in under 2 s (2026-09-19, `P0`). Ballast does not fix it: 5 L against the estimated bleed resistance
gives a 23 s time constant, and §2.5 asks for a 20 s plateau.

A bell does not fight the bleed. It is a **constant-pressure source**: as gas escapes the bell
sinks, and the pressure — weight over area — does not change. The supply only has to replace what
the sensor drinks, of order 1 mL/s at 500 Pa.
**That estimate is superseded: the measured figure is ~5.5 mL/s at 500 Pa**
(`../../ndLouvers/pressure-testing.history.md` §3.4a, 2026-09-20). The circuit resistance was measured at
**flow-dependent**: 2.29 × 10⁸ Pa·s/m³ at 0.82 mL/s but only 1.0 × 10⁸ at 0.28 (history §3.5–§3.6), so
**the supply demand does not scale linearly with pressure**. Both are below the U-tube figure the
1 mL/s came from — that observation was taken
at ten times full scale, where the flow in the sensor's internal channel is not laminar. **At 500 Pa
the bell therefore sinks ~18 mm/min and lasts about four minutes per charge**, and the supply has to
keep up with 5.5 mL/s.

Its accuracy comes from **mass and diameter**, both measurable at home to better than 0.2 %, rather
than from a liquid column read against a ruler. That is the other half of the U-tube's problem:
125 Pa is 12.7 mm of water, and contact-angle hysteresis alone puts ±1 mm of uncertainty on a 6 mm
bore — ±10 Pa, before anyone picks up a ruler.

## Why the load has to hang below

**A constant-pressure source is neutrally stable in heave by construction** — pushing it down does
not raise its upthrust, which is the very property that makes it immune to the bleed. The same
property leaves it almost no stiffness in *tilt*. The only restoring moment a floating bell has is
the buoyancy of the ring of wall piercing the surface:

    K_ring = ρ·g·π·R³·t  =  0.0104 N·m/rad    (R = 75 mm, t = 0.8 mm)

It falls as R³, so a smaller bell has even less of it — but the hanging load dominates either way.

Against that, the weight acts through the CG while the gas force acts through the lid centroid, so a
CG a distance ℓ *above* the lid plane contributes −W·ℓ:

| configuration | tilt stiffness | |
|---|---|---|
| rev A, bare bell | +0.053 N·m/rad | stable |
| rev A at 125 Pa, masses on the tray | +0.001 N·m/rad | neutral — tips at a touch |
| rev A at 500 Pa, masses on the tray | **−0.139** N·m/rad | **capsizes** |
| rev F, bare bell | +0.041 N·m/rad | stable |
| rev F at 125 Pa | +0.27 N·m/rad | solid |
| rev F at 500 Pa | **+1.24** N·m/rad | very solid |

With 1308 g aboard, rev A's combined CG had to sit within **1.4 mm** of the lid plane. Nothing
achievable. **Rev F's stability rises with load instead of falling**, because the stabilising mass
and the applied mass are the same object.

> **⚠ THE +0.041 N·m/rad "BARE, STABLE" ROW IS CONTRADICTED BY THE BENCH** (2026-09-20,
> `../../ndLouvers/pressure-testing.history.md` §3.3e). A bare bell on this geometry **capsized twice at an
> immersion of 49–50 mm and settled at a 5–8° angle of loll at 59–64 mm.** A 5–8° loll on the
> wall-sided relation `tan θ = √(−2·GM/BM)` with `BM` ≈ 16 mm puts `GM` at **≈ −0.1 mm** — neutral,
> not stable — and the capsize at 49 mm requires **`GM` ≈ −6 mm**. The bare bell reaches neutral
> only at an immersion near **60 mm** and improves from there.
>
> **The likely cause of the error is the reference plane, and the correction is an owner decision,
> not an edit I have made.** The table above takes the righting couple about the **lid plane**
> (`+W·ℓ`, ℓ = the CG below the lid). A metacentric treatment takes it about the **centre of
> buoyancy**, which on this bell sits **7–14 mm below the water** rather than 45–67 mm above it at
> the lid — roughly 50 mm of righting arm the bell does not have. The paragraph immediately above
> this table already says the right thing in words: *"the same property leaves it almost no
> stiffness in tilt."* **The prose and the table disagree, and the bench agrees with the prose.**
> **CONFIRMED AGAIN ON A DIFFERENT BALLAST, AND THE CROSSOVER IS NOW LOCATED** (2026-09-21,
> history §3.8e). With 17.5 g of ballast the bell **lolled ~5° at d ≤ 57 mm and returned upright by
> d ≥ 64.5 mm**, unaided, as it sank — so the crossover is bracketed in **57–64.5 mm**, against the
> ≈ 60 mm this box predicted from the capsize and loll angles. **The immersion dependence the table
> omits is real, repeatable across ballasts, and now quantified.** Third bench contradiction of
> the "+0.041 N·m/rad, bare, stable" row.
> **A tilt costs more through the waterline than through `cos θ`.** At 5° the `cos θ` term is only
> +0.21 Pa on a 38 Pa point, but the waterline runs **±6.6 mm around a 150 mm skirt — 1.6 Pa** — so
> which azimuth `d` is read at dominates. The force balance wants the **axis** waterline, i.e. the
> mean of the high and low sides. **Record the read azimuth whenever the bell is not upright**, and
> treat any mark below d ≈ 60 mm in a light configuration as untrustworthy without it.
> Until the model is re-derived, treat the load rows as unverified too, and **do not run this bell
> bare** — §"What it can and cannot reach" carries the working consequence.

**Do not "improve" this with a fixed ballast.** Stabilising weight below the lid and the pressure
floor are the *same quantity*. A fixed ballast hung deep enough to give the full-load stiffness
would need ~333 g of apparent weight at 150 mm, which puts **~190 Pa on the floor** at Ø150 —
worse than the instrument's whole useful low range. Only the applied load can pay for its own
stability, which is exactly what hanging it does.

## The formula

    P = k_m · m_app  −  k_d · d

- `m_app` — **apparent** mass, in grams (below)
- `d` — rim immersion, read off the skirt graduations at the outer water line, in mm
- `k_m = g / A_eff` — nominal **0.5672 Pa/g**
- `k_d = ρ·g·(A_wall + A_rod) / A_eff` — nominal **0.2155 Pa/mm**

**Those two numbers are rev F NOMINAL and are not the bell that exists** — see §"As built", where
`k_m` is **0.5639** and `k_d` **0.2367** (§"As built"; the 0.5549–0.5551 and 0.2337/0.2372 pairs are
withdrawn). The formula itself is
unchanged; only the constants move.
Nominal `ID 148.4` with an M3 rod → `A_eff = 17 289 mm²`, `A_wall = 375 mm²`, water at 20 °C.
**A thread has two areas and they go in different places:** the **major** diameter (7.07 mm²) is
what the bore removes from the lid; the **pitch** diameter (5.62 mm²) is what displaces water at
the surface. Both are ~0.04 % of `A_eff`, so the distinction changes nothing — it is written down
only so the next reader does not have to decide which one to use. `k_d` is larger than rev A/B's because the rod now pierces the water surface
alongside the skirt.

**Derivation**, so nobody has to re-derive it: taking the force balance with gas on the lid
underside, liquid pressure on the rim annulus, buoyancy of the submerged hanging load, and weight
down, the effective area comes out as the **inner** cross-section less the rod bore, and the
correction is the buoyancy of whatever crosses the surface. The outer and mid-wall diameters both
appear in intermediate steps and neither is the answer.

### `m_app` is dry masses plus one measured ratio

Routine use is pure arithmetic:

    m_app = (bell + lid + rod, dry) + 0.8736 × (stainless, dry) + 0.195 × (PLA pan, dry)

**The rod is not in the buoyancy term.** Its submerged length changes with `d`, so it lives in `k_d`
via `A_rod`; only the *always* submerged pieces — washers, pan, nut — get a ratio. In
rev F there are two: **0.8736 for the stainless, 0.195 for the PLA pan.**

**But measure that ratio; do not take it from a table.** `m_app = m_dry·(1 − ρ_w/ρ_ss)`, and a 1 %
error in the assumed density gives a **0.145 %** error in P. That sounds tolerable until you notice
what you would be assuming:

| ρ (g/cm³) | `m_app/m_dry` | vs 304 | |
|---|---|---|---|
| 7.70 | 0.87036 | −0.376 % | 430 ferritic — common in cheap washers |
| 7.75 | 0.87120 | −0.280 % | 410 |
| 7.85 | 0.87284 | −0.092 % | the generic "steel" table value |
| **7.90** | **0.87365** | — | **304 / A2** |
| 8.00 | 0.87523 | +0.181 % | 316 / A4 |

The grade spread is **0.56 % of reading, 2.8 Pa at 500 Pa** — on its own as large as everything
else in the budget put together, and comparable to an unchecked scale span. A bin of "stainless
washers" does not say which grade it is. A magnet halves the range for free — 430 and 410 are strongly magnetic,
304 and 316 largely are not — but cold-worked 304 goes weakly magnetic, so treat it as an indicator
rather than a determination.

**Three ways to pin it down, best first:**

| method | error on P |
|---|---|
| **hydrostatic weighing of the batch** — 0.1 g on 800 g | **0.013 %** |
| displacement in a graduated cylinder — ±1 mL on 117 mL | 0.124 % |
| caliper dimensions | ~0.22 % — thickness and stamping burrs dominate |

One weighing per material, once ever. After that the arithmetic above is exact, because apparent
weights add and each ratio applies to any subset. The caliper route is the one to avoid.

### Use it differentially — the one check that removes the degeneracy

`P = k_m·m_app − k_d·d` has three unknowns that all enter as a single constant: the tare mass, the
`d` datum and the skirt meniscus force. **No absolute reading can separate them**, and 2026-09-20
left a −0.42 ± 0.19 Pa residual that is equally well explained by any of the three.

**Adding a known mass does separate them.** At constant trapped volume the bell sinks until the
extra displacement carries the extra weight, and the algebra collapses to

    ΔP = Δm·g / A_o  =  0.5405 Pa/g   (as built, A_o = 18 146 mm²)

exact, with `d`, the tare and the meniscus all cancelling from the difference. **Only the scale's
span survives**, which is the one term this file already says is the reference. Do this first with
the reference weights, before any absolute ladder point is taken seriously.

**[`calibrationBellWeighing.svg`](calibrationBellWeighing.svg) is the rig**, and the trick that makes it
easy is that **you never weigh the object in water — you weigh the water.** Tare a dish on the
scale, lower the load in from a stand that rests on the bench, and the scale reads the displaced
mass directly, because the buoyant force has an equal and opposite reaction on the liquid. No
below-balance hook, no special balance.

The sheet draws **two ways to hold the load**, and the method is identical for both:

| | |
|---|---|
| **A — a thread** | 0.3 mm fishing line, laced straight through the washers' own Ø13 bores. Smallest correction, no carrier to account for. |
| **B — the bell's own rod** | Rigid, cannot swing into a wall, and the pan is already on it. Larger correction, but it is cancelled exactly. |

**Take a blank reading if — and only if — your scale can see it.** Lower the bare thread or rod to
a marked depth, read it, then re-tare and repeat with the load at the *same* mark;
`m_disp = L − B`. It cancels the suspension's own displacement, its meniscus, a soaked knot and any
carrier, all at once. But it is worth **0.008 Pa on 0.3 mm line and 0.06 Pa on a Ø3 rod**, so on a
1 g-resolution scale it is entirely invisible and you should skip it. See the scale section below.

⚠ **Lace the washers onto the line itself rather than using a hook or a mesh bag.** A carrier is a
second material with its own displacement, and you would end up measuring the ratio of
washers-plus-carrier, which is not a quantity you need. This one still matters on a coarse scale,
because a wire hook or a bag can easily displace several grams.

## Your scale is the reference — resolution is not its important property

`P = k_m·m_app − k_d·d`, so **a scale reading (1+ε) times true makes every pressure (1+ε) times
true.** The scale's *span* error propagates straight through as a span error on the reference, and
no averaging removes it. Resolution, by contrast, is a rounding term that barely registers.

Budget at 500 Pa (`m_app` ≈ 906 g), RSS:

| term | 1 g scale, span unchecked | 1 g, span ±0.1 % | 0.1 g, span ±0.1 % |
|---|---|---|---|
| rounding, ±½ division | 0.28 Pa | 0.28 Pa | 0.03 Pa |
| **scale span** | **2.57 Pa** | 0.51 Pa | 0.51 Pa |
| stainless ratio, from 117 g of displacement | 0.57 Pa | 0.57 Pa | 0.06 Pa |
| PLA pan displacement, 16 g | 0.57 Pa | 0.57 Pa | 0.06 Pa |
| area, ID to ±0.15 mm | 0.85 Pa | 0.85 Pa | 0.85 Pa |
| `d` to ±1 mm | 0.22 Pa | 0.22 Pa | 0.22 Pa |
| **total** | **2.85 Pa = 0.57 %** | **1.32 Pa = 0.26 %** | **1.02 Pa = 0.20 %** |

> **⚠ FOUR TERMS MEASURED ON 2026-09-20 ARE MISSING FROM THAT TABLE, AND AT THE BOTTOM OF THE RANGE
> THEY DOMINATE IT.** `../../ndLouvers/pressure-testing.history.md` §3.3 owns the evidence; `pressure-testing.md` §2.2b carries the rules.
>
> | term | size | at 500 Pa | at 43 Pa |
> |---|---|---|---|
> | **wetted-bell film** after any handling, decaying ~80 s | 2.5 g = **1.4 Pa** | 0.28 % | **3.3 %** |
> | **skirt meniscus force**, contact angle unknown | 0 to **±3.9 Pa** | 0.78 % | **9 %** |
> | **tilt**, `P → Mg/(A_eff·cos θ)`, 10° | **0.44 Pa** | 0.09 % | 1.0 % |
> | **`d` read by eye, no graduations** | ±1 mm scatter **+1.7 mm systematic** | 0.15 % | 1.0 % |
>
> **The film is the one that bites in practice** — five replications in one session, and the
> before-and-after weighing in §"Measure the finished part" **cannot see it**, because that check is
> for ingress *into* the bell while the film is on the outside, present when you read and gone when
> you weigh. **Charge through the standpipe so the bell never leaves the water**, and if it must be
> handled, wait two minutes.
> **The meniscus term is a band, not a bias** — contact-angle hysteresis means it is not repeatable
> between settlings, and its sign follows whether the skirt is wetting or not. It is unmeasured.
> **The tilt term is silent**: the 65 s of record before an observed 5–8° loll was as quiet as any
> settled stretch, so **no past reading can be audited for it**. Look at the bell before every
> reading and record that you did.

Three things fall out of that table:

1. **Even the worst column is five times better than the SDP810's own 3 % span spec.** The exercise
   is worth doing on whatever scale you already own; it is not worth abandoning for want of a
   better one.
2. **Checking the span is worth far more than buying resolution.** It takes the total from 0.57 %
   to 0.26 %; going from 1 g to 0.1 g on top of that buys only 0.26 % → 0.20 %, because by then the
   **area** term dominates and the scale has stopped mattering.
3. **So buy one calibration weight, not a better scale.** A 500 g or 1 kg M1/F2 weight is the
   cheapest item in this whole project and it is the reference for the reference. Check near the
   *working* mass — a span check at 500 g assumes linearity up to 900 g. Weighing A, B and A+B
   separately tests linearity, but it cannot see a pure gain error, so it is not a substitute.

### Two things to do differently on a coarse scale

4. **Do not weigh the pan's displacement — compute it.** 16 g read to ±1 g is ±6 %, and because the
   pan is always fitted this lands as a constant **±0.57 Pa** offset, which at 125 Pa is the single
   largest term in the budget. `m_disp,pan = m_dry,pan / 1.24` instead: PLA density is good to ~3 %,
   giving ±0.27 Pa, twice as good as weighing it.
5. **Weigh more washers than you need for the ratio.** The ratio's error scales as (scale
   resolution)/(displacement measured), so measuring 24 washers instead of 12 halves that term for
   the price of a few washers.

### What a 1 g scale can no longer see — stop worrying about all of it

| | | |
|---|---|---|
| 0.014 g | 0.008 Pa | thread blank: line displacement plus meniscus |
| 0.070 g | 0.040 Pa | meniscus on a Ø3 rod |
| 0.100 g | 0.057 Pa | rod blank |
| 0.310 g | 0.176 Pa | 2 mm cotton string instead of 0.3 mm line |
| 0.300 g | 0.170 Pa | twenty 3 mm bubbles clinging to the stack |
| **1.000 g** | **0.567 Pa** | a 1 cm³ void in the pan — **exactly one division** |

Everything above the last row is below a single division, so the blank, the meniscus, the thin-line
requirement and the bubble hygiene are all moot at this resolution. **The last row is not:** keep
printing the pan solid, keep the rule that no *visible* bubble may be left on the stack, and keep
the before-and-after session weighing — it now catches ingress of 1 g or more, which is still the
size that matters.

### The PLA pan is the part that needs this most

Its density hardly matters — a 1 % error is **0.09 Pa**, because PLA at 1.24 is nearly neutrally
buoyant. But **a 1 cm³ internal void is 1 g of buoyancy, 0.57 Pa**, six times larger, and a void
that slowly floods moves the tare with nothing in the record to show it.

So **print the pan solid** — enough walls, top and bottom layers and 100 % infill that there is no
enclosed air. The hydrostatic weighing catches any void on the day; the before-and-after session
weighing catches flooding later.

Water density over 10–30 °C moves `m_app` by 0.038 % — negligible, but §2.2 already asks for the
water temperature, so carry it.

## Measure the finished part

Print shrinkage does not matter, because nothing here is made *to* the drawing — it is measured
after printing and the constants recomputed. **§"As built" is that measurement for the bell that
exists, and it is what caught a 23 % error in `k_d`.** Repeat all of it for any new print.

1. Two **perpendicular** inside diameters at the rim: `A_eff = π·d₁·d₂/4 − π·d_rod²/4`. The first
   term is exact for an ellipse, so ovality costs nothing as long as both are measured.
2. Derive `A_wall` from the same two IDs and the measured OD.
3. Weigh the whole assembly **before and after every session**. A change is water ingress into the
   bell, and it invalidates the session rather than needing a correction.

## What it can and cannot reach

| | |
|---|---|
| Range, **as built and demonstrated** | **38.2 Pa → 387.8 Pa** — the six ladder rungs of `../../ndLouvers/pressure-testing.md` §2.4, `m_app` 98.8 → 728.5 g read at d = 70 mm |
| Range, rev F nominal | 42.8 Pa → 500 Pa (36.3 Pa rod-less, below) — **not this bell** |
| Resolution | 1 g = 0.567 Pa; a 0.1 g scale resolves 0.06 Pa |
| Dry stainless at 500 Pa | 923 g — 24 mm of Ø80 stack |
| Dry stainless at 125 Pa | 166 g |
| Uncertainty | **set by your scale** — see below. ±0.26 % at 500 Pa on a 1 g scale with its span checked |

Against the SDP810's own **3 % of reading** span accuracy that is roughly a tenfold margin, which
is what makes the exercise worth doing at all.

> **⚠ THE BARE BELL HAS NO USABLE RANGE, AND THE FLOOR IS NOT WHAT LIMITS IT.** Measured
> 2026-09-20: it lolls below an immersion of ~60 mm and grounds at 68 mm in the tub as filled, so
> the whole bare working window is **immersion 60–68 mm, P ≈ 27.1 → 26.1 Pa — about 1 Pa wide.**
> The 36.3 Pa "rod-less floor" below is a mass-over-area figure and **is not reachable**: the bell
> tips over before it gets there. **Hang load.** 50 g of washers 150 mm below the lid takes `GM`
> from ≈0 to **+28 mm** at a working point of ~52 Pa — the fixture is sound, the bare configuration
> is the broken one, and note 2's "bare is safe / treat the bare point as the only rod-less one" is
> the opposite of what this bell can do.
> **Two other limits came out of the same session.** The tub held ~68 mm of water against the
> **≥180 mm** specified, which is what sets the grounding limit and denies the bell the only depths
> at which it is stable. **That grounding figure is superseded** — the tub has since been filled
> deeper, and 2026-09-21 reached **d = 78.5 mm** before the bell was near the bottom at the 726 g
> load (`../../ndLouvers/pressure-testing.history.md` §3.10). The ladder's **d = 70 mm** read depth
> lives in the margin that buys, and is above the 64.5 mm at which the bell returns upright unaided.
> And **lifting the bell put −367 Pa on the line** — harmless to the part
> (P_max 1 bar) but **3× full scale for a ±125 Pa sensor**, so never have one connected to a bell
> that is being lifted, recharged or straightened.

**The floor is the lid, hub, rod and pan, not the skirt.** PLA is only 1.24× the density of water,
so submerged PLA is nearly weightless and a taller skirt costs almost nothing.

> **⚠ THE AS-BUILT FLOOR IS 38.2 Pa, NOT 42.8.** The 42.8 is rev F nominal — `k_m` 0.5672 on a 100 g
> tare — and this bell is lighter with a lower `k_m` and a deeper usable `d`: `m_app` **98.8 g** at
> `k_m` **0.5639** read at **d = 70 mm** gives **38.8 Pa**, which is ladder rung 1 of
> `../../ndLouvers/pressure-testing.md` §2.4. **It is reached with hung load, not bare** — that is
> the whole difference from the warning above, which is about the bare configuration and stands.
> Quote 38.2; the rev F figure describes a bell that was never built.

Below the floor,
rely on the part's own **0.08 Pa zero accuracy** and `< 0.05 Pa/year` offset stability. Since span
error is *multiplicative*, points at 50/100/120 Pa constrain the span that governs 2 and 5 Pa as
well, and §2.9 explicitly allows reporting a coarser bound rather than inventing a better reference.

## The constraint that sets the washer diameter

**The whole stack must stay below the inner water surface at every point**, and 500 Pa is the worst
case because `h = P/ρg` is largest there — 51 mm, putting the inner surface 101 mm below the lid
against 126 mm to the stack top.

117 cm³ of stainless is **24 mm of stack on Ø80 washers and 64 mm on Ø50**. Ø50 emerges. That, and
nothing else, is why the drawing says Ø80.

## Bubbles: a 0.1–0.3 Pa effect, not a showstopper

A 1 cm³ bubble is 12 mm across and would be obvious. Realistic bubbles on smooth stainless are
1–3 mm, **0.0002–0.006 Pa each**; the thin film between two stacked flat washers is ~0.01 Pa per
interface. Wet and tap the stack before lowering it, load any dished washer **concave-down**, and
let the repeat readings §2.5 already requires catch the rest. It matters proportionally at the
43 Pa floor (≈0.7 %) and is negligible at 500 Pa.

Stainless rather than plain steel is not fussiness: rust would change the mass between sessions and
nothing in the record would show it. The same argument retires the aluminium rod — aluminium against
stainless in tap water is a galvanic pair, slow but real, and it would have eaten mass out of the
tare with no external sign.

The **submerged M3 thread** is the one deliberate trap the design still has: 71 mm³ of helical groove
over the ~49 mm that sits below the surface, worth **0.04 Pa** if it all stayed dry. A 0.5 mm groove
holds air stubbornly once it starts dry, so tap the rod once under water and it is gone.

## Print notes

| Part | Orientation | Why |
|---|---|---|
| 1 Skirt | **rim down** | the first layer is the sealing edge, so it comes off the plate flat and square. Use a brim — a thin 150 mm ring lifts at a corner and goes oval. |
| 2 Lid | **top face down** | collar, ribs and hub grow upward, no supports |
| 3 Pan | flat | trivial |

Wall 0.80 on the skirt is two perimeters at a 0.4 nozzle, no top/bottom layers, no infill.
**The bell that exists came out at 1.00**, against 0.90 in its own STL and 0.80 here — so **a thin
wall does not print to its nominal, and `k_d` is proportional to it.** Measure it; do not assume it.
**Print the graduations.** The existing bell has none, so its immersion depth is read by eye against
a plain skirt, which measured ±1 mm of scatter plus a ~1.7 mm systematic — the single largest
avoidable term in §"As built" item 4.

**Airtightness is not an accuracy requirement.** A pinhole costs supply flow, not pressure — the
pressure is set by weight over area whatever leaks. Do glue the skirt-to-collar joint, because a
large leak wastes the syringe.

**The rod threads into the hub — do not glue it.** Tap the hub M3 right through the lid, or print
the thread. At 25 mm engagement the PLA thread shears at roughly 3.5 kN against the 9 N it
carries, so there is no strength reason to make it tight and every reason not to strip it.
Squareness is uncritical: the washers have
34 mm of radial clearance to the skirt, and an off-axis hang changes nothing in the formula — which is the second thing this layout buys,
since rev B needed the load centred to 1 mm to keep bearing friction down.

## Things that would corrupt every reading

1. **Nothing compliant may touch the moving bell.** The gas enters through a **standpipe fixed to
   the tub floor**, which is what the rev A/B core used to do. A tube run to the lid adds its own
   stiffness to the weight and the error is silent.
   > **⚠ THIS HAPPENED, IT COST A RUN, AND IT IS THE LARGEST ERROR THIS FIXTURE HAS PRODUCED**
   > (2026-09-21, `../../ndLouvers/pressure-testing.history.md` §3.7). The pressure line bore on the bell
   > for a whole session and the bell was seen **hanging on it** at the end. The tube in
   > compression put a constant **+5.6 Pa — 10 g, 0.11 N** — on a 180 Pa point, then as the bell
   > sank it went slack and into tension and began taking the weight instead, which is an
   > **accelerating** pressure loss and not something a bleed or a pinch can imitate.
   > **Three things to carry:**
   > **(a) It is invisible inside the run.** That session had the cleanest instrument record on
   > file, perfect counters, and three collinear points. A constant force is degenerate with the
   > tare mass, the `d` datum and the meniscus — and it is an order of magnitude larger than all
   > three, so the degeneracy is not a small-print caveat here.
   > **(b) The detector is a length-change pair, not a better single run.** The contact force does
   > not scale with line length and the line loss does, so two runs at different line lengths
   > over-determine the system and the contact has nowhere to hide. That is the only reason this
   > one was caught.
   > **(c) Charge and settle through the standpipe, and look at where the line goes** before every
   > session, not just at whether the bell is upright.
2. **Keep `d` greater than `h = P/ρg`**, 51 mm at 500 Pa. Below that the bell blows out under its
   own rim and the pressure is capped by immersion depth instead of mass.
3. **Keep the sensor at the height of the bell's water line**, or correct at **0.0118 Pa/mm** of
   air column. 100 mm is 1.2 Pa, which is 1 % of a 125 Pa point. This applies to every version of
   this rig and was missing from revs A and B.
   > **⚠ THIS CORRECTION MAY DOUBLE-COUNT, AND IT HAS NOT BEEN RE-DERIVED** (plan open item 56). 0.0118 Pa/mm is exactly
   > `ρ_air·g`, i.e. the full uncancelled air column. But **both sensor ports are at the sensor** —
   > one fed by an air-filled tube from the bell, the other open to room air — and the two columns
   > have the same density, so to first order they cancel and the residual is `Δρ·g·Δz`, of order
   > 0.2 Pa/m for a few kelvin of tube-to-room temperature difference. If that is right the term is
   > ~50× smaller than stated. **It is quoted as 1 % of a 125 Pa point, so it is worth settling
   > before it stays in the budget.** Left as written pending an owner decision; it is conservative
   > either way.

## The rod-less point, and what it is actually worth

Unscrewing the rod and fitting an M3×6 grub screw in its place gives a **second configuration**
with its own constants, because removing the rod changes `A_eff` *and* `k_d` together:

**All four constants in the table below are rev F NOMINAL**, and the bell that exists is rod-less
with `k_m` **0.5639** and `k_d` **0.2367** — see §"As built". The rod-less *row* is the closest
match to it and is still 23 % out on `k_d`.

| | `k_m` | `k_d` | tare | floor |
|---|---|---|---|---|
| rod and pan fitted | 0.5672 Pa/g | 0.2155 Pa/mm | 100 g | **42.8 Pa** |
| rod out, M3×6 grub screw in | 0.5670 Pa/g | 0.2122 Pa/mm | 88 g | **36.3 Pa** |

**It reaches only 6.5 Pa lower, so it is not a way to extend the ladder.** Its real value is as a
**model check**: it moves both constants at once, so if the bare rod-less reading lands where the
rodded fit predicts, that corroborates the whole force balance rather than adding one more point to
the curve it was fitted from. §2.7 asks for validation points held back from the fit, and a
configuration change the model predicts correctly is stronger evidence than another point on the
same curve.

**Blank it, don't leave it open.** An open Ø3 hole passes ~35 mL/s at 41 Pa — thirty-five times
the sensor's bleed, and no syringe keeps up with that. An M3×6 grub screw beats tape: repeatable,
and its mass is a known constant. Weigh it with the bell (~0.3 g).

⚠ **Do not put weights on the lid in this configuration — that is rev A again.** The stability
budget is `K = 0.0104 + 9.807×10⁻⁶·(3076 − 15·m)` with `m` in grams sitting 15 mm above the lid:

| load on the lid | K | pressure |
|---|---|---|
| 0 g | +0.041 N·m/rad | 36.3 Pa |
| 72 g | +0.030 | 76.8 Pa |
| 140 g | +0.020 | 115.4 Pa |
| **276 g** | **0** | **192.5 Pa — capsizes** |

The bench has already shown that this margin cannot be felt until it goes, so treat the **bare**
reading as the only rod-less one.

> **⚠ THE BENCH HAS NOW SHOWN SOMETHING WORSE, AND IT INVERTS THE ADVICE ABOVE.** On 2026-09-20 a
> **bare, unloaded, rod-less** bell capsized twice and lolled 5–8° with nothing at all on the lid
> (`../../ndLouvers/pressure-testing.history.md` §3.3e, plan open item 55). The `0 g → +0.041 N·m/rad` row
> is the same figure the main stability table gives, and **it is contradicted by the same
> measurement** — so this table cannot be trusted either, and **"treat the bare reading as the only
> rod-less one" is the opposite of what this bell can do: the bare reading is the one that cannot
> be taken.** The model check this section describes is still worth having; it needs a bell that
> stays upright, which means hanging load.

## Operating sequence

1. Feed air through the standpipe from a syringe or a pump with a bleed. **Never lift the bell to
   recharge it** — that is what leaves the 1.4 Pa film, and the standpipe exists to avoid it.
   **Check where the pressure line runs before every session**: a line bearing on the bell put a
   silent constant **+5.6 Pa** on a whole run on 2026-09-21 (§"Things that would corrupt every
   reading" item 1).
   Measured demand: **0.32 mL/s at ~28 Pa, rising to ~5.5 mL/s at 500 Pa** (2026-09-20). **A leak
   test with the sensor removed showed zero sinking in 10 minutes**, so essentially all of it is the
   sensor's bypass rather than a leak in the bell or its lines. Unsupplied, the bell sags
   **0.19–0.3 Pa/min** at 28 Pa, which is **0.7–1.0 mm/min of sinking**.
2. **Nothing has to be pumped to a commanded pressure.** Read `m_app` and `d` at the moment the
   sensor is read, and record where it landed — §2.4's ladder values are targets to land near.
   **Within 30 s**: at 0.8 mm/min a two-minute gap is 1.6 mm = 0.4 Pa.
2a. **Look at the bell and record that it is upright.** A 5–8° lean is invisible in the data and
   worth a few tenths of a pascal through `cos θ`; see the tilt row in the budget. **If it is
   leaning, the bigger cost is the waterline, not `cos θ`** — ±6.6 mm around the skirt at 5°,
   1.6 Pa — so read `d` as the mean of the high and low sides and **record that you did.**
   It self-corrects as it sinks: expect upright above d ≈ 60 mm, lolling below.
2b. **After any contact, wait two minutes** for the film to drain before pairing anything.
3. Tap the bell before every reading to free seal stiction. **This counts as contact** — it costs
   you the two minutes in 2b, and on a bare bell at low immersion it can put it over.
4. Changing washers means lifting the bell out and re-settling. That is the price of having no
   bearings, and it is the one thing rev B did better.
5. Dry the parts after use, and re-measure the ID if they have soaked for hours. PLA is hygroscopic
   and hydrolyses slowly in warm water; never store the bell assembled and wet.
