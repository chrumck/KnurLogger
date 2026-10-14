#!/usr/bin/env python3
"""Refit a driven session's cavity pressure against speed and GPS altitude, fitting the altitude
term instead of assuming it.

WHY THIS EXISTS. A cavity Cp is read as p = p0 + Cp*q once the barometric head of the route's
elevation has been taken out. Taking it out means subtracting rho*g*h with GPS altitude for h --
and GPS altitude carries several metres of error, where one metre is 12 Pa. Subtracting it in
full therefore injects that error into the signal being measured. Fitting the coefficient instead
says how much of the altitude trace is real: its physical value is -1, and anything closer to zero
is the fraction of the GPS variance that is error, by the usual errors-in-variables attenuation.

A fit that returns a coefficient far from -1 has not separated the two terms, and a reduction that
forced -1 on the same data over-corrected by one over that coefficient.

INPUTS. A logger SD session (.ndjson) for enclosurePressurePa, and the RaceChrono .rcz of the same
drive for speed and altitude. They do not share a clock: car sessions boot with an unsynchronised
wall clock, so the two are aligned here the way thermals-testing.md 2.3 does it, by
cross-correlating cavity temperature against low-passed GPS speed. That aligns bulk groupings and
not transients, so --shift sweeps the alignment and the fit is reported against it.
"""

import argparse
import json
import sys
import zipfile
from collections import defaultdict

import numpy as np

GPS_DEVICE = 100
GRAVITY = 9.80665
DRY_AIR_R = 287.058
PA_PER_METRE = 12.0
STILL_KMH = 2.0


def readSession(path):
    """(bootSeconds, pressurePa, cavityC) and (bootSeconds, ambientC) from an SD session."""
    encBoot, pressure, cavity, ambBoot, ambient = [], [], [], [], []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            try:
                record = json.loads(line)
            except ValueError:
                continue
            kind = record.get("t")
            if kind == "enclosure" and record.get("pressureValid"):
                value = record.get("enclosurePressurePa")
                if value is not None and 5.0e4 < value < 1.2e5:
                    encBoot.append(record["bootUs"] / 1e6)
                    pressure.append(value)
                    cavity.append(record.get("cavityTemperatureC"))
            elif kind == "temp":
                for channel in record.get("channels") or []:
                    if channel.get("name") == "temp0" and channel.get("valid"):
                        centi = channel.get("centiC")
                        if centi is not None:
                            ambBoot.append(record["bootUs"] / 1e6)
                            ambient.append(centi / 100.0)
    return (np.array(encBoot), np.array(pressure, float), np.array(cavity, float),
            np.array(ambBoot), np.array(ambient, float))


def readGps(path):
    """The longest GPS fragment in the archive: (name, seconds, speedMs, altitudeM)."""
    archive = zipfile.ZipFile(path)
    files = defaultdict(dict)
    for name in archive.namelist():
        fragment, _, leaf = name.rpartition("/")
        if leaf.startswith("channel_1_%d_0_" % GPS_DEVICE):
            files[fragment][leaf.split("_", 4)[4]] = archive.read(name)
    best = None
    for fragment, group in files.items():
        if "1_1" not in group:
            continue
        times = np.frombuffer(group["1_1"], "<i8")
        if best is None or len(times) > len(best[1]):
            best = (fragment, times, group)
    if best is None:
        raise SystemExit("no GPS in archive")
    fragment, times, group = best
    return (fragment, (times - times[0]) / 1000.0,
            np.frombuffer(group["4_0"], "<i4") / 1000.0,
            np.frombuffer(group["5_0"], "<i4") / 1000.0)


def lowpass(values, tau):
    out = np.empty_like(values)
    accumulator = values[0]
    for index, value in enumerate(values):
        accumulator += (value - accumulator) / tau
        out[index] = accumulator
    return out


def align(grid, speed, encBoot, cavityC, span):
    """Offset in seconds minimising corr(cavity temperature, low-passed speed)."""
    smoothed = lowpass(speed, 180.0)
    best = (None, 1.0)
    for offset in np.arange(-span, span, 1.0):
        sampled = np.interp(grid + offset, encBoot - encBoot[0], cavityC,
                            left=np.nan, right=np.nan)
        usable = np.isfinite(sampled)
        if usable.sum() < len(grid) // 2:
            continue
        correlation = np.corrcoef(sampled[usable], smoothed[usable])[0, 1]
        if correlation < best[1]:
            best = (offset, correlation)
    return best


def fitBoth(pressure, head, q):
    design = np.vstack([np.ones_like(q), head, q]).T
    beta, _, _, _ = np.linalg.lstsq(design, pressure, rcond=None)
    residual = pressure - design @ beta
    errors = np.sqrt(np.sum(residual ** 2) / (len(q) - 3)
                     * np.diag(np.linalg.inv(design.T @ design)))
    return beta, errors, residual.std()


