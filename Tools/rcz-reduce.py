#!/usr/bin/env python3
"""Reduce a RaceChrono .rcz to the per-session quantities a driven session is read from.

`rcz-channels.py` answers "are the channel definitions right". This answers "what did the session
measure, and which parts of it may be quoted". It prints an inventory, an instrument-health block,
a per-channel validity block, pressure ratios and slopes against dynamic pressure, a per-lap
thermal table, a cavity-pressure fit, and a standstill block -- in that order, because each one
qualifies the one after it.

A session that was paused and resumed stores each stretch in its own fragment: the first at the
archive root, the rest under "resume_<n>/". Each is reported separately and is a separate run on
track. Reading only the root silently discards every later stretch.

CHANNEL NAMES HERE ARE POSITIONAL AND MEAN NOTHING. P0-P5 are fixed by mux position and temp0-temp3
by enrollment order; which measurement role each one serves is a decision of the project that
commissioned the session, not a property of the logger. Pass --roles to label them in the output
and --ambient to say which thermal channel carries the air temperature the density term needs. The
map in force is printed in the header, because a report that does not state its map cannot be read
back later -- nothing in a .rcz records it, nor the plate, body or mounting state of the car.

WHAT THE REJECTION RULES ARE FOR, because the defaults are not conservative by accident:

1. The -32768 invalid marker is not the whole of a channel's bad data. A differential sensor whose
   output has saturated keeps sending valid-flagged samples that are compressed against the limit,
   and only the ones past it carry the marker. So a channel that sent ANY marker is assumed to have
   reached its limit, and --clip-guard then rejects every one-second window whose peak sits within
   that fraction of the channel's largest valid reading. Rejecting on the marker alone leaves the
   channel biased low at exactly the speeds that saturated it.
2. Ratios are formed from one-second means and a whole window is dropped if any sample in it is
   invalid, rather than averaging around the hole. Dynamic pressure goes as speed squared, so a
   window straddling a rejection is not a mean of anything.
3. The BME280 publishes its pressure, temperature and humidity invalid together when a measurement
   is skipped, and its absolute pressure then decodes to about 4.29e6 kPa. One such cycle moves a
   session mean by thousands of bar, so it is dropped before any density term is formed.

The cavity fit carries an elevation term beside the dynamic one. On a circuit flat enough it
changes nothing; on anything else it is worth more than the signal, and its fitted coefficient is
the diagnostic -- it has a physical value of -1, so a fit that returns something else has not
separated the two terms and its Cp means nothing. GPS altitude is the only height available and it
is noisy, which is exactly why the coefficient is printed rather than assumed.
"""

import argparse
import json
import sys
import zipfile
from collections import defaultdict

import numpy as np

GPS_DEVICE = 100
PRESSURE_CHANNELS = {1119117: "P0", 2167693: "P1", 3216269: "P2", 4264845: "P3", 5313421: "P4"}
THERMAL_CHANNELS = {1119115: "temp0", 2167691: "temp1", 3216267: "temp2", 4264843: "temp3"}
CAVITY_CHANNELS = {52499341: "pAbs", 52499339: "cavityT", 52499347: "cavityRh"}
COUNTER_CHANNELS = {22090633: "pressureCycles", 4264841: "thermalCycles", 56693641: "cavityCycles"}
HEALTH_CHANNELS = {
    24187785: "pressureEnabled", 25236361: "pressureSent", 26284937: "pressureReadErrors",
    27333513: "pressureCrcFailures", 28382089: "pressureMux",
    1119113: "probesEnumerated", 2167689: "probesValid", 3216265: "thermalReadErrors",
    53547913: "cavityStatus", 54596489: "cavityChipId", 55645065: "cavityReadErrors",
    11604873: "liveThrottle", 12653449: "stickyThrottle", 13702025: "undervoltage",
}
HEARTBEAT_CHANNEL = 16847753


def baseClock(series):
    """The densest known channel's timestamps, which everything else is held forward onto.

    Not simply a pressure channel: recordings made before the pressure channels were defined
    carry the thermal ones at a tenth the rate and reduce perfectly well on those instead. Taking
    the densest available keeps one rule for both and never silently returns nothing.
    """
    known = set(PRESSURE_CHANNELS) | set(THERMAL_CHANNELS) | set(CAVITY_CHANNELS)
    present = [series[c] for c in known if c in series]
    return max(present, key=lambda pair: len(pair[0])) if present else None

