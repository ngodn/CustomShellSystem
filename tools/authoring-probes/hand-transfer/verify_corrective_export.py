"""Verify B2 adds only the 16 internal morphs and the isolated mesh package."""
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = WORK / 'arm-rest-correctives-export-v1'
base_path = WORK / 'arm-rest-alignment-v1/full-candidate.mesh.json'
new_path = OUT / 'candidate.mesh.json'
base, new = [json.loads(p.read_text()) for p in (base_path, new_path)]
audit = json.loads(new_path.with_suffix('.audit.json').read_text())
models = json.loads(Path(__file__).with_name('left-finger-correctives-v1.json').read_text())['parameters']
assert base.keys() == new.keys()
unchanged = [k for k in base if k not in ('mesh_package', 'morph_targets')]
assert all(base[k] == new[k] for k in unchanged), 'Adding correctives changed mesh data'
assert new['mesh_package'] == '/Game/CSSAuthoring/DiagnosticReferences/SK_ArmRestV44B2Correctives_V1'
old_shapes = {m['name']: m for m in base['morph_targets']}
new_shapes = {m['name']: m for m in new['morph_targets']}
assert len(old_shapes) == 6 and len(new_shapes) == len(new['morph_targets']) == 22
assert all(new_shapes[n] == m for n, m in old_shapes.items())
assert new_shapes.keys() - old_shapes.keys() == {m['shape'] for m in models}
assert audit['output_sha256'] == hashlib.sha256(new_path.read_bytes()).hexdigest()
for model in models:
    declaration = audit['internal_morphs'][model['shape']]
    assert declaration['baked_value'] == model['baked_value']
    assert declaration['weight_contract'] == 'desired_source_value_minus_baked_value'
    deltas = new_shapes[model['shape']]['deltas']
    assert deltas and len({d[0] for d in deltas}) == len(deltas)
    assert all(0 <= d[0] < len(new['points']) and all(math.isfinite(v) for v in d[1:]) for d in deltas)
report = dict(unchanged_fields=unchanged, original_morphs=list(old_shapes), internal_morphs=sorted(audit['internal_morphs']),
              counts=audit['counts'], base_sha256=hashlib.sha256(base_path.read_bytes()).hexdigest(),
              output_sha256=audit['output_sha256'], baked_values_match_driver=True,
              scope='Exact interchange preservation and driver/export agreement. Not full source delta comparison, UE import, cook or GPU execution.')
(OUT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
