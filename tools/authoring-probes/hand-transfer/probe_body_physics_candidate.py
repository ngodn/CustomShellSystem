"""Save/read a fitted diagnostic Physics Asset and test actual component queries.

Pinned UE 5.6.1 editor Python. Does not modify the source mesh or game files.
"""
import hashlib
import json
import math
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_BODY_PHYSICS_PROBE_DIR']).resolve()
assert OUT.parent == WORK.resolve() and not OUT.exists()
OUT.mkdir()
FIT = WORK / 'b2-body-physics-fit-v4'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
PACKAGE = '/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V1'
MESH = '/Game/CSSAuthoring/DiagnosticReferences/SK_B2GameReferenceMetadata_V2'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
write = lambda name, value: (OUT/name).write_text(json.dumps(value, indent=2)+'\n')
protected = json.loads((WORK/'b2-reference-fresh-v2/report.json').read_text())['protected_hashes']
assert all(digest(Path(path)) == value for path, value in protected.items())
definition = json.loads((FIT/'candidate.json').read_text())
queries = json.loads((FIT/'queries.json').read_text())
mesh = unreal.load_asset(MESH)
assert mesh and mesh.get_editor_property('physics_asset') is None
create = os.environ.get('CSS_BODY_PHYSICS_CREATE') == '1'
if create:
    assert not unreal.EditorAssetLibrary.does_asset_exist(PACKAGE)
    asset = unreal.CSSPhysicsProbeLibrary.create_body_candidate(mesh, json.dumps(definition), PACKAGE)
else:
    asset = unreal.load_asset(PACKAGE)
assert asset
actual = json.loads(unreal.CSSPhysicsProbeLibrary.inspect_body_candidate(asset))
write('actual-asset.json', actual)


def subset(expected, value, path=''):
    """Every source field must survive, including nested fields absent from defaults."""
    if isinstance(expected, dict):
        if isinstance(value, str):
            value = {k.lower(): float(v) for k, v in re.findall(
                r'([A-Za-z][A-Za-z0-9]*)=([-+0-9.eE]+)', value)}
        assert isinstance(value, dict), (path, expected, value)
        lower = {k.lower(): v for k, v in value.items()}
        for key, item in expected.items():
            assert key.lower() in lower, (path, key, 'missing')
            subset(item, lower[key.lower()], path+'/'+key)
    elif isinstance(expected, list):
        assert len(expected) == len(value), (path, len(expected), len(value))
        for i, (a, b) in enumerate(zip(expected, value, strict=True)):
            subset(a, b, path+'/'+str(i))
    elif isinstance(expected, bool):
        assert expected is value, (path, expected, value)
    elif isinstance(expected, (int, float)):
        assert isinstance(value, (int, float)) and math.isclose(expected, value, rel_tol=1e-6, abs_tol=1e-5), (path, expected, value)
    else:
        assert expected == value or (isinstance(expected, str) and isinstance(value, str)
                                    and expected.split('::')[-1] == value.split('::')[-1]), (path, expected, value)


for key in ('bodies', 'constraints', 'solver_settings', 'bounds_bodies'):
    subset(definition[key], actual[key], key)
pair_sort = lambda rows: sorted(rows, key=lambda row: row['indices'])
assert pair_sort(definition['collision_disable_table']) == pair_sort(actual['collision_disable_table'])
if create:
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

result = json.loads(unreal.CSSPhysicsProbeLibrary.probe_body_queries(mesh, asset, json.dumps(queries)))
write('queries.json', result)
assert result['world_destroyed']
assert len(result['phases']) == 4
failures = []
for phase in result['phases']:
    count = 22 if phase['fitted'] else 0
    assert phase['body_count'] == phase['valid_bodies'] == count, phase
    assert len(phase['queries']) == len(queries)
    for query, observed in zip(queries, phase['queries'], strict=True):
        assert query['id'] == observed['id']
        expected = query['expected'] and phase['fitted']
        if observed['hit'] != expected:
            failures.append(dict(phase=phase['phase'], expected=expected, **observed))
write('query-failures.json', failures)
assert not failures, failures[:10]
assert mesh.get_editor_property('physics_asset') is None
assert all(digest(Path(path)) == value for path, value in protected.items())
write('report.json', dict(passed=True, created=create, package=PACKAGE,
    query_cases=len(queries)*4, bodies=22, constraints=21,
    protected_hashes=protected, fitted_asset_sha256=digest(CONTENT/(PACKAGE.removeprefix('/Game/')+'.uasset')),
    scope=__doc__, game_damage_parry_verified=False, ragdoll_verified=False))
print('CSS_BODY_PHYSICS_QUERIES_PASS', len(queries)*4, flush=True)
