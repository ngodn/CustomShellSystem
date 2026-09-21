"""Verify collision binding and preserved payload in the independently decoded V44 cook."""
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT = WORK/'b2-body-bound-cooked-readback-v1'
DECODED = OUT/'decoded'
OLD = WORK/'b2-reference-cooked-readback-v1/decoded'
load = lambda p: json.loads(p.read_text())
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert not (OUT/'validation.json').exists()
assert load(OUT/'report.json')['passed']
assert all(load(OUT/(n+'.exit.json'))['exit_code'] == 0 for n in ('pack', 'verify', 'decode'))
assert load(WORK/'b2-body-bound-cook-v2/exit.json')['exit_code'] == 0
assert load(WORK/'b2-reference-cooked-readback-v1/validation.json')['passed']
old_stem, stem = 'SK_B2GameReferenceMetadata_V2', 'SK_B2PhysicsBound_V1'
old_mesh = load(OLD/(old_stem+'.json'))
mesh = load(DECODED/(stem+'.json'))
item = next(x for x in mesh if x['Type'] == 'SkeletalMesh')
physics_path = '/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V3'
assert item['Properties']['PhysicsAsset']['ObjectPath'].rsplit('.', 1)[0] == physics_path
# Normalize owner identity and the new Physics Asset reference.
normalized = json.loads(json.dumps(mesh).replace(stem, old_stem))
next(x for x in normalized if x['Type'] == 'SkeletalMesh')['Properties'].pop('PhysicsAsset')
# Adding a serialized property relocates the empty Nanite bulk-data marker.
# No payload is ignored: require an empty, unflagged stream on both sides.
empty_offsets = []
for package in (old_mesh, normalized):
    pages = next(x for x in package if x['Type'] == 'SkeletalMesh')['NaniteResources']['StreamablePages']
    assert pages['ElementCount'] == pages['SizeOnDisk'] == 0 and pages['BulkDataFlags'] == 'BULKDATA_None'
    empty_offsets.append(pages.pop('OffsetInFile'))
assert normalized == old_mesh, 'Cooked mesh changed beyond identity, binding and empty bulk location'
old_psk = next(OLD.rglob(old_stem+'.pskx'))
new_psk = next(DECODED.rglob(stem+'.pskx'))
assert old_psk.read_bytes() == new_psk.read_bytes(), 'Geometry/skin/material/morph interchange changed'
assert load(OLD/(old_stem+'.refskel.json')) == load(DECODED/(stem+'.refskel.json'))
preserved = ['SKEL_B2GameReferenceMetadata_V2', 'ABP_CSS_ControlRigProbeHandB2ReferenceV2', 'CR_CSS_LeftHandCombinedV1']
for name in preserved:
    assert load(OLD/(name+'.json')) == load(DECODED/(name+'.json')), name
physics = load(DECODED/'PA_B2BodyFit_V3.json')
asset = next(x for x in physics if x['Type'] == 'PhysicsAsset')
by_name = {x['Name']: x for x in physics}
def resolve(ref):
    return by_name[ref['ObjectName'].split(':')[-1].rstrip("'")]
props = asset['Properties']
body_objects = [resolve(r) for r in props['SkeletalBodySetups']]
joint_objects = [resolve(r) for r in props['ConstraintSetup']]
assert len(body_objects) == 22 and all(x['Type'] == 'SkeletalBodySetup' for x in body_objects)
assert len(joint_objects) == 21 and all(x['Type'] == 'PhysicsConstraintTemplate' for x in joint_objects)
actual = dict(bodies=[x['Properties'] for x in body_objects],
    constraints=[x['Properties'] for x in joint_objects], bounds_bodies=props['BoundsBodies'],
    solver_settings=props['SolverSettings'], collision_disable_table=[
        dict(indices=x['Key']['Indices'], value=x['Value']) for x in asset['CollisionDisableTable']])
expected = load(WORK/'b2-body-hand-fit-v1/candidate.json')
def subset(a, b, path=''):
    if isinstance(a, dict):
        assert isinstance(b, dict), path
        for key, value in a.items():
            assert key in b, (path, key, 'missing')
            subset(value, b[key], path+'/'+key)
    elif isinstance(a, list):
        assert len(a) == len(b), path
        for i, (x,y) in enumerate(zip(a,b,strict=True)):
            subset(x,y,path+'/'+str(i))
    elif isinstance(a, bool):
        assert a is b, (path,a,b)
    elif isinstance(a, (int,float)):
        # The fitted definition contains doubles; several UE shape fields cook as float32.
        assert isinstance(b, (int,float)) and math.isclose(a,b,rel_tol=1e-6,abs_tol=1e-5), (path,a,b)
    else:
        assert a == b or (isinstance(a,str) and isinstance(b,str) and '::' in a
                         and a.split('::')[-1] == b), (path,a,b)
for key in ('bodies', 'constraints', 'bounds_bodies', 'solver_settings'):
    subset(expected[key], actual[key], key)
pair_sort = lambda xs: sorted(xs, key=lambda x:x['indices'])
assert pair_sort(expected['collision_disable_table']) == pair_sort(actual['collision_disable_table'])
shape_counts = {kind:sum(len(body['AggGeom'].get(kind, [])) for body in actual['bodies'])
                for kind in ('SphylElems','BoxElems','TaperedCapsuleElems')}
assert sum(shape_counts.values()) == 23
report = dict(passed=True, physics_package=physics_path, bodies=22, constraints=21,
    shapes=shape_counts, collision_disable_pairs=len(actual['collision_disable_table']),
    geometry_skin_morph_bytes_identical=True, mesh_content_preserved_except_physics=True,
    relocated_empty_nanite_bulk_offsets=empty_offsets,
    skeleton_animation_rig_preserved=preserved, scope=__doc__, cooked_execution_verified=False,
    game_damage_parry_verified=False, source_hashes={str(p):digest(p) for p in
        (old_psk,new_psk,DECODED/'PA_B2BodyFit_V3.json',WORK/'b2-body-hand-fit-v1/candidate.json')})
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k != 'source_hashes'}))
