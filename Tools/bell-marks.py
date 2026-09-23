"""Evaluate calibration-bell marks against one pressure channel of a KnurLogger session.

Usage:  CH=P1 R_PATH=2.01e6 python Tools/bell-marks.py <session.ndjson> <m_dry_g> <m_disp_g> MARK ...
        MARK is HH:MM:SS@d_mm, optionally followed by ~HH:MM:SS@d0_mm giving the un-pinch time and
        depth of the same charge, from which the sink rate and therefore Q are taken.
        Mark times are LOCAL, CEST, on the session's local date. CH defaults to P0.
        R_PATH defaults to the ladder's short unfiltered leg; set it for any other line.

Counters in a pressure record are cumulative session totals, so the session total is the LAST
record's value and per-cycle validity is each channel's valid flag.
"""
import json, sys, os, statistics as st
from datetime import datetime, timezone, timedelta

CH = os.environ.get('CH', 'P0')
R_PATH = float(os.environ.get('R_PATH', '2.01e6'))   # Pa.s/m^3; the ladder's short leg

path = sys.argv[1]
LOCAL = timedelta(hours=2)          # CEST; taiUs on this box equals unix UTC (checked vs isoTime)

# as built
K_M = 0.5639      # Pa/g, g/A_eff on the water-measured A_eff
K_D = 0.2414      # Pa/mm, rho.g.(A_wall + A_rod)/A_eff, rod fitted
A_O = 18146.0     # mm^2 outer; sink rate x A_o = flow
P_CAL = 96600.0   # Pa; the SDP810's calibration pressure, reading = dp x P_abs/P_CAL
FIT_S = 60        # s; straight-line fit over the window ending at the mark

m_dry, m_disp = float(sys.argv[2]), float(sys.argv[3])
m_app = m_dry - m_disp


def parse_point(s):
    hms, d = s.split('@')
    return hms, float(d)


marks = []
for arg in sys.argv[4:]:
    mark, _, start = arg.partition('~')
    marks.append((parse_point(mark), parse_point(start) if start else None))

recs = []
baro = []
counters_last = None
first = last = None
with open(path, encoding='utf-8') as f:
    for line in f:
        r = json.loads(line)
        t = datetime.fromtimestamp(r['taiUs']/1e6, timezone.utc) if 'taiUs' in r else None
        if r['t'] == 'session':
            print('session', r['isoTime'], 'mode', r['mode'])
        if r['t'] == 'enclosure':
            if r.get('pressureValid'):
                baro.append((t, r['enclosurePressurePa']))
            continue
        if r['t'] != 'pressure':
            if r['t'] not in ('temp', 'supply'):
                print('event', r['t'], t.isoformat() if t else '', {k: v for k, v in r.items() if k in ('message', 'note', 'reason', 'event', 'notifications')})
            continue
        ch = {c['ch']: c for c in r['channels']}
        recs.append((t, r, ch))
        counters_last = r
        first = first or t
        last = t

n = len(recs)
dur = (last - first).total_seconds()
print(f'\npressure cycles {n}, {dur/60:.1f} min, {n/dur:.3f} Hz, local {(first+LOCAL).strftime("%H:%M:%S")}-{(last+LOCAL).strftime("%H:%M:%S")}')
c = counters_last
print('session totals (last record): readErrors', c['readErrors'], 'crcFailures', c['crcFailures'],
      'i2cFirstAttemptFailures', c['i2cFirstAttemptFailures'], 'i2cRecovered', c['i2cRecovered'], 'i2cExhausted', c['i2cExhausted'])
if baro:
    print(f'P_abs (BME280) {min(p for _, p in baro)/100:.1f}-{max(p for _, p in baro)/100:.1f} mbar over the session')
else:
    print('P_abs: NO valid enclosure pressure in this session - the barometric factor cannot be applied')
vm = {}
for t, r, ch in recs:
    vm[r['validMask']] = vm.get(r['validMask'], 0) + 1
