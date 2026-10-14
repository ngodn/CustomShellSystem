"""Validate and summarize paired editor animation timings without game access."""
import argparse
from collections import defaultdict
import hashlib
import json
import math
from pathlib import Path
import statistics


def metrics(values):
    ordered = sorted(values)
    return dict(mean=statistics.fmean(values), median=statistics.median(values),
                p95=ordered[math.ceil(.95*len(values))-1],
                p99=ordered[math.ceil(.99*len(values))-1], maximum=ordered[-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', type=Path)
    args = parser.parse_args()
    stage = args.stage.resolve()
    data = json.loads((stage/'body-cost.json').read_text())
    receipt = json.loads((stage/'exit.json').read_text())
    assert data['passed'] and receipt == dict(returncode=0, sources_unchanged=True)
    inputs = json.loads((stage/'inputs.json').read_text())
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in inputs.items())
    cases = data['cases']
    assert len(cases) == 48
    groups = defaultdict(dict)
    seen = set()
    for case in cases:
        key = (case['mesh'], case['fps'], case['repeat'], case['body_enabled'])
        assert key not in seen
        seen.add(key)
        assert case['passed'] and case['fps'] in (30, 60, 120) and case['repeat'] in range(4)
        assert case['rig_count'] == 3 and case['warmup_frames'] == 2*case['fps']
        assert case['body_class'] == '/Game/CSS/eins0fx/EveSB/CR_Body.CR_Body_C'
        limits = [float(v) for v in case['limits'].strip('()').split(',')]
        travel = [float(v) for v in case['travel'].strip('()').split(',')]
        assert len(limits) == 7 and all(abs(a-math.radians(b)) < 1e-6 for a,b in zip(limits,[6,6,10,10,3,3,5]))
        assert travel == [1.5,1.5,1,1,.5,.5,.5]
        rows = case['samples']
        assert len(rows) == 600 and [r['frame'] for r in rows] == list(range(600))
        for row in rows:
            assert all(math.isfinite(row[k]) and row[k] >= 0 for k in ('update_ms','refresh_ms','total_ms'))
            assert abs(row['total_ms']-row['update_ms']-row['refresh_ms']) < 1e-6
        increments = [b['steps']-a['steps'] for a,b in zip(rows,rows[1:])]
        assert all(v == (120//case['fps'] if case['body_enabled'] else 0) for v in increments)
        groups[key[:2]][key[2:]] = metrics([r['total_ms'] for r in rows])
    assert len(groups) == 6
    summary = []
    for (mesh, fps), values in sorted(groups.items()):
        assert set(values) == {(i, enabled) for i in range(4) for enabled in (False, True)}
        delta = [values[i,True]['median']-values[i,False]['median'] for i in range(4)]
        summary.append(dict(mesh=mesh, fps=fps,
            disabled_median_ms=statistics.median(values[i,False]['median'] for i in range(4)),
            enabled_median_ms=statistics.median(values[i,True]['median'] for i in range(4)),
            paired_median_deltas_ms=delta, median_paired_delta_ms=statistics.median(delta),
            runs=[dict(repeat=i, enabled=enabled, **values[i,enabled]) for i in range(4) for enabled in (False,True)]))
    report = dict(passed=True, cases=len(cases), measured_frames=48*600, summary=summary,
        scope='Synchronous editor TickAnimation plus RefreshBoneTransforms elapsed wall time. '
              'Paired enable/bypass estimates incremental body-solver work in this fixture. '
              'No cloth, GPU, live CSS core or gameplay frame-rate acceptance.',
        data_sha256=hashlib.sha256((stage/'body-cost.json').read_bytes()).hexdigest())
    (stage/'verified.json').write_text(json.dumps(report,indent=2)+'\n')
    for row in summary:
        print(row['mesh'].split('/')[-1],row['fps'], 'disabled/enabled/delta ms',
              *(round(row[k],4) for k in ('disabled_median_ms','enabled_median_ms','median_paired_delta_ms')))


if __name__ == '__main__':
    main()