# Published as decipascals and divided by ten thousand on the phone, so kPa arrive and the marker
# lands here. Anything below it is the marker and not a reading: the parts span half a kilopascal.
PRESSURE_SENTINEL_KPA = -3.0
THERMAL_SENTINEL_C = -300.0
CAVITY_SENTINEL_KPA = 1.0e5

GRAVITY = 9.80665
DRY_AIR_R = 287.058
DEFAULT_BANDS = (30, 60, 90, 120, 150, 170, 200)


def readArchive(path):
    """Return {fragment: {'pressure'/'thermal'/..: {name: (timesMs, values)}, 'gps': {...}}}."""
    archive = zipfile.ZipFile(path)
    values, times, gps = defaultdict(dict), defaultdict(dict), defaultdict(dict)
    for name in archive.namelist():
        fragment, _, leaf = name.rpartition("/")
        parts = leaf.split("_")
        if leaf.startswith("channel_1_%d_0_" % GPS_DEVICE):
            gps[fragment][leaf.split("_", 4)[4]] = archive.read(name)
        elif leaf.startswith("channel2_") and len(parts) == 6:
            values[fragment][(int(parts[2]), int(parts[3]))] = np.frombuffer(archive.read(name), "<f8")
        elif leaf.startswith("channel_") and len(parts) == 6 and parts[4:] == ["1", "1"]:
            times[fragment][(int(parts[2]), int(parts[3]))] = np.frombuffer(archive.read(name), "<i8")

    fragments = {}
    for fragment in set(values) | set(gps):
        series, byDevice = {}, {}
        for key, value in values.get(fragment, {}).items():
            stamp = times.get(fragment, {}).get(key)
            if stamp is None:
                continue
            count = min(len(value), len(stamp))
            byDevice[key] = (stamp[:count], value[:count])
            # Keyed on the channel alone, because a device's number is the order the phone was
            # told about it and changes if the devices are re-added; the channel ids do not.
            series[key[1]] = byDevice[key]
        fragments[fragment] = {"series": series, "byDevice": byDevice, "gps": decodeGps(gps.get(fragment, {}))}
    return fragments


def decodeGps(files):
    if "1_1" not in files:
        return {}
    position = np.frombuffer(files["3_1"], "<i4")
    return {
        "t": np.frombuffer(files["1_1"], "<i8"),
        "distanceM": np.frombuffer(files["2_1"], "<i8") / 1000.0,
        # RaceChrono stores coordinates in sixths of a microdegree, not in degrees times 1e7.
        "latitude": position[0::2] / 6.0e6,
        "longitude": position[1::2] / 6.0e6,
        "speedMs": np.frombuffer(files["4_0"], "<i4") / 1000.0,
        "altitudeM": np.frombuffer(files["5_0"], "<i4") / 1000.0,
    }


def orderFragments(fragments):
    def rank(name):
        return 0 if not name else int(name.rstrip("/").split("_")[1]) + 1
    return sorted(fragments, key=rank)


def readLaps(path):
    """Complete laps per fragment index, from the archive's own lap list."""
    with zipfile.ZipFile(path) as archive:
        if "session.json" not in archive.namelist():
            return {}
        session = json.loads(archive.read("session.json").decode("utf-8"))
    laps = defaultdict(list)
    for lap in session.get("laps", []):
        if "finishTimestamp" in lap and not lap.get("isInvalid"):
            laps[lap.get("sessionResume", 0)].append((lap["startTimestamp"], lap["finishTimestamp"]))
    return laps


def holdResample(sourceTimes, sourceValues, targetTimes):
    """Each target takes the last source sample at or before it; NaN before the first."""
    index = np.searchsorted(sourceTimes, targetTimes, side="right") - 1
    out = np.full(len(targetTimes), np.nan)
    known = index >= 0
    out[known] = sourceValues[index[known]]
    return out


