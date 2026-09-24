"""Fit the SDP810 span term eps across the bench calibration ladder.

Usage:  python Tools/ladder-fit.py [sessions_dir]      (default ../KnurLoggerData/sessions)

Every mark in force is re-derived from its session file with bell-marks.py's formulas, then fitted
by weighted least squares under five models: a constant per part; plus a rung offset common to all
parts; plus a sign split per part; a slope per part; and a constant plus sign split. The noise model
comes from genuine repeat pairs (same rung, channel and sign), and every uncertainty is inflated by
sqrt(chi2/dof) because marks scatter more across sessions than same-session pairs show.

MARKS lists only the marks in force: where the owner re-ran a channel, the re-run replaced the first
run and only the re-run is here. Mark times are LOCAL, CEST.
"""
import json, math, os, statistics as st, sys
from datetime import datetime, timezone, timedelta
import numpy as np

SESS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', '..', 'KnurLoggerData', 'sessions')
LOCAL = timedelta(hours=2)
K_M, K_D, A_O, P_CAL, R_SHORT = 0.5639, 0.2414, 18146.0, 96600.0, 2.01e6   # as-built bell; same as bell-marks.py
MASS = {1: (104.5, 5.7), 2: (190.5, 16.5), 3: (296.2, 51.5), 4: (386.2, 39.5), 5: (530.3, 59.2), 6: (823.9, 95.4)}
# mL/s from the run sheet's sink estimates, for rungs without a 65/70 mm pair
Q_ASSUMED = {1: 0.288, 2: 0.484, 3: 0.642, 4: 0.877, 6: 1.512}
CH = ['P0', 'P1', 'P2', 'P3', 'P4']

# (session id, rung, [(channel+sign, mark)]) at d = 70 mm with an assumed Q
MARKS = [
    ('2026-09-22T15-30-24.581464Z', 1, [('P0H', '17:32:35'), ('P0L', '17:33:40'), ('P1H', '17:35:30'), ('P1L', '17:36:20'),
      ('P2H', '17:37:30'), ('P2L', '17:39:02'), ('P3H', '17:40:00'), ('P3L', '17:41:00'), ('P4H', '17:42:30'), ('P4L', '17:43:30')]),
    ('2026-09-22T16-05-49.010417Z', 1, [('P3H', '18:08:10'), ('P3L', '18:10:00'), ('P4H', '18:11:30'), ('P4L', '18:12:30')]),
    ('2026-09-23T15-00-08.798505Z', 2, [('P2H', '17:25:40'), ('P2L', '17:26:48'), ('P3H', '17:28:46'), ('P3L', '17:29:50'),
      ('P4H', '17:31:45'), ('P4L', '17:34:04')]),
    ('2026-09-23T15-54-40.942849Z', 2, [('P0H', '18:00:24'), ('P0L', '18:03:02'), ('P1H', '18:05:15'), ('P1L', '18:07:35')]),
    ('2026-09-22T17-56-42.064714Z', 3, [('P0H', '20:01:19'), ('P0L', '20:07:52'), ('P1H', '20:09:47'), ('P1L', '20:11:19'),
      ('P2H', '20:17:20'), ('P2L', '20:20:07'), ('P3H', '20:22:34'), ('P3L', '20:23:57'), ('P4H', '20:26:19'), ('P4L', '20:27:09')]),
    ('2026-09-23T17-10-27.650542Z', 4, [('P0H', '19:11:47'), ('P0L', '19:14:58'), ('P4H', '19:23:05'), ('P4L', '19:25:10')]),
    ('2026-09-23T17-30-57.946654Z', 4, [('P1H', '19:32:01'), ('P1L', '19:33:02'), ('P3H', '19:34:40'), ('P3L', '19:38:03')]),
    # run B's P0L 19:03:24 is excluded: P4's unexplained -198 Pa event sat on it
    ('2026-09-22T16-45-47.921652Z', 6, [('P0H', '18:47:37'), ('P0L', '18:49:05'), ('P1H', '18:51:04'), ('P1L', '18:52:26'),
      ('P3H', '18:53:51'), ('P3L', '18:55:30'), ('P4H', '18:58:15'), ('P4L', '19:00:15'),
      ('P0H', '19:02:08'), ('P1H', '19:04:22'), ('P1L', '19:05:22'), ('P3H', '19:06:36'), ('P3L', '19:07:31'),
      ('P4H', '19:08:52'), ('P4L', '19:10:20')]),
]
# Rung 5: two marks per charge, Q from the pair. P1H uses its 65 mm mark: the 70 mm one has 1 s of quiet after it.
RUNG5 = [('2026-09-23T18-14-16.530664Z', 'P0H', '20:18:20', '20:19:45', 70), ('2026-09-23T18-14-16.530664Z', 'P0L', '20:22:05', '20:23:20', 70),
         ('2026-09-23T18-14-16.530664Z', 'P1H', '20:25:35', '20:26:47', 65), ('2026-09-23T18-56-50.129743Z', 'P1L', '20:59:30', '21:00:53', 70),
         ('2026-09-23T18-14-16.530664Z', 'P3H', '20:35:35', '20:36:59', 70), ('2026-09-23T18-14-16.530664Z', 'P3L', '20:39:05', '20:40:20', 70),
         ('2026-09-23T18-14-16.530664Z', 'P4H', '20:42:46', '20:44:07', 70), ('2026-09-23T18-14-16.530664Z', 'P4L', '20:45:58', '20:47:18', 70)]