def fitOne(pressure, q):
    design = np.vstack([np.ones_like(q), q]).T
    beta, _, _, _ = np.linalg.lstsq(design, pressure, rcond=None)
    return beta, (pressure - design @ beta).std()


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("session", help="logger SD session .ndjson")
    parser.add_argument("archive", help="RaceChrono .rcz of the same drive")
    parser.add_argument("--bands", default="0,25,60,95,135,200")
    parser.add_argument("--shift", type=float, default=30.0,
                        help="sweep the alignment by up to this many seconds (default 30)")
    parser.add_argument("--search", type=float, default=3000.0,
                        help="alignment search half-width in seconds (default 3000)")
    args = parser.parse_args(argv)

    encBoot, pressurePa, cavityC, ambBoot, ambientC = readSession(args.session)
    if not len(pressurePa):
        raise SystemExit("no valid enclosure pressure in that session")
    fragment, gpsTime, speedMs, altitudeM = readGps(args.archive)
    grid = np.arange(0.0, gpsTime[-1], 1.0)
    speed = np.interp(grid, gpsTime, speedMs)
    altitude = np.interp(grid, gpsTime, altitudeM)

    offset, correlation = align(grid, speed, encBoot, cavityC, args.search)
    if offset is None:
        raise SystemExit("could not align the two records")
    pressure = np.interp(grid + offset, encBoot - encBoot[0], pressurePa,
                         left=np.nan, right=np.nan)
    ambient = np.interp(grid + offset, ambBoot - encBoot[0], ambientC,
                        left=np.nan, right=np.nan)
    usable = np.isfinite(pressure) & np.isfinite(ambient)
    grid, speed, altitude = grid[usable], speed[usable], altitude[usable]
    pressure, ambient = pressure[usable], ambient[usable]

    kmh = speed * 3.6
    rho = pressure / (DRY_AIR_R * (ambient + 273.15))
    q = 0.5 * rho * speed ** 2
    still = kmh < STILL_KMH
    if still.sum() < 10:
        raise SystemExit("fewer than 10 stationary seconds: no reference to read against")
    dh = altitude - altitude[still].mean()
    head = rho * GRAVITY * dh

    print("== %s" % args.archive)
    print("   GPS fragment %s, %d s, peak %.1f km/h; aligned at %+.0f s, r = %.3f"
          % (fragment or "(root)", len(grid), kmh.max(), offset, correlation))
    print("   raw pressure spread %.1f Pa; altitude span %.1f m, sd %.2f m"
          % (pressure.max() - pressure.min(), altitude.max() - altitude.min(), altitude.std()))

    beta, errors, residual = fitBoth(pressure, head, q)
    attenuation = -beta[1]
    print("\n   FITTED      altitude coefficient %+.3f +/- %.3f   (physical value -1)"
          % (beta[1], errors[1]))
    print("               Cp %+.4f +/- %.4f, residual sd %.1f Pa"
          % (beta[2], errors[2], residual))
    forced, forcedResidual = fitOne(pressure + head, q)
    plain, plainResidual = fitOne(pressure, q)
    print("   FORCED -1   Cp %+.4f, residual sd %.1f Pa" % (forced[1], forcedResidual))
    print("   NO TERM     Cp %+.4f, residual sd %.1f Pa" % (plain[1], plainResidual))

    if 0.0 < attenuation < 1.0:
        print("\n   Read as attenuation: %.0f%% of the GPS altitude variance is real height, so"
              % (attenuation * 100))
        print("   sd(true) ~ %.1f m and sd(error) ~ %.1f m (%.0f Pa). Forcing -1 over-corrects"
              % (altitude.std() * np.sqrt(attenuation),
                 altitude.std() * np.sqrt(1 - attenuation),
                 altitude.std() * np.sqrt(1 - attenuation) * PA_PER_METRE))
        print("   this data by %.1fx." % (1.0 / attenuation))

    edges = [float(value) for value in args.bands.split(",")]
    corrected = pressure + head
    print("\n   band km/h      n   mean q   Cp forced-1   Cp raw    mean dh")
    for low, high in zip(edges, edges[1:]):
        inBand = (kmh >= low) & (kmh < high)
        if inBand.sum() < 20:
            continue
        meanQ = q[inBand].mean()
        if meanQ > 3:
            forcedCp = "%+.3f" % ((corrected[inBand].mean() - corrected[still].mean()) / meanQ)
            rawCp = "%+.3f" % ((pressure[inBand].mean() - pressure[still].mean()) / meanQ)
        else:
            forcedCp = rawCp = "--"
        print("   %4d-%4d %6d %8.1f %12s %8s %8.1f m"
              % (low, high, inBand.sum(), meanQ, forcedCp, rawCp, dh[inBand].mean()))
    print("   A band's mean dh times 12 Pa/m is what forcing -1 adds to it; against that band's q")
    print("   that is the Cp the correction manufactures.")

    print("\n   alignment sweep, because the cross-correlation aligns groupings not transients:")
    print("   shift   altitude coeff   Cp fitted   residual")
    for shift in (-args.shift, -args.shift / 2, 0.0, args.shift / 2, args.shift):
        rolled = int(round(shift))
        shiftedHead = rho * GRAVITY * (np.roll(altitude, rolled) - altitude[still].mean())
        shiftedQ = 0.5 * rho * np.roll(speed, rolled) ** 2
        shiftedBeta, _, shiftedResidual = fitBoth(pressure, shiftedHead, shiftedQ)
        print("   %+5d   %+12.3f   %+9.4f   %6.1f Pa"
              % (rolled, shiftedBeta[1], shiftedBeta[2], shiftedResidual))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
