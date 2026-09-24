#!/usr/bin/env python3
"""Recompute a session's pressure correction offline and compare it with what the logger logged.

Usage:  python Tools/pressure-correction-check.py <session.ndjson> [...]

The parameters come from the session's own pressureBaseline `correction`, so a recording is
checked against the configuration it was made with. For every channel with a logged
`correctedPa`, the correction is recomputed from `pressurePa` and the record's
`absolutePressurePa`; the report gives the largest difference per channel and checks that
`sentDeciPa` is that value on the wire.

A valid raw reading with no `correctedPa` must be explained: no held absolute pressure, one
older than the hold bound, or a corrected value past the wire's int16. A run of cycles with no
usable absolute pressure is a stale-hold period, and each one must open with a warning event
and, unless the session ended inside it, close with a recovery event.

Exit status is 1 if any check fails.
"""
import json, math, sys

TOLERANCE_PA = 0.01
INT16_MAX = 32767
REFUSALS = ("absolutePressureMissing", "absolutePressureStale")


def correct(rdg, absolute_pa, calibration_pa, r, r_fixed, ch):
    if rdg == 0:
        return 0.0
    b = absolute_pa / calibration_pa
    eps = ch["spanPositive"] if rdg >= 0 else ch["spanNegative"]
    dp_port = abs(rdg) / (b * (1 + eps))
    q = (dp_port / (ch["bypassCoefficient"] * 1e-6)) ** (1 / (ch["bypassExponent"] + 1))
    r_path = sum(r_fixed + r * length for length in (ch["lineLengthHighM"], ch["lineLengthLowM"]) if length > 0)
    return math.copysign(dp_port + q * 1e-6 * r_path, rdg)


def round_half_away(x):
    return int(math.copysign(math.floor(abs(x) + 0.5), x))


def check(path):
    records = [json.loads(line) for line in open(path, encoding="utf-8") if line.strip()]
    baselines = [r for r in records if r["t"] == "pressureBaseline"]
    if not baselines or "correction" not in baselines[0]:
        print(f"{path}: no pressureBaseline correction - recorded before the correction existed")
        return False

    c = baselines[0]["correction"]
    if c["formula"] != "divide":
        print(f"{path}: formula '{c['formula']}' is not the one this script checks")
        return False
    slots = {ch["ch"]: ch for ch in c["channels"]}
    hold_ms = c["absoluteHoldMs"]

    ok = True
    largest = {name: (0.0, None) for name in slots}
    corrected_count = {name: 0 for name in slots}
    periods = []
    in_period = False

    for rec in (r for r in records if r["t"] == "pressure"):
        absolute_pa, age_ms = rec["absolutePressurePa"], rec["absolutePressureAgeMs"]
        refused = absolute_pa is None or age_ms > hold_ms
        for ch in rec["channels"]:
            name = ch["ch"]
            if not ch["valid"]:
                continue
            if ch["correctedPa"] is None:
                expected = ("absolutePressureMissing" if absolute_pa is None
                            else "absolutePressureStale" if age_ms > hold_ms else "correctedOutOfRange")
                if ch["reason"] != expected or ch["sentDeciPa"] != -32768:
                    print(f"cycle {rec['cycle']} {name}: uncorrected with reason {ch['reason']}"
                          f" and sentDeciPa {ch['sentDeciPa']}, expected {expected} and -32768")
                    ok = False
                continue

            if absolute_pa is None or age_ms > hold_ms:
                print(f"cycle {rec['cycle']} {name}: corrected with absolute pressure"
                      f" {absolute_pa} Pa aged {age_ms} ms, past the {hold_ms} ms hold")
                ok = False
                continue

            recomputed = correct(ch["pressurePa"], absolute_pa, c["calibrationAbsolutePa"],
                                 c["tubingResistancePerMetre"], c["lineFixedResistance"], slots[name])
            diff = abs(recomputed - ch["correctedPa"])
            corrected_count[name] += 1
            if diff > largest[name][0] or largest[name][1] is None:
                largest[name] = (diff, rec["cycle"])

            deci = round_half_away(ch["correctedPa"] * 10)
            if abs(deci) > INT16_MAX or ch["sentDeciPa"] != deci:
                print(f"cycle {rec['cycle']} {name}: sentDeciPa {ch['sentDeciPa']} is not"
                      f" correctedPa {ch['correctedPa']} on the wire ({deci})")
                ok = False

        if refused and not in_period:
            periods.append({"start": rec["cycle"], "end": None})
            in_period = True
        elif not refused and in_period:
            periods[-1]["end"] = rec["cycle"]
            in_period = False

    events = [r for r in records if r["t"] == "event"]
    opened = [e for e in events if e["severity"] == "warning" and e["message"].startswith(REFUSALS)]
    closed = [e for e in events if e["message"].startswith("absolute pressure back")]
    expected_closed = sum(1 for p in periods if p["end"] is not None)

    print(f"{path}")
    print(f"  correction: {c['formula']}, hold {hold_ms} ms, r {c['tubingResistancePerMetre']:g},"
          f" R_fixed {c['lineFixedResistance']:g}")
    for name, (diff, cycle) in largest.items():
        if corrected_count[name] == 0:
            print(f"  {name}: no corrected values")
            continue
        status = "ok" if diff < TOLERANCE_PA else "FAIL"
        ok = ok and diff < TOLERANCE_PA
        print(f"  {name}: {corrected_count[name]} corrected values, largest difference"
              f" {diff:.6f} Pa (cycle {cycle}), R_path {slots[name]['linePathResistance']:g}  {status}")
    for p in periods:
        print(f"  stale-hold period: cycles {p['start']}..{p['end'] if p['end'] else 'end of session'}")
    events_ok = len(opened) == len(periods) and len(closed) == expected_closed
    ok = ok and events_ok
    print(f"  stale-hold periods {len(periods)}, warning events {len(opened)};"
          f" recoveries {expected_closed}, recovery events {len(closed)}  {'ok' if events_ok else 'FAIL'}")
    return ok


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    results = [check(path) for path in sys.argv[1:]]
    sys.exit(0 if all(results) else 1)
