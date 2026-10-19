#!/usr/bin/env python3
"""Gates for the V11 travel input and soft limit, fixed before the run.

Reads from STAGE:
  controlrig-body-angular-v10-timing-v10.json   V10 rerun, 37 fixtures
  controlrig-body-angular-v11-timing-sz0.json   V11, SoftZone 0, travel 0
  controlrig-body-angular-v11-timing-sz25.json  V11, SoftZone 0.25, travel 0
  controlrig-body-angular-v11-softlimit.json    soft-limit fixtures
and, when present, the archived 20 Sep V10 timing run (--archived).

Gates (docs/research/2026-10-19-body-motion-travel-channel.md):
  1 theta <= L + 1e-6 rad on every frame and helper of every V11 run, with L
    the requested travel or the authored Limits/MaxTravel cap.
  2 Unchanged tolerances: review_controlrig_body_timing.py on both V11 runs
    (run separately), and here for the soft fixtures 0.25 deg piecewise/bounce
    and 0.3 deg smooth against the 120 FPS twin, 0.05 deg at aligned times,
    zero pivot and scale drift, preview checks present above 120 FPS.
  3 SoftZone 0 with travel 0 equals the V10 rerun on all 37 fixtures: every
    output rotation bit for bit, and within 0.001 deg.
  4 At 120 FPS, on fixtures whose SoftZone 0 twin hits the hard clamp, the
    largest one-frame change of radial angular speed d|theta|/dt on frames in
    the outer 25 percent band is at most a quarter of the twin's.
  5 ClampContacts is 0 on Firm and Natural smooth translation with SoftZone
    0.25 at every rate. Other counts are reported, not gated.
  6 At 120 FPS with SoftZone 0.25, the largest one-frame change of radial
    angular acceleration in the three frames after a knee crossing is no larger
    than the largest one produced on frames entirely below the knee.
Settle time (to 0.1 deg after the input stops at 2 s) is reported.
"""
import argparse
import bisect
import hashlib
import json
import math
from pathlib import Path

H = 1/120
SETTLE = math.radians(.1)


def multiply(a, b):
    x, y, z, w = a
    X, Y, Z, W = b
    return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X, w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]


def angle(a, b):
    """Degrees between two rotations, accurate near zero."""
    x, y, z, w = a
    q = multiply([-x, -y, -z, w], b)
    return math.degrees(2*math.atan2(math.sqrt(q[0]**2+q[1]**2+q[2]**2), abs(q[3])))


def slerp(a, b, t):
    dot = sum(x*y for x, y in zip(a, b))
    if dot < 0:
        b, dot = [-v for v in b], -dot
    if dot > .9995:
        q = [x+t*(y-x) for x, y in zip(a, b)]
    else:
        theta = math.acos(min(1, dot))
        q = [(math.sin((1-t)*theta)*x+math.sin(t*theta)*y)/math.sin(theta) for x, y in zip(a, b)]
    n = math.sqrt(sum(v*v for v in q))
    return [v/n for v in q]


def comparison(case, reference):
    times = [s['time'] for s in reference['samples']]
    values = [s['rotations'] for s in reference['samples']]
    largest = aligned = 0.
    for sample in case['samples']:
        t = sample['time']
        index = min(len(times)-1, bisect.bisect_left(times, t))
        if abs(times[index]-t) < 1e-7:
            error = max(angle(a, b) for a, b in zip(sample['rotations'], values[index]))
            aligned = max(aligned, error)
        else:
            previous = max(0, index-1)
            alpha = (t-times[previous])/(times[index]-times[previous])
            target = [slerp(a, b, alpha) for a, b in zip(values[previous], values[index])]
            error = max(angle(a, b) for a, b in zip(sample['rotations'], target))
        largest = max(largest, error)
    return dict(all_sample_degrees=largest, aligned_sample_degrees=aligned)


def series(case, i):
    return [s['theta_radians'][i] for s in case['samples']], [s['limit_radians'][i] for s in case['samples']]


def speed_change_in_band(case, i):
    theta, limit = series(case, i)
    v = [0.]+[(theta[k]-theta[k-1])/H for k in range(1, len(theta))]
    largest = 0.
    for k in range(2, len(theta)):
        if theta[k] >= .75*limit[k] or theta[k-1] >= .75*limit[k-1]:
            largest = max(largest, abs(v[k]-v[k-1]))
    return largest


def knee_continuity(case, i, zone):
    theta, limit = series(case, i)
    knee = [(1-zone)*l for l in limit]
    v = [0.]+[(theta[k]-theta[k-1])/H for k in range(1, len(theta))]
    a = [0., 0.]+[(v[k]-v[k-1])/H for k in range(2, len(theta))]
    jump = [0.]*3+[abs(a[k]-a[k-1]) for k in range(3, len(theta))]
    below = [theta[k] < knee[k] for k in range(len(theta))]
    reference = max((jump[k] for k in range(6, len(theta)) if all(below[k-3:k+1])), default=0.)
    crossing = 0.
    crossings = 0
    for k in range(4, len(theta)):
        if below[k] != below[k-1]:
            crossings += 1
            crossing = max(crossing, *(jump[j] for j in range(k, min(k+3, len(theta)))))
    return dict(crossings=crossings, crossing_jump=crossing, below_knee_jump=reference)


