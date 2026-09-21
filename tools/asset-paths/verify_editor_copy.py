"""Fresh editor verification of copied bindings and actual hand/physics behavior."""
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import unreal

ROOT = Path(__file__).resolve().parents[3]
CSS = ROOT / 'CustomShellSystem'
OUT = Path(os.environ['CSS_CLOSURE_WORK']).resolve()
assert OUT.is_relative_to(CSS / 'work') and not (OUT / 'verified.json').exists()
load = lambda p: json.loads(p.read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
write = lambda n,v: (OUT/n).write_text(json.dumps(v, indent=2)+'\n')
prior = load(OUT/'copied.json')
assert prior['passed'] and load(OUT/'copy-exit.json')['exit_code'] == 0
mapping = load(OUT/'map.json')
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected = dict(prior['protected'])
protected.update({str(content/(p.removeprefix('/Game/')+'.uasset')):h for p,h in prior['assets'].items()})
assert all(sha(Path(p)) == h for p,h in protected.items())


def normalized(value):
    if isinstance(value, dict): return {k:normalized(v) for k,v in value.items()}
    if isinstance(value, list): return [normalized(v) for v in value]
    if isinstance(value, str):
        for old,new in sorted(mapping.items(), key=lambda pair:-len(pair[0])):
            value = value.replace(old+'.'+old.rsplit('/',1)[1], new+'.'+new.rsplit('/',1)[1])
            value = value.replace(old,new)
    return value


def path(obj): return obj.get_path_name() if obj else None

def same(a, b, context):
    assert normalized(a) == b, context


def material_slots(mesh):
    return json.loads(unreal.CSSRetargetLibrary.inspect_mesh_materials(mesh))


registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_searchable_names=False,
    include_soft_management_references=False, include_hard_management_references=False)
closure = load(OUT/'closure.json')['packages']
rows = {}
for old, info in closure.items():
    new = mapping[old]
    original, candidate = unreal.load_asset(old), unreal.load_asset(new)
    assert original and candidate and original.get_class() == candidate.get_class(), new
    expected = sorted(mapping.get(d,d) for d in info['dependencies'])
    actual = sorted(str(d) for d in registry.get_dependencies(new,options))
    assert expected == actual, (new,'dependencies',expected,actual)
    kind = info['classes'][0]
    if kind == 'SkeletalMesh':
        same(json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(original)),
             json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(candidate)), (new,'bind'))
        for key in ('skeleton','physics_asset','shadow_physics_asset','post_process_anim_blueprint'):
            same(path(original.get_editor_property(key)),path(candidate.get_editor_property(key)),(new,key))
        same(material_slots(original),material_slots(candidate),(new,'materials'))
        assert [str(m.get_name()) for m in original.get_editor_property('morph_targets')] == [
            str(m.get_name()) for m in candidate.get_editor_property('morph_targets')],(new,'morphs')
    elif kind == 'Skeleton':
        for function in ('inspect_skeleton','inspect_skeleton_metadata','inspect_virtual_bones'):
            inspect = getattr(unreal.CSSRetargetLibrary,function)
            same(json.loads(inspect(original)),json.loads(inspect(candidate)),(new,function))
    elif kind == 'PhysicsAsset':
        same(json.loads(unreal.CSSPhysicsProbeLibrary.inspect_body_candidate(original)),
             json.loads(unreal.CSSPhysicsProbeLibrary.inspect_body_candidate(candidate)),(new,'physics'))
    elif kind == 'AnimBlueprint':
        same(path(original.get_editor_property('target_skeleton')),path(candidate.get_editor_property('target_skeleton')),(new,'target'))
        assert candidate.generated_class()
        for function in ('inspect_spring_defaults','inspect_dynamics_defaults','inspect_pose_corrections'):
            inspect = getattr(unreal.CSSRetargetLibrary,function)
            same(json.loads(inspect(original)),json.loads(inspect(candidate)),(new,function))
    rows[new] = dict(kind=kind, dependencies=actual)
write('bindings.json',rows)

old_mesh = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_HeelSupportsV45C')
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl')
old_bp = unreal.load_asset('/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2')
bp = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
assert len(mesh.get_editor_property('materials')) == 30
assert len(mesh.get_editor_property('morph_targets')) == 22
skeleton = mesh.get_editor_property('skeleton')
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))) == 379
assert len(json.loads(unreal.CSSRetargetLibrary.inspect_virtual_bones(skeleton))) == 9
for name,expected in {'CSSStiffness':200.,'CSSDamping':24.,'CSSHandEnabled':True,'CSSHandInputIsV43Compatible':False}.items():
    assert unreal.get_default_object(bp.generated_class()).get_editor_property(name) == expected

work = CSS/'work/grip-grounding-v1'
curves = [m['shape'] for m in load(CSS/'tools/authoring-probes/hand-transfer/left-finger-correctives-v1.json')['parameters']]
maximum_error = 0.
def compare(a,b,where=''):
    global maximum_error
    if isinstance(a,dict):
        assert isinstance(b,dict) and a.keys()==b.keys(),where
        for key in a: compare(a[key],b[key],where+'/'+key)
    elif isinstance(a,list):
        assert isinstance(b,list) and len(a)==len(b),where
        for i,(x,y) in enumerate(zip(a,b,strict=True)):compare(x,y,where+'/'+str(i))
    elif isinstance(a,(int,float)) and not isinstance(a,bool):
        error=abs(a-b);assert math.isfinite(error) and error<=1e-6,(where,a,b)
        maximum_error=max(maximum_error,error)
    else: assert a==b,(where,a,b)

for sample in range(5):
    snapshot = load(work/'b2-animation-binding-v1'/f'candidate_game_rotations-s{sample}-compressed-ik1.json')['pose']['Snapshot']
    before = json.loads(unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(old_mesh,old_bp,json.dumps(snapshot),curves,False))
    after = json.loads(unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(mesh,bp,json.dumps(snapshot),curves,False))
    write(f'hand-old{sample}.json',before);write(f'hand-new{sample}.json',after)
    compare(before,after,str(sample))
queries = load(work/'b2-body-hand-fit-v1/queries.json')
physics = mesh.get_editor_property('physics_asset')
actual = json.loads(unreal.CSSPhysicsProbeLibrary.probe_body_queries(mesh,physics,json.dumps(queries),True))
write('queries.json',actual)
assert actual['world_destroyed'] and actual['uses_mesh_binding'] and len(actual['phases']) == 2
for phase in actual['phases']:
    assert phase['body_count'] == phase['valid_bodies'] == 22
    for expected,observed in zip(queries,phase['queries'],strict=True):
        assert expected['id'] == observed['id'] and expected['expected'] == observed['hit']
        if 'probe_point' in expected:
            assert observed['point_body'].lower() == expected['probe_bone'].lower()
            distance=observed['point_distance']
            assert 0<=distance<=.001 if expected['expected'] else distance>100
assert all(sha(Path(p)) == h for p,h in protected.items())
write('verified.json',dict(passed=True,assets=len(rows),raw_bones=379,virtual_bones=9,
    materials=30,morphs=22,hand_cases=35,maximum_component_error=maximum_error,
    body_query_phases=2,protected_hashes=protected,
    scope='Fresh editor binding, dependency, hand component and bound collision comparison. Cook, moving body/hair and live-game checks remain.'))
print('CSS_COPIED_ASSETS_VERIFIED',len(rows),flush=True)
