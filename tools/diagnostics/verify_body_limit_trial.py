"""Compare the saved candidate with the isolated limit intervention at every frame."""
import hashlib
import json
from pathlib import Path
import sys


stage = Path(sys.argv[1])
baseline = Path(sys.argv[2])
for name in ('build-exit.json', 'author-exit.json', 'verify-reload-exit.json', 'clip-exit.json'):
    assert json.loads((stage/name).read_text())['returncode'] == 0, name
authored = json.loads((stage/'authored.json').read_text())
reloaded = json.loads((stage/'reloaded.json').read_text())
assert reloaded['passed'] and reloaded['after'] == authored['after']
assert authored['before']['graph_sha256'] == authored['after']['graph_sha256']
assert {name for name, value in authored['before']['values'].items()
        if value != authored['after']['values'][name]} == {'Limits', 'MaxTravel'}
source = json.loads((stage/'input.json').read_text())
assert hashlib.sha256(Path(source['source']).read_bytes()).hexdigest() == source['sha256']
asset = stage/'editor/Content/CSS/eins0fx/EveSB/CR_Body.uasset'
assert hashlib.sha256(asset.read_bytes()).hexdigest() == authored['sha256']
candidate = json.loads((stage/'response.json').read_text())
old = json.loads((baseline/'response.json').read_text())
assert candidate['inputs_unchanged'] and len(candidate['cases']) == 6
seen, maximum = set(), 0.
for row in candidate['cases']:
    identity = row['clip'], row['preset']
    assert identity not in seen
    seen.add(identity)
    reference = next(r for r in old['cases'] if
        (r['clip'], r['preset'], r['mode']) == (*identity, 'current_old_limits'))
    assert len(row['samples_degrees']) == len(reference['samples_degrees'])
    for actual, expected in zip(row['samples_degrees'], reference['samples_degrees']):
        assert len(actual) == len(expected) == 7
        maximum = max(maximum, *(abs(a-b) for a, b in zip(actual, expected)))
    assert row['timing'] == reference['timing']
assert seen == {(r['clip'], r['preset']) for r in old['cases']}
assert maximum < .0001, maximum
proof = dict(passed=True, cases=6, maximum_angle_difference_degrees=maximum,
             source_unchanged=True, graph_unchanged=True,
             defaults_survive_reload_and_compile=True, candidate_sha256=authored['sha256'],
             remaining=['Windows cook and readback', 'package verification', 'gameplay acceptance'])
(stage/'verified.json').write_text(json.dumps(proof, indent=2)+'\n')
print('Saved candidate verified against all six limit-only cases; max difference:',maximum)
