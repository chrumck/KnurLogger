#!/usr/bin/env python3
"""Audit a RaceChrono .rcz export's channel definitions against what the logger sends.

RaceChrono stores each channel's samples in a file named after the channel's numeric id,
and that id encodes the predefined channel the user picked in the app:

    id = slot * 2**20 + channelType

So an export names every channel slot in force when it was recorded, which makes the
phone's channel list auditable without the phone. The values then say whether each
equation was typed correctly: a thermal channel reading +327.68 instead of -327.68 is
the -32768 invalid sentinel decoded unsigned, which is the one fault that stays
invisible once probes are attached and reading above zero.

Only the four channel types box 2 uses are named here. Box 1's channels come back as
bare type numbers, which is enough to tell them apart.

A channel with no value in any sample is reported but NOT called a fault: RaceChrono
renders a deliberate out-of-calibration-range marker as no value, so an all-empty
channel can be a sensor that was out of range for the whole session. Box 1 has two that
are out of range while the car is cold and read normally once it is warm.

A session that was paused and resumed stores each stretch in its own fragment: the first
at the archive root, the rest under "resume_<n>/". Fragments are reported separately,
because a channel edited between them legitimately differs, and because reading only the
root silently discards every later stretch.
"""

import struct
import sys
import zipfile
from collections import defaultdict

TYPE_BASES = {70537: "Digital", 70539: "Temperature", 70541: "Pressure", 70547: "Percent"}
UNSIGNED_SENTINEL = 327.68
SENTINEL_TOLERANCE = 0.01


def decodeChannelId(channelId):
    slot, channelType = divmod(channelId, 1 << 20)
    name = TYPE_BASES.get(channelType)
    if name is None:
        return "type %d" % channelType, slot, False
    return name, slot, True


def readFragments(path):
    fragments = defaultdict(lambda: defaultdict(dict))
    with zipfile.ZipFile(path) as archive:
        for name in archive.namelist():
            fragment, _, leaf = name.rpartition("/")
            parts = leaf.split("_")
            if not leaf.startswith("channel2_") or len(parts) != 6:
                continue
            deviceId, channelId = int(parts[2]), int(parts[3])
            raw = archive.read(name)
            fragments[fragment][deviceId][channelId] = struct.unpack("<%dd" % (len(raw) // 8), raw)
    return fragments


def main(paths):
    for path in paths:
        print("== %s" % path)
        fragments = readFragments(path)
        for fragment, byDevice in sorted(fragments.items()):
            if len(fragments) > 1:
                print("~~ fragment %s" % (fragment or "(root)"))
            reportFragment(byDevice)
    return 0


def reportFragment(byDevice):
    for deviceId, byChannel in sorted(byDevice.items()):
        print("-- device %d, %d channels" % (deviceId, len(byChannel)))
        rows = sorted(
            (decodeChannelId(cid) + (cid, values) for cid, values in byChannel.items()),
            key=lambda row: (row[0], row[1]),
        )
        for channelType, slot, isKnown, channelId, values in rows:
            finite = [v for v in values if v == v]
            span = "%.6g .. %.6g" % (min(finite), max(finite)) if finite else "no values"
            label = "%s Front %d" % (channelType, slot) if isKnown else "%s slot %d" % (channelType, slot)
            note = ""
            if not finite:
                note = "  <-- no value in ANY sample: out of range all session, or a bad equation"
            elif channelType == "Temperature" and any(
                abs(v - UNSIGNED_SENTINEL) < SENTINEL_TOLERANCE for v in finite
            ):
                note = "  <-- UNSIGNED: reads +327.68, so bytesToUint where bytesToInt belongs"
            print("   %-24s id=%-9d n=%-5d %s%s" % (label, channelId, len(values), span, note))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: rcz-channels.py <session.rcz> [...]")
    sys.exit(main(sys.argv[1:]))
