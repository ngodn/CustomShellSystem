"""Verify saved Blender hand evidence; baseline must fail this same gate."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('evidence', type=Path)
parser.add_argument('--variant', choices=('baseline', 'calibrated-no-correctives'), required=True)
args = parser.parse_args()
root = args.evidence.resolve()
report = json.loads((root / 'report.json').read_text())
checks = json.loads((root / 'input-domain-checks.json').read_text())
assert len(checks) == 95
assert max(r['raw_rotation_error_degrees'] for r in checks) < .001

def pairs(path):
    data = json.loads(path.read_text())
    return {tuple(sorted(tuple(sorted(t)) for t in p['triangles'])) for p in data['pairs']}

neutral = pairs(root / 'v44b2-control-0-contacts.json')
assert len(neutral) == 4
for version in ('v43', 'v44b2'):
    for control in (0, 60):
        assert pairs(root / f'{version}-control-{control}-contacts.json') == neutral
rows = []
for sample in range(5):
    label = f'h2-s{sample}-{args.variant}'
    row = next(r for r in report['cases'] if r['case'] == label)
    actual = pairs(root / f'v44b2-{label}-contacts.json')
    extra = actual - neutral
    assert row['new_pairs'] == len(extra)
    rows.append(dict(sample=sample, new_pairs=len(extra), neutral_pairs_retained=neutral <= actual))
passed = all(r['new_pairs'] == 0 and r['neutral_pairs_retained'] for r in rows)
print(json.dumps(dict(passed=passed, variant=args.variant, samples=rows,
                      scope='Five H2 samples only, not weapon contact or live acceptance.'), indent=2))
raise SystemExit(0 if passed else 2)