def buildFrame(fragment, ambientChannel, extra):
    """One table on the densest channel's clock: everything else is held forward onto it."""
    series, gps = fragment["series"], fragment["gps"]
    clock = baseClock(series)
    if clock is None or not gps:
        return None
    base = clock[0]
    frame = {"t": base}
    for channel, name in PRESSURE_CHANNELS.items():
        if channel in series:
            value = holdResample(*series[channel], base)
            frame[name] = np.where(value < PRESSURE_SENTINEL_KPA, np.nan, value * 1000.0)
    for channel, name in THERMAL_CHANNELS.items():
        if channel in series:
            value = holdResample(*series[channel], base)
            frame[name] = np.where(value < THERMAL_SENTINEL_C, np.nan, value)
    for channel, name in CAVITY_CHANNELS.items():
        if channel in series:
            value = holdResample(*series[channel], base)
            frame[name] = np.where(np.abs(value) > CAVITY_SENTINEL_KPA, np.nan, value)
    if "pAbs" in frame:
        frame["pAbs"] *= 1000.0
    if 25236361 in series:
        frame["pressureSent"] = holdResample(*series[25236361], base)

    for key, name in extra.items():
        if key in fragment["byDevice"]:
            frame[name] = holdResample(*fragment["byDevice"][key], base)

    frame["speedMs"] = holdResample(gps["t"], gps["speedMs"], base)
    frame["altitudeM"] = holdResample(gps["t"], gps["altitudeM"], base)
    ambient = frame.get(ambientChannel)
    if ambient is not None and "pAbs" in frame:
        frame["density"] = frame["pAbs"] / (DRY_AIR_R * (ambient + 273.15))
        frame["q"] = 0.5 * frame["density"] * frame["speedMs"] ** 2
    return frame


def secondWindows(frame, mask=None):
    """Collapse to one-second windows, carrying each window's peak and worst validity."""
    times = frame["t"]
    keep = np.ones(len(times), bool) if mask is None else mask
    if keep.sum() < 2:
        return None
    second = (times[keep] - times[keep][0]) // 1000
    _, start = np.unique(second, return_index=True)

    windows = {}
    for name, value in frame.items():
        if name == "t":
            continue
        selected = value[keep]
        finite = np.add.reduceat(np.isfinite(selected).astype(float), start)
        total = np.add.reduceat(np.nan_to_num(selected), start)
        windows[name] = np.where(finite > 0, total / np.maximum(finite, 1), np.nan)
        windows["_complete_" + name] = finite == np.diff(np.append(start, len(selected)))
        if name in PRESSURE_CHANNELS.values():
            windows["_peak_" + name] = np.maximum.reduceat(np.nan_to_num(selected), start)
    return windows


def clipGuards(fragments, order, guard):
    """A channel that ever sent the marker reached its limit, so its top readings are compressed."""
    guards, evidence = {}, {}
    for name in PRESSURE_CHANNELS.values():
        markers, peak = 0, -np.inf
        for key in order:
            series = fragments[key]["series"]
            channel = next((c for c, n in PRESSURE_CHANNELS.items() if n == name), None)
            if channel not in series:
                continue
            value = series[channel][1]
            markers += int((value < PRESSURE_SENTINEL_KPA).sum())
            valid = value[value >= PRESSURE_SENTINEL_KPA]
            if len(valid):
                peak = max(peak, valid.max() * 1000.0)
        evidence[name] = (markers, peak)
        guards[name] = guard * peak if markers and np.isfinite(peak) else np.inf
    return guards, evidence


def usableMask(windows, channels, guards, bandLow, bandHigh):
    speed = windows["speedMs"] * 3.6
    mask = (speed >= bandLow) & (speed < bandHigh) & np.isfinite(windows.get("q", windows["speedMs"]))
    for name in channels:
        if name not in windows:
            return np.zeros(len(speed), bool)
        mask &= windows["_complete_" + name] & (windows["_peak_" + name] < guards[name])
    if "pressureSent" in windows:
        mask &= windows["pressureSent"] == np.nanmax(windows["pressureSent"])
    return mask


def label(name, roles):
    return "%s (%s)" % (name, roles[name]) if name in roles else name


