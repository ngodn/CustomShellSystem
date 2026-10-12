"""Verify the native frame-variation matrix after its editor commandlet exits."""
import json
from pathlib import Path
import sys


def verify(stage):
    exit_result = json.loads((stage / 'exit.json').read_text())
    assert exit_result == {'returncode': 0, 'inputs_unchanged': True}, exit_result
    data = json.loads((stage / 'frame-variation.json').read_text())
    patterns = {'steady_30', 'uneven_no_hitch', 'isolated_hitch', 'repeated_hitches'}
    expected = {(rig, preset, pattern) for rig in ('current', 'v42')
                for preset in ('firm', 'natural', 'off') for pattern in patterns}
    baseline = json.loads((stage.parent / 'peony-v2-motion-compare1/response.json').read_text())
    assert len(data['cases']) == len(expected) == 24
    seen, rows = set(), []
    for row in data['cases']:
        identity = row['mode'], row['preset'], row['pattern']
        assert identity not in seen
        seen.add(identity)
        timing, angles = row['timing'], row['samples_degrees']
        assert len(timing) == len(angles) and timing[0]['delta'] == 0
        assert abs(timing[-1]['seconds']-4) < 1e-9
        actual = [i for i, t in enumerate(timing)
                  if i == 0 or t['resets'] != timing[i-1]['resets']]
        wanted = [i for i, t in enumerate(timing) if i == 0 or t['delta'] > 8/120+1e-6]
        assert actual == wanted, (identity, actual, wanted)
        assert timing[-1]['resets'] == len(wanted)
        assert all(max(angles[i]) < .001 for i in actual)
        assert all(0 <= t['substeps'] <= 8 for t in timing)
        if row['preset'] == 'off':
            assert max(row['peak_degrees']) < .001
        if row['pattern'] == 'steady_30':
            old = next(x for x in baseline['cases']
                       if (x['mode'], x['preset'], x['fps']) == (*identity[:2], 30))
            assert max(abs(a-b) for a, b in zip(row['peak_degrees'], old['peak_degrees'])) < 1e-3
        jump = max((max(abs(a-b) for a, b in zip(angles[i][:4], angles[i-1][:4]))
                    for i in actual if i), default=0)
        rows.append(dict(mode=row['mode'], preset=row['preset'], pattern=row['pattern'],
                         resets=len(actual), peak_degrees=row['peak_degrees'],
                         max_reset_jump_degrees=jump))
    assert seen == expected
    return dict(cases=24, all_resets_match_hitch_or_initialization=True,
                steady_baseline_matches=True, results=rows)


if __name__ == '__main__':
    stage = Path(sys.argv[1])
    result = verify(stage)
    (stage / 'verified.json').write_text(json.dumps(result, indent=2)+'\n')
    print('24 native cases verified; steady baseline unchanged')
