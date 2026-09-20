# Liquid-sealed pressure bell — printed reference source

**Status: an OPTION, not an adopted decision.** The reference-setup route for Step 0b's pressure
qualification is undecided (`../../ndLouvers/pressure-testing.md` §2.2), so nothing in that file
points here yet. This is a fabrication package, not a requirement, and it closes no open item.

> **⚠ LIVING IN THIS REPOSITORY CHANGES NOTHING ABOUT WHAT IT MAY DECIDE.** `CLAUDE.md` here is
> explicit that this repository owns software, host configuration and box-2 hardware, and **owns
> no measurement decision** — `../../ndLouvers/CFD-Learning-Plan.md` Step 0b is the authority on
> calibration and acceptance, exactly as it is for the perfboard build sheet. This file owns the
> fixture: its geometry, its constants and how to use it. It does not own whether the fixture is
> adopted, what accuracy is required of it, or what counts as a qualified reference.
>
> Section numbers like §2.2 and §2.9 below refer to `../../ndLouvers/pressure-testing.md`.
> Cross-repo links resolve on disk and 404 on the public git host; that is accepted here, as
> `CLAUDE.md` says.

[`calibrationBell.svg`](calibrationBell.svg) is the drawing, at **rev F**, and
[`calibrationBellWeighing.svg`](calibrationBellWeighing.svg) is the rig for the one calibration
weighing it needs. This file is what the drawings cannot carry: why the thing is shaped this
way, and what it can and cannot do.

Printed parts live beside them: `calibrationBell.stl`, `calibrationBellPan.stl`,
`calibrationBell.3mf` and `pipeStand.stl`.

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
after printing and the constants recomputed.

1. Two **perpendicular** inside diameters at the rim: `A_eff = π·d₁·d₂/4 − π·d_rod²/4`. The first
   term is exact for an ellipse, so ovality costs nothing as long as both are measured.
2. Derive `A_wall` from the same two IDs and the measured OD.
3. Weigh the whole assembly **before and after every session**. A change is water ingress into the
   bell, and it invalidates the session rather than needing a correction.

## What it can and cannot reach

| | |
|---|---|
| Range | **42.8 Pa → 500 Pa** (36.3 Pa rod-less, below) |
| Resolution | 1 g = 0.567 Pa; a 0.1 g scale resolves 0.06 Pa |
| Dry stainless at 500 Pa | 923 g — 24 mm of Ø80 stack |
| Dry stainless at 125 Pa | 166 g |
| Uncertainty | **set by your scale** — see below. ±0.26 % at 500 Pa on a 1 g scale with its span checked |

Against the SDP810's own **3 % of reading** span accuracy that is roughly a tenfold margin, which
is what makes the exercise worth doing at all.

**The 42.8 Pa floor is the lid, hub, rod and pan, not the skirt.** PLA is only 1.24× the density of water,
so submerged PLA is nearly weightless and a taller skirt costs almost nothing. Below the floor,
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
2. **Keep `d` greater than `h = P/ρg`**, 51 mm at 500 Pa. Below that the bell blows out under its
   own rim and the pressure is capped by immersion depth instead of mass.
3. **Keep the sensor at the height of the bell's water line**, or correct at **0.0118 Pa/mm** of air
   column. 100 mm is 1.2 Pa, which is 1 % of a 125 Pa point. This applies to every version of this
   rig and was missing from revs A and B.

## The rod-less point, and what it is actually worth

Unscrewing the rod and fitting an M3×6 grub screw in its place gives a **second configuration**
with its own constants, because removing the rod changes `A_eff` *and* `k_d` together:

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

## Operating sequence

1. Feed air through the standpipe from a syringe or a pump with a bleed.
2. **Nothing has to be pumped to a commanded pressure.** Read `m_app` and `d` at the moment the
   sensor is read, and record where it landed — §2.4's ladder values are targets to land near.
3. Tap the bell before every reading to free seal stiction.
4. Changing washers means lifting the bell out and re-settling. That is the price of having no
   bearings, and it is the one thing rev B did better.
5. Dry the parts after use, and re-measure the ID if they have soaked for hours. PLA is hygroscopic
   and hydrolyses slowly in warm water; never store the bell assembled and wet.