def settle_seconds(case):
    last = None
    for s in case['samples']:
        if s['time'] < 2:
            continue
        if max(s['theta_radians']) >= SETTLE:
            last = s['time']
    end = case['samples'][-1]['time']
    if last is None:
        return 0.
    return None if last >= end else last - 2 + 1/case['fps']


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('stage', type=Path)
    parser.add_argument('--archived', type=Path)
    args = parser.parse_args()
    output = args.stage/'controlrig-body-angular-v11-softlimit-review.json'
    if output.exists():
        raise FileExistsError(output)
    paths = dict(v10=args.stage/'controlrig-body-angular-v10-timing-v10.json',
                 sz0=args.stage/'controlrig-body-angular-v11-timing-sz0.json',
                 sz25=args.stage/'controlrig-body-angular-v11-timing-sz25.json',
                 soft=args.stage/'controlrig-body-angular-v11-softlimit.json')
    data = {k: json.loads(p.read_text()) for k, p in paths.items()}
    checks = {}
    metrics = {}

    def check(name, passed, **values):
        checks[name] = dict(passed=bool(passed), **values)

    # Gate 1
    for run in ('sz0', 'sz25', 'soft'):
        excess, frames = -math.inf, 0
        for case in data[run]['cases']:
            for s in case['samples']:
                frames += 1
                for theta, bound in zip(s['theta_radians'], s['limit_radians'], strict=True):
                    excess = max(excess, theta-bound)
        check(f'1/{run}/theta_within_limit', excess <= 1e-6, maximum_excess_radians=excess, frames=frames)

    # Gate 3
    if len(data['v10']['cases']) != 37 or len(data['sz0']['cases']) != 37 or len(data['sz25']['cases']) != 37:
        raise ValueError('Expected 37 fixtures in each timing run')
    reference = {c['name']: c for c in data['v10']['cases']}
    identical = differing = 0
    largest = 0.
    for case in data['sz0']['cases']:
        old = reference[case['name']]
        for a, b in zip(case['samples'], old['samples'], strict=True):
            for x, y in zip(a['body_pose'], b['body_pose'], strict=True):
                same = x['rotation'] == y['rotation'] and x['translation'] == y['translation'] and x['scale'] == y['scale']
                identical += same
                differing += not same
                largest = max(largest, angle(x['rotation'], y['rotation']))
    check('3/sz0_matches_v10_rerun_bitwise', differing == 0, identical_transforms=identical,
          differing_transforms=differing, largest_degrees=largest)
    check('3/sz0_matches_v10_rerun_within_0.001deg', largest <= .001, largest_degrees=largest)
    if args.archived:
        archived = {c['name']: c for c in json.loads(args.archived.read_text())['cases']}
        same_count = diff_count = 0
        largest_archived = 0.
        for case in data['sz0']['cases']:
            for a, b in zip(case['samples'], archived[case['name']]['samples'], strict=True):
                for x, y in zip(a['body_pose'], b['body_pose'], strict=True):
                    same_count += x['rotation'] == y['rotation']
                    diff_count += x['rotation'] != y['rotation']
                    largest_archived = max(largest_archived, angle(x['rotation'], y['rotation']))
        metrics['archived_v10_comparison'] = dict(identical_rotations=same_count, differing_rotations=diff_count,
            largest_degrees=largest_archived, source=str(args.archived),
            source_sha256=hashlib.sha256(args.archived.read_bytes()).hexdigest())
    # Default SoftZone on the 37 fixtures with travel 0: how far it moves output.
    sz25 = {c['name']: c for c in data['sz25']['cases']}
    metrics['sz25_vs_sz0_largest_degrees'] = max(angle(x['rotation'], y['rotation'])
        for case in data['sz0']['cases'] for a, b in zip(case['samples'], sz25[case['name']]['samples'])
        for x, y in zip(a['body_pose'], b['body_pose']))
    for run in ('sz0', 'sz25'):
        metrics[run+'_clamp_contacts'] = {c['name']: c['samples'][-1]['clamp_contacts'] for c in data[run]['cases']}

    # Soft fixtures
    soft = {c['name']: c for c in data['soft']['cases']}
    table = {}
    for name, case in soft.items():
        peak = [max(s['theta_radians'][i]/s['limit_radians'][i] for s in case['samples']) for i in range(7)]
        table[name] = dict(clamp_contacts=case['samples'][-1]['clamp_contacts'], peak_fraction_of_limit=peak,
                           peak_degrees=[math.degrees(max(s['theta_radians'][i] for s in case['samples'])) for i in range(7)],
                           settle_seconds=settle_seconds(case), steps=case['samples'][-1]['steps'],
                           resets=case['samples'][-1]['resets'])
        # Gate 2, per-fixture parts
        check(f'2/{name}/pivot_scale', case['maximum']['pivot_cm'] < 1e-5 and case['maximum']['scale'] < 1e-6,
              pivot_cm=case['maximum']['pivot_cm'], scale=case['maximum']['scale'])
        check(f'2/{name}/unit_quaternion', case['maximum']['quaternion_norm_error'] < 1e-6,
              value=case['maximum']['quaternion_norm_error'])
        check(f'2/{name}/clock', case['maximum']['substeps'] <= 8 and case['maximum']['preview_steps'] <= 1
              and case['samples'][-1]['resets'] == 1, substeps=case['maximum']['substeps'], resets=case['samples'][-1]['resets'])
        if case['fps'] > 120:
            check(f'2/{name}/preview_state_checked', case['preview_unchanged_frames'] > 0,
                  frames=case['preview_unchanged_frames'])
        base = name.rsplit('_', 1)[0]
        if case['fps'] != 120 and base+'_120' in soft:
            result = comparison(case, soft[base+'_120'])
            table[name]['timing'] = result
            limit = .3 if case['motion_kind'] == 'smooth' else .25
            check(f'2/{name}/timing_vs_120', result['all_sample_degrees'] <= limit, limit=limit, **result)
            if case['motion_kind'] != 'smooth':
                check(f'2/{name}/aligned_timing_vs_120', result['aligned_sample_degrees'] <= .05, **result)
    metrics['soft_fixtures'] = table

    # Gates 4 and 6 at 120 FPS, gate 5
    for name, case in soft.items():
        if case['soft_zone'] == 0 or case['fps'] != 120:
            continue
        twin = soft[name.replace('_sz25_', '_sz0_')]
        hit = twin['samples'][-1]['clamp_contacts'] > 0
        soft_change = max(speed_change_in_band(case, i) for i in range(7))
        hard_change = max(speed_change_in_band(twin, i) for i in range(7))
        table[name]['band_speed_change'] = dict(soft=soft_change, hard=hard_change, hard_clamp_hit=hit)
        if hit:
            check(f'4/{name}/band_speed_change_quarter_of_hard', soft_change <= .25*hard_change,
                  soft_rad_per_s=soft_change, hard_rad_per_s=hard_change,
                  ratio=soft_change/hard_change if hard_change else None)
        knees = [knee_continuity(case, i, case['soft_zone']) for i in range(7)]
        table[name]['knee'] = knees
        crossing = max(k['crossing_jump'] for k in knees)
        below = max(k['below_knee_jump'] for k in knees)
        if any(k['crossings'] for k in knees):
            check(f'6/{name}/knee_acceleration_step', crossing <= below,
                  crossing_rad_per_s2=crossing, below_knee_rad_per_s2=below)
    for name, case in soft.items():
        if case['template'] in ('firm', 'natural') and case['motion_kind'] == 'smooth' and case['soft_zone'] > 0:
            check(f'5/{name}/no_clamp_contacts', case['samples'][-1]['clamp_contacts'] == 0,
                  clamp_contacts=case['samples'][-1]['clamp_contacts'])

    # Not a gate: SoftZone 0.5 at 120 FPS (probe -CSSBodySoftZones=0.5
    # -CSSBodyRates=120 -CSSBodyLabel=wide), measured with the gate 4 and 6
    # metrics against the same SoftZone 0 twins, to size the band for a retune.
    wide_path = args.stage/'controlrig-body-angular-v11-softlimit-wide.json'
    if wide_path.exists():
        wide = {}
        for case in json.loads(wide_path.read_text())['cases']:
            twin = soft[case['name'].replace('_sz50_', '_sz0_')]
            knees = [knee_continuity(case, i, case['soft_zone']) for i in range(7)]
            wide[case['name']] = dict(clamp_contacts=case['samples'][-1]['clamp_contacts'],
                twin_clamp_contacts=twin['samples'][-1]['clamp_contacts'],
                band_speed_change=max(speed_change_in_band(case, i) for i in range(7)),
                twin_band_speed_change=max(speed_change_in_band(twin, i) for i in range(7)),
                knee_crossing_jump=max(k['crossing_jump'] for k in knees),
                below_knee_jump=max(k['below_knee_jump'] for k in knees),
                settle_seconds=settle_seconds(case))
        metrics['exploratory_soft_zone_0_5'] = wide
        paths['wide'] = wide_path
    failures = sorted(k for k, v in checks.items() if not v['passed'])
    report = dict(passed=not failures, checks=checks, failures=failures, metrics=metrics,
        sources={k: hashlib.sha256(p.read_bytes()).hexdigest() for k, p in paths.items()},
        scope='Direct timed V11 rig, synthetic fixtures. No AnimBP, cook, clip or live acceptance.')
    output.write_text(json.dumps(report, indent=1)+'\n')
    print(f'{len(checks)-len(failures)}/{len(checks)} checks passed')
    for name in failures[:60]:
        print('FAIL', name, json.dumps({k: v for k, v in checks[name].items() if k != 'passed'}))
    raise SystemExit(1 if failures else 0)


if __name__ == '__main__':
    main()
