# Surface static button — hood tap 5D

This file owns the button's geometry, build and derived constants. Measurement requirements and
acceptance belong to [ndLouvers instrumentation-spec.md](../../ndLouvers/instrumentation-spec.md);
[pressure-testing.md §1.7](../../ndLouvers/pressure-testing.md#17-a3--the-hood-surface-static-button)
owns the method, its bias qualification and its role in A3.

[surfaceStaticButton.svg](surfaceStaticButton.svg) is the rev A drawing. No STL exists yet.

## Design

1. **The pressure path is brass; the printed body is a fairing.** A Ø2.0/1.3 mm brass tube lies
   on the hood in a slot through the body, its crown flush with the Ø12 flat top, and the Ø0.8
   sensing hole is drilled in that crown. Brass is there for a clean, square-edged hole, not for
   porosity, which is accepted at this pickup (`pressure-testing.md` §1.2).
2. **Body:** Ø30.0 base, Ø12.0 flat top at 2.0, a straight ramp to a 0.4 edge (about 10°). The
   slot is 2.2 wide and full height, from 4.0 ahead of the hole to the aft edge.
3. **Tube:** 30 long. The nose is closed with a rounded solder plug 3.0 ahead of the hole, the
   plug ending at least 1.0 ahead of the hole's edge. The tube runs aft to a 12.0 stub past the
   body's edge, where a reducer or a 2/4 silicone sleeve takes it to the 4/6 car line. That joint
   must pass `pressure-testing.md` §1.3 item 1's checks.
4. **It is never smoke-tested directly.** Channel D is tested through wand 2D with the button left
   as found (`pressure-testing.md` §2.4 step 5), which puts the brass and its joint in the tested
   flow path. Keep the flat top smooth across the brass stripe for the flow, not for a seal.

## Build

1. Print three — two for §1.7's side-by-side bias check and a spare. Base down, 0.1 layers,
   ironing on the top, 100 % infill, ASA or PETG: a hood in sun exceeds PLA's softening point.
   Adjust the slot in the slicer until the tube drops in; do not file it.
2. Cut the tube to 30, close the nose with solder and round it. Drill the Ø0.8 hole 3.0 behind
   the nose end, square to what will be the base, and deburr inside without enlarging or
   countersinking it.
3. Bond the tube into the slot with epoxy, filling the gaps, with none in the hole or bore. Flush
   the tube with clean dry air.
4. Fit with thin double-sided tape under the body and tube, tube axis along the local flow and
   the stub aft. Feather the edge with tape and keep tape off the flat top. Record the hole's
   coordinates on the scan.

## Line resistance

The 1.3 mm bore is not negligible. Hole to stub end is 27 mm of flowing brass, **≈ 7.0 × 10⁶
Pa·s/m³** by Poiseuille at 20 °C (derived; a ±0.05 mm bore tolerance moves it ±16 %). The
logger's `R_fixed` counts a wand (~0.85 × 10⁶) where this port has the button, so the net
addition is ≈ 6.2 × 10⁶, **equivalent to about 2.0 m of car stock**. Uncorrected, it reads about
4.5 % low at 70 Pa, which would fail the smoke test's light point.

So until `pressure-testing.md` §2.4 step 2's bench A/B against a wand has measured it, enter the
button's port length as its silicone length **plus 2.0 m** (KnurLogger
[pressure-correction.md](../pressure-correction.md) step 8). The A/B's measured figure replaces
the 2.0 m. The installed smoke test's light point then checks it end to end, since the test flow
crosses the brass.

## Rain

The hole floods in rain whatever the body is made of, so **D is not valid in rain**, and water
that enters runs down the line toward the sensor cluster. The line's trap below the sensor
(`pressure-testing.md` §1.4) is what protects the SDP810; inspect it after any wet drive.