_cache = {}


def load(sid):
    if sid not in _cache:
        pres, baro = [], []
        for line in open(os.path.join(SESS, sid + '-log.ndjson'), encoding='utf-8'):
            r = json.loads(line)
            if r['t'] == 'pressure':
                pres.append((r['taiUs'] / 1e6, {c['ch']: c for c in r['channels']}))
            elif r['t'] == 'enclosure' and r.get('pressureValid'):
                baro.append((r['taiUs'] / 1e6, r['enclosurePressurePa']))
        _cache[sid] = (pres, baro)
    return _cache[sid]


def to_epoch(sid, hms):
    lf = datetime.fromtimestamp(load(sid)[0][0][0], timezone.utc) + LOCAL
    h, m, s = map(int, hms.split(':'))
    return (datetime(lf.year, lf.month, lf.day, h, m, s, tzinfo=timezone.utc) - LOCAL).timestamp()


def eps_at(sid, ch, hms, d, m_dry, m_disp, q_mls):
    pres, baro = load(sid)
    tm = to_epoch(sid, hms)
    reading = st.mean(abs(c[ch]['pressurePa']) for t, c in pres if c[ch]['valid'] and tm - 10 <= t <= tm)
    p_abs = st.mean(p for t, p in baro if abs(t - tm) <= 60)
    p_bell = K_M * (m_dry - m_disp) - K_D * d
    return p_bell, 100 * (reading / ((p_abs / P_CAL) * (p_bell - q_mls * 1e-6 * R_SHORT)) - 1)


rows = []
for sid, rung, marks in MARKS:
    for lab, hms in marks:
        p, e = eps_at(sid, lab[:2], hms, 70, *MASS[rung], Q_ASSUMED[rung])
        rows.append(dict(rung=rung, ch=lab[:2], sign=lab[2], P=p, eps=e))
for sid, lab, t65, t70, use in RUNG5:
    q = 5 / (to_epoch(sid, t70) - to_epoch(sid, t65)) * A_O * 1e-3
    p, e = eps_at(sid, lab[:2], t70 if use == 70 else t65, use, *MASS[5], q)
    rows.append(dict(rung=5, ch=lab[:2], sign=lab[2], P=p, eps=e))

print(f'{len(rows)} marks in force; per rung, mean over both signs (%):')
for rung in range(1, 7):
    cells = []
    for c in CH:
        v = [r['eps'] for r in rows if r['rung'] == rung and r['ch'] == c]
        cells.append(f'{c} {st.mean(v):+6.2f}' if v else f'{c}    -- ')
    print(f'  rung {rung}: ' + '  '.join(cells))

# noise model from genuine repeat pairs: sigma(P)^2 = a^2 + (b/P)^2 through rungs 1 and 6
pairs = {}
for r in rows:
    pairs.setdefault((r['rung'], r['ch'], r['sign']), []).append(r['eps'])