def reportInventory(fragments, order, laps):
    print("== sessions")
    print("%-4s %-21s %8s %9s %6s %9s %9s" % ("#", "start, UTC", "minutes", "km", "laps", "best lap", "peak km/h"))
    for index, key in enumerate(order):
        gps = fragments[key]["gps"]
        if not gps:
            continue
        times = [(b - a) / 1000.0 for a, b in laps.get(index, [])]
        print("%-4d %-21s %8.1f %9.2f %6d %9s %9.1f" % (
            index + 1, np.datetime64(int(gps["t"][0]), "ms"), (gps["t"][-1] - gps["t"][0]) / 60000.0,
            gps["distanceM"][-1] / 1000.0, len(times),
            "%.3f" % min(times) if times else "-", gps["speedMs"].max() * 3.6))


def reportHealth(fragments, order):
    print("\n== instrument health")
    print("   A counter that skips lost a notification; one that freezes lost the link.")
    print("%-4s %7s %9s %10s %10s %9s %9s %9s" % (
        "#", "n", "Hz", "lost", "max gap", "sent", "readErr", "sticky"))
    for index, key in enumerate(order):
        series = fragments[key]["series"]
        base = baseClock(series)
        if base is None:
            continue
        times = base[0]
        span = (times[-1] - times[0]) / 1000.0
        lost = []
        for channel, _ in COUNTER_CHANNELS.items():
            if channel in series:
                lost.append(countSkips(series[channel][1], 65536))
        if HEARTBEAT_CHANNEL in series:
            lost.append(countSkips(series[HEARTBEAT_CHANNEL][1], 256))
        sent = series.get(25236361)
        errors = [series[c][1].max() for c in (26284937, 27333513, 3216265, 55645065) if c in series]
        sticky = series.get(12653449)
        print("%-4d %7d %9.3f %10s %8d ms %9s %9d %9s" % (
            index + 1, len(times), (len(times) - 1) / span if span else 0,
            "%d/%d" % (max(lost), len(times)) if lost else "-",
            np.diff(times).max(),
            "%g..%g" % (sent[1].min(), sent[1].max()) if sent is not None else "-",
            max(errors) if errors else 0,
            "%g..%g" % (sticky[1].min(), sticky[1].max()) if sticky is not None else "-"))

    for index, key in enumerate(order):
        series = fragments[key]["series"]
        notes = []
        for channel, name in list(HEALTH_CHANNELS.items()):
            if channel in series and name in ("cavityStatus", "probesEnumerated", "probesValid", "pressureEnabled"):
                value = series[channel][1]
                if value.min() != value.max():
                    notes.append("%s fell to %g" % (name, value.min()))
        for channel, name in CAVITY_CHANNELS.items():
            if channel in series:
                invalid = int((np.abs(series[channel][1]) > CAVITY_SENTINEL_KPA).sum())
                if invalid:
                    notes.append("%s invalid on %d cycle(s)" % (name, invalid))
        if notes:
            print("   session %d: %s" % (index + 1, "; ".join(sorted(set(notes)))))