print('validMask histogram', vm)
for p in ('P0', 'P1', 'P2', 'P3', 'P4'):
    inval = [(t+LOCAL).strftime('%H:%M:%S') for t, r, ch in recs if not ch[p]['valid']]
    vals = [ch[p]['pressurePa'] for t, r, ch in recs if ch[p]['valid']]
    temps = [ch[p]['sensorTemperatureC'] for t, r, ch in recs if ch[p]['valid']]
    if vals:
        print(f'{p}: invalid cycles {len(inval)} {inval[:6]}; min {min(vals):.3f} max {max(vals):.3f} mean {st.mean(vals):.3f}; T {min(temps):.1f}-{max(temps):.1f} C')
    else:
        print(f'{p}: no valid cycles')

print(f'\n{CH} trace, 10 s bins (local):')
bins = {}
for t, r, ch in recs:
    if ch[CH]['valid']:
        k = int(t.timestamp() // 10) * 10
        bins.setdefault(k, []).append(ch[CH]['pressurePa'])
for k in sorted(bins):
    v = bins[k]
    tl = datetime.fromtimestamp(k, timezone.utc) + LOCAL
    print(f'{tl.strftime("%H:%M:%S")} n={len(v):3d} mean {st.mean(v):8.3f} sd {st.pstdev(v):.3f}')


def to_utc(hms):
    hh, mm, ss = map(int, hms.split(':'))
    lf = first + LOCAL
    return datetime(lf.year, lf.month, lf.day, hh, mm, ss, tzinfo=timezone.utc) - LOCAL


def fit_at(tm, sec):
    """Straight-line fit of the channel over the window ending at tm, evaluated at tm."""
    pts = [((t - tm).total_seconds(), ch[CH]['pressurePa']) for t, r, ch in recs
           if ch[CH]['valid'] and tm - timedelta(seconds=sec) <= t <= tm]
    if len(pts) < 3:
        return float('nan'), float('nan'), len(pts)
    xm, ym = st.mean(x for x, _ in pts), st.mean(y for _, y in pts)
    slope = sum((x-xm)*(y-ym) for x, y in pts) / sum((x-xm)**2 for x, _ in pts)
    resid = st.pstdev(y - (ym + slope*(x-xm)) for x, y in pts)
    return ym - slope*xm, resid, len(pts)


def p_abs_at(tm):
    near = [p for t, p in baro if abs((t - tm).total_seconds()) <= FIT_S]
    return st.mean(near) if near else float('nan')


print(f'\nm_dry {m_dry} g, m_disp {m_disp} g, m_app {m_app} g, k_m*m_app = {K_M*m_app:.2f} Pa; '
      f'k_d {K_D}, R_path {R_PATH:.3g} Pa.s/m^3')
print('mark(local)  d_mm  P_bell   fit60 (resid,n)       P_abs    Q mL/s  loss Pa  f=1-rdg/P_bell  eps')
prev = None
for (hms, d), start in marks:
    tm = to_utc(hms)
    reading, resid, npts = fit_at(tm, FIT_S)
    pb = K_M*m_app - K_D*d
    pa = p_abs_at(tm)
    baro_factor = pa / P_CAL
    # Q from this charge's own un-pinch point when given, else from the previous mark
    if start:
        t0, d0 = to_utc(start[0]), start[1]
    elif prev:
        t0, d0 = prev
    else:
        t0 = None
    if t0 is not None and (tm - t0).total_seconds() > 0:
        rate = (d - d0) / (tm - t0).total_seconds()        # mm/s
        q = rate * A_O * 1e-9                              # m^3/s
    else:
        q = float('nan')
    loss = q * R_PATH
    eps = reading / (baro_factor * (pb - loss)) - 1
    print(f'{hms}  {d:5.1f}  {pb:7.2f}  {reading:8.3f} ({resid:.3f},n{npts})  {pa/100:7.1f}  {q*1e6:6.3f}  {loss:6.2f}  '
          f'{(1 - reading/pb)*100:6.2f}%        {eps*100:+6.2f}%')
    prev = (tm, d)
