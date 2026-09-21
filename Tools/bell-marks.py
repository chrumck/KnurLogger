"""Evaluate calibration-bell marks against one pressure channel of a KnurLogger session.

Usage:  CH=P1 python Tools/bell-marks.py <session.ndjson> <m_dry_g> <m_disp_g> HH:MM:SS@d_mm ...
        (CH defaults to P0; mark times are LOCAL, CEST, on the session's local date)

Method and constants: ../ndLouvers/pressure-testing.md 3.15 and 3.15d. Counters in a pressure
record are cumulative session totals (CLAUDE.md), so the session total is the LAST record's value
and per-cycle validity is each channel's valid flag. Averaging windows END at the mark (3.8a
item 2); a straight-line fit at the mark beats the trailing mean printed here by ~0.17 Pa at heavy
sag rates. Q is taken from the sink rate between marks; the tubing loss is Q x R_PATH.
"""
import json, sys, os, statistics as st
CH = os.environ.get('CH', 'P0')
from datetime import datetime, timezone, timedelta

path = sys.argv[1]
LOCAL = timedelta(hours=2)          # CEST; taiUs on this box equals unix UTC (checked vs isoTime)

# as-built constants, calibrationBell.md "As built" / pressure-testing.md 3.5
K_M = 0.5551      # Pa/g, rod fitted
K_D = 0.2372      # Pa/mm, rod fitted
A_O = 18146.0     # mm^2 outer, as used in 3.5b item 5
A_I = 17671.0     # mm^2 inner (A_eff)
R_PATH = 1.038e7  # Pa.s/m^3, 2x1.5 m + wands + 2 filters (3.8c item 8)

m_dry, m_disp = float(sys.argv[2]), float(sys.argv[3])
m_app = m_dry - m_disp
marks = [(s.split('@')[0], float(s.split('@')[1])) for s in sys.argv[4:]]   # "HH:MM:SS@d_mm" local

recs = []
counters_last = None
first = last = None
with open(path, encoding='utf-8') as f:
    for line in f:
        r = json.loads(line)
        if r['t'] == 'session':
            print('session', r['isoTime'], 'mode', r['mode'])
        if r['t'] != 'pressure':
            if r['t'] not in ('temp', 'enclosure', 'supply'):
                print('event', r['t'], datetime.fromtimestamp(r['taiUs']/1e6, timezone.utc).isoformat(), {k: v for k, v in r.items() if k in ('message', 'note', 'reason', 'event', 'notifications')})
            continue
        t = datetime.fromtimestamp(r['taiUs']/1e6, timezone.utc)
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
vm = {}
for t, r, ch in recs:
    vm[r['validMask']] = vm.get(r['validMask'], 0) + 1
print('validMask histogram', vm)
for p in ('P0', 'P1', 'P2', 'P3', 'P4'):
    inval = [(t+LOCAL).strftime('%H:%M:%S') for t, r, ch in recs if not ch[p]['valid']]
    vals = [ch[p]['pressurePa'] for t, r, ch in recs if ch[p]['valid']]
    temps = [ch[p]['sensorTemperatureC'] for t, r, ch in recs if ch[p]['valid']]
    print(f'{p}: invalid cycles {len(inval)} {inval[:6]}; min {min(vals):.3f} max {max(vals):.3f} mean {st.mean(vals):.3f}; T {min(temps):.1f}-{max(temps):.1f} C')

# 10 s bins of P0 for the trace
print('\n{CH} trace, 10 s bins (local):')
bins = {}
for t, r, ch in recs:
    if ch[CH]['valid']:
        k = int(t.timestamp() // 10) * 10
        bins.setdefault(k, []).append(ch[CH]['pressurePa'])
for k in sorted(bins):
    v = bins[k]
    tl = datetime.fromtimestamp(k, timezone.utc) + LOCAL
    print(f'{tl.strftime("%H:%M:%S")} n={len(v):3d} mean {st.mean(v):8.3f} sd {st.pstdev(v):.3f}')

print(f'\nm_dry {m_dry} g, m_disp {m_disp} g, m_app {m_app} g, k_m*m_app = {K_M*m_app:.2f} Pa')
print('mark(local)  d_mm  P_bell   reading30(sd)      reading60(sd)     f30      f60   Q*R_path(pred loss)')
prev = None
rows = []
for hms, d in marks:
    hh, mm, ss = map(int, hms.split(':'))
    lf = first + LOCAL
    tm = datetime(lf.year, lf.month, lf.day, hh, mm, ss, tzinfo=timezone.utc) - LOCAL
    def win(sec):
        v = [ch[CH]['pressurePa'] for t, r, ch in recs if ch[CH]['valid'] and tm - timedelta(seconds=sec) <= t <= tm]
        return (st.mean(v), st.pstdev(v), len(v)) if v else (float('nan'), float('nan'), 0)
    r30, r60 = win(30), win(60)
    pb = K_M*m_app - K_D*d
    f30 = 1 - r30[0]/pb
    f60 = 1 - r60[0]/pb
    rows.append((tm, d, pb, r30[0], f30))
    print(f'{hms}  {d:5.1f}  {pb:7.2f}  {r30[0]:8.3f} ({r30[1]:.3f},n{r30[2]})  {r60[0]:8.3f} ({r60[1]:.3f})  {f30*100:5.2f}%  {f60*100:5.2f}%')
    if prev:
        dt = (tm - prev[0]).total_seconds()
        rate = (d - prev[1]) / dt * 60          # mm/min
        for A, name in ((A_O, 'A_o'), (A_I, 'A_i')):
            Q = rate/60 * A * 1e-9              # m^3/s
            loss = Q * R_PATH
            pb_mid = (pb + prev[2]) / 2
            print(f'    interval sink {rate:.2f} mm/min -> Q({name}) {Q*1e6:.3f} mL/s, Q*R_path = {loss:.2f} Pa = {loss/pb_mid*100:.2f}% of P_bell; R_s = {(pb_mid-loss)/Q:.3e}')
    prev = (tm, d, pb)

# slope/intercept check across marks (expect disagreement if f varies; here P moves ~6%)
ds = [r[1] for r in rows]; rd = [r[3] for r in rows]
if len(rows) >= 3:
    xm, ym = st.mean(ds), st.mean(rd)
    slope = sum((x-xm)*(y-ym) for x, y in zip(ds, rd)) / sum((x-xm)**2 for x in ds)
    icpt = ym - slope*xm
    print(f'\nfit reading = {icpt:.3f} + {slope:.4f} d;  f_slope = {(1+slope/K_D)*100:.2f}%  f_intercept = {(1-icpt/(K_M*m_app))*100:.2f}%')