diffs = {}
for (rung, _, _), v in pairs.items():
    if len(v) == 2:
        diffs.setdefault(rung, []).append(v[0] - v[1])
sd = {k: math.sqrt(sum(d * d for d in v) / len(v) / 2) for k, v in diffs.items()}
P_r = {k: st.mean(r['P'] for r in rows if r['rung'] == k) for k in range(1, 7)}
b2 = (sd[1] ** 2 - sd[6] ** 2) / (1 / P_r[1] ** 2 - 1 / P_r[6] ** 2)
a2 = sd[6] ** 2 - b2 / P_r[6] ** 2
sigma = lambda P: math.sqrt(a2 + b2 / P ** 2)
print(f'\nrepeat pairs {({k: len(v) for k, v in diffs.items()})}; single-mark sd {({k: round(v, 2) for k, v in sd.items()})} pp; '
      f'sigma = sqrt({math.sqrt(a2):.2f}^2 + ({math.sqrt(b2):.0f}/P)^2) pp')


def design(rs, rung_offsets, sign=False, slope=False):
    cols, names = [], []
    for c in CH:
        cols.append([1.0 if r['ch'] == c else 0.0 for r in rs]); names.append(f'eps_{c}')
    if rung_offsets:
        present = sorted({r['rung'] for r in rs})
        for k in present[:-1]:   # offsets common to all parts, summing to zero across the rungs
            cols.append([1.0 if r['rung'] == k else (-1.0 if r['rung'] == present[-1] else 0.0) for r in rs]); names.append(f'rung{k}')
    if sign:
        for c in CH:
            cols.append([(0.5 if r['sign'] == 'H' else -0.5) if r['ch'] == c else 0.0 for r in rs]); names.append(f'H-L_{c}')
    if slope:
        for c in CH:
            cols.append([(r['P'] - 200) / 100 if r['ch'] == c else 0.0 for r in rs]); names.append(f'slope_{c}')
    return np.array(cols).T, names


def wls(rs, X):
    y = np.array([r['eps'] for r in rs]); w = np.array([1 / sigma(r['P']) ** 2 for r in rs])
    sw = np.sqrt(w)
    beta, *_ = np.linalg.lstsq(X * sw[:, None], y * sw, rcond=None)
    res = y - X @ beta
    chi2, dof = float(np.sum(w * res ** 2)), len(y) - X.shape[1]
    se = np.sqrt(np.diag(np.linalg.inv((X * sw[:, None]).T @ (X * sw[:, None])))) * math.sqrt(max(1.0, chi2 / dof))
    return beta, se, chi2, dof, res


for title, kw in [('one constant per part', dict(rung_offsets=False)),
                  ('constant per part + common rung offset', dict(rung_offsets=True)),
                  ('+ sign split per part', dict(rung_offsets=True, sign=True)),
                  ('constant + slope per part (pp per 100 Pa)', dict(rung_offsets=False, slope=True)),
                  ('constant + sign split per part', dict(rung_offsets=False, sign=True))]:
    X, names = design(rows, **kw)
    beta, se, chi2, dof, res = wls(rows, X)
    print(f'\n== {title}: chi2/dof = {chi2:.1f}/{dof}; uncertainties inflated x{math.sqrt(max(1, chi2 / dof)):.2f}; '
          f'max |resid| {max(abs(res)):.2f} pp')
    for n, b, s in zip(names, beta, se):
        print(f'  {n:10s} {b:+6.2f} +/- {s:.2f}' + ('   > 2 SE' if not n.startswith('eps_') and abs(b) > 2 * s else ''))

train = [r for r in rows if r['rung'] != 5]; test = [r for r in rows if r['rung'] == 5]
beta, *_ = wls(train, design(train, rung_offsets=False)[0])
resid = [r['eps'] - dict(zip(CH, beta))[r['ch']] for r in test]
print(f'\n== validation, rung 5 held back: rms {math.sqrt(sum(x * x for x in resid) / len(resid)):.2f} pp '
      f'against the noise model\'s {sigma(test[0]["P"]):.2f}; ' + ', '.join('%s%s %+.2f' % (r['ch'], r['sign'], x) for r, x in zip(test, resid)))