def countSkips(counter, modulus):
    step = np.diff(counter.astype(np.int64))
    step = np.where(step < -modulus // 2, step + modulus, step)
    return int(np.clip(step - 1, 0, None).sum())


def reportChannels(fragments, order, guards, evidence, roles, guard):
    print("\n== pressure channels, Pa")
    for name, (markers, peak) in sorted(evidence.items()):
        if not np.isfinite(peak):
            continue
        state = "limit reached: rejecting peaks above %.1f Pa (%.0f%% of %.1f)" % (
            guards[name], 100 * guard, peak) if markers else "no marker sent"
        print("   %-12s largest valid %8.1f   markers %5d   %s" % (label(name, roles), peak, markers, state))
    print("%-4s " % "#" + " ".join("%22s" % label(n, roles) for n in sorted(PRESSURE_CHANNELS.values())))
    for index, key in enumerate(order):
        series = fragments[key]["series"]
        cells = []
        for name in sorted(PRESSURE_CHANNELS.values()):
            channel = next(c for c, n in PRESSURE_CHANNELS.items() if n == name)
            if channel not in series:
                cells.append("%22s" % "-")
                continue
            value = series[channel][1] * 1000.0
            valid = value[value >= PRESSURE_SENTINEL_KPA * 1000.0]
            cells.append("%22s" % ("%.1f..%.1f" % (valid.min(), valid.max()) if len(valid) else "none"))
        print("%-4d " % (index + 1) + " ".join(cells))


def reportRatios(frames, order, guards, roles, bands):
    channels = [n for n in sorted(PRESSURE_CHANNELS.values())]
    print("\n== pressure over dynamic pressure, one-second windows")
    print("   Medians. A band is omitted where fewer than five windows survive the rejection rules.")
    print("%-4s %-10s %7s " % ("#", "km/h", "n") + " ".join("%12s" % label(n, roles) for n in channels))
    for index, key in enumerate(order):
        windows = frames.get(key)
        if windows is None or "q" not in windows:
            continue
        for low, high in zip(bands, bands[1:]):
            mask = usableMask(windows, channels, guards, low, high) & (windows["q"] > 50)
            if mask.sum() < 5:
                continue
            cells = ["%12.4f" % np.median(windows[n][mask] / windows["q"][mask]) for n in channels]
            print("%-4d %-10s %7d " % (index + 1, "%d-%d" % (low, high), mask.sum()) + " ".join(cells))
        print()


def reportSlopes(frames, order, guards, roles, low, high):
    """A fit, because an offset that does not scale with speed is not visible in a ratio."""
    channels = [n for n in sorted(PRESSURE_CHANNELS.values())]
    print("== fit P = a*q + b over %d-%d km/h" % (low, high))
    print("   b is the part of the reading that does not come from road speed.")
    print("%-4s %7s " % ("#", "n") + " ".join("%20s" % label(n, roles) for n in channels))
    for index, key in enumerate(order):
        windows = frames.get(key)
        if windows is None or "q" not in windows:
            continue
        mask = usableMask(windows, channels, guards, low, high) & (windows["q"] > 50)
        if mask.sum() < 20:
            continue
        cells = []
        for name in channels:
            slope, offset = np.polyfit(windows["q"][mask], windows[name][mask], 1)
            cells.append("%20s" % ("a %.4f b %+.1f" % (slope, offset)))
        print("%-4d %7d " % (index + 1, mask.sum()) + " ".join(cells))


def reportThermal(fragments, order, frames, laps, ambientChannel, roles, extra):
    print("\n== per complete lap, degrees C (bracketed against %s)" % ambientChannel)
    present = [n for n in sorted(THERMAL_CHANNELS.values())] + sorted(extra.values())
    print("%-4s %5s %8s %8s " % ("#", "lap", "seconds", "km/h")
          + " ".join("%14s" % label(n, roles) for n in present))
    for index, key in enumerate(order):
        frame = frames.get("_frame_" + key)
        if frame is None:
            continue
        for number, (start, finish) in enumerate(laps.get(index, []), 1):
            span = (frame["t"] >= start) & (frame["t"] < finish)
            if span.sum() < 10:
                continue
            cells = []
            for name in present:
                if name not in frame:
                    cells.append("%14s" % "-")
                    continue
                value = np.nanmean(frame[name][span])
                if name in THERMAL_CHANNELS.values() and name != ambientChannel and ambientChannel in frame:
                    cells.append("%14s" % ("%.2f (%+.2f)" % (value, value - np.nanmean(frame[ambientChannel][span]))))
                else:
                    cells.append("%14.2f" % value)
            print("%-4d %5d %8.1f %8.0f " % (
                index + 1, number, (finish - start) / 1000.0, np.nanmean(frame["speedMs"][span]) * 3.6)
                + " ".join(cells))
        print()


def reportCavity(frames, order):
    print("== cavity pressure against dynamic pressure and height")
    print("   The height coefficient has a physical value of -1. Away from it the two terms are")
    print("   not separated and the Cp beside it means nothing.")
    print("%-4s %7s %12s %12s %10s %10s" % ("#", "n", "Cp, q only", "Cp with h", "h coeff", "resid sd"))
    for index, key in enumerate(order):
        windows = frames.get(key)
        if windows is None or "q" not in windows or "pAbs" not in windows:
            continue
        mask = np.isfinite(windows["pAbs"]) & np.isfinite(windows["q"]) & np.isfinite(windows["altitudeM"])
        if mask.sum() < 60:
            continue
        q, pressure = windows["q"][mask], windows["pAbs"][mask]
        height = windows["density"][mask] * GRAVITY * windows["altitudeM"][mask]
        flat = np.linalg.lstsq(np.column_stack([q, np.ones(len(q))]), pressure, rcond=None)[0]
        design = np.column_stack([q, height, np.ones(len(q))])
        full = np.linalg.lstsq(design, pressure, rcond=None)[0]
        print("%-4d %7d %12.4f %12.4f %10.3f %9.1f" % (
            index + 1, mask.sum(), flat[0], full[0], full[1], (pressure - design @ full).std()))


def reportStandstill(frames, order, roles):
    """A differential that is not zero on a stopped car is being driven by something on the car."""
    channels = [n for n in sorted(PRESSURE_CHANNELS.values())]
    print("\n== medians below 2 km/h, Pa")
    print("%-4s %7s " % ("#", "n") + " ".join("%12s" % label(n, roles) for n in channels))
    for index, key in enumerate(order):
        windows = frames.get(key)
        if windows is None:
            continue
        mask = (windows["speedMs"] * 3.6 < 2) & np.isfinite(windows["speedMs"])
        if mask.sum() < 10:
            continue
        cells = ["%12.2f" % np.nanmedian(windows[n][mask]) if n in windows else "%12s" % "-" for n in channels]
        print("%-4d %7d " % (index + 1, mask.sum()) + " ".join(cells))


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("archive")
    parser.add_argument("--roles", default="",
                        help="label channels in the output, e.g. P0=U,P1=C,P3=D,P4=X,temp0=ambient")
    parser.add_argument("--ambient", default="temp0",
                        help="thermal channel carrying air temperature, for the density term")
    parser.add_argument("--extra", default="",
                        help="further channels for the per-lap table, as device:channel=label, e.g. "
                             "200:10026=coolant -- the archive carries no meaning for these, so "
                             "whatever is named here is the caller's decoding and not the logger's")
    parser.add_argument("--clip-guard", type=float, default=0.92,
                        help="reject windows peaking above this fraction of a limited channel's "
                             "largest valid reading (default 0.92); 1.0 rejects on the marker alone")
    parser.add_argument("--slope-band", default="40,170", help="km/h range for the fit (default 40,170)")
    parser.add_argument("--bands", default=",".join(str(b) for b in DEFAULT_BANDS))
    args = parser.parse_args(argv)

    roles = dict(pair.split("=", 1) for pair in args.roles.split(",") if "=" in pair)
    extra = {}
    for pair in args.extra.split(","):
        if "=" not in pair:
            continue
        source, name = pair.split("=", 1)
        device, _, channel = source.partition(":")
        extra[(int(device), int(channel))] = name
    bands = tuple(int(b) for b in args.bands.split(","))
    slopeLow, slopeHigh = (int(b) for b in args.slope_band.split(","))

    fragments = readArchive(args.archive)
    order = orderFragments(fragments)
    laps = readLaps(args.archive)
    if not order:
        sys.exit("no channel data in %s" % args.archive)

    print("== %s" % args.archive)
    print("   %d session(s); density from %s; roles %s" % (
        len(order), args.ambient, roles if roles else "not given, channels are positional"))
    print("   Nothing in a .rcz records the car's configuration or where the logger sat. If this")
    print("   report is kept, keep the session record that states them beside it.")

    guards, evidence = clipGuards(fragments, order, args.clip_guard)
    frames = {}
    for key in order:
        frame = buildFrame(fragments[key], args.ambient, extra)
        if frame is None:
            continue
        frames["_frame_" + key] = frame
        frames[key] = secondWindows(frame)

    # Without air temperature there is no density and so no q, and every block that divides by it
    # would print an empty table rather than an error. Say so instead.
    if not any("q" in frames[key] for key in order if key in frames):
        print("\n   !! no %s in this archive, so there is no density term: every quantity formed"
              " against\n      dynamic pressure is omitted below. Name the right channel with"
              " --ambient." % args.ambient)

    reportInventory(fragments, order, laps)
    reportHealth(fragments, order)
    reportChannels(fragments, order, guards, evidence, roles, args.clip_guard)
    reportRatios(frames, order, guards, roles, bands)
    reportSlopes(frames, order, guards, roles, slopeLow, slopeHigh)
    reportThermal(fragments, order, frames, laps, args.ambient, roles, extra)
    reportCavity(frames, order)
    reportStandstill(frames, order, roles)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
