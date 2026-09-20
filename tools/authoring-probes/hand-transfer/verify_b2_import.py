"""Fresh-process readback of the complete B2 mesh and its isolated Skeleton."""
import hashlib
import json
import math
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'arm-rest-b2-full-import-v1'
assert not (OUT/'readback.json').exists()
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source=load(OUT/'input.json');assert digest(Path(source['source']))==source['sha256']
assert load(OUT/'exit.json')['exit_code']==0
saved=load(OUT/'saved-assets.json')
assert all(digest(Path(p))==info['sha256'] for p,info in saved.items())
data=load(Path(source['source']))
mesh=unreal.load_asset(data['mesh_package']);skeleton=unreal.load_asset(data['skeleton_package'])
assert mesh and skeleton and mesh.get_editor_property('skeleton')==skeleton
bind=json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
reference=json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))
assert len(bind)==len(reference)==len(data['bones'])==379
errors=[]
# USkeleton::CreateReferenceSkeletonFromMesh calls FReferenceSkeleton::Add,
# which normalizes every copied quaternion again (UE 5.6.1). Bound only that
# rounding difference; translations and scales must still match exactly.
rotation_rounding_limit=4*math.ulp(1.0)
for actual,ref,wanted in zip(bind,reference,data['bones'],strict=True):
    assert actual['name']==ref['name']==wanted['name'] and actual['parent']==ref['parent']==wanted['parent']
    position=max(abs(a-b) for a,b in zip(actual['translation'],wanted['translation']))
    scale=max(abs(a-b) for a,b in zip(actual['scale'],wanted['scale']))
    qa,qb=actual['rotation'],wanted['rotation']
    dot=abs(sum(a*b for a,b in zip(qa,qb)))/math.sqrt(sum(a*a for a in qa)*sum(b*b for b in qb))
    angle=math.degrees(2*math.acos(min(1,dot)))
    assert position<1e-6 and scale<1e-6 and angle<.001,(actual['name'],position,scale,angle)
    assert all(actual[k]==ref[k] for k in ('translation','scale'))
    reference_rotation_error=max(abs(a-b) for a,b in zip(qa,ref['rotation']))
    assert reference_rotation_error<=rotation_rounding_limit,(actual['name'],reference_rotation_error)
    assert all(abs(sum(v*v for v in q)-1)<=8*math.ulp(1.0) for q in (qa,ref['rotation']))
    errors.append(dict(name=actual['name'],translation_cm=position,scale=scale,angle_degrees=angle,
        skeleton_rotation_component_error=reference_rotation_error))
# Reflected editor property access, not a live UE4SS raw-array read.
morphs=sorted(str(m.get_name()) for m in mesh.get_editor_property('morph_targets'))
assert morphs==sorted(m['name'] for m in data['morph_targets']) and len(morphs)==22
materials=[str(s.material_slot_name) for s in mesh.get_editor_property('materials')]
assert materials==data['materials'] and len(materials)==30
assert mesh.get_editor_property('post_process_anim_blueprint') is None
assert mesh.get_editor_property('physics_asset') is None
assert all(digest(Path(p))==h for p,h in source['protected_hashes'].items())
assert all(digest(Path(p))==info['sha256'] for p,info in saved.items())
(OUT/'engine-b2-bind.json').write_text(json.dumps(bind,indent=2)+'\n')
(OUT/'readback.json').write_text(json.dumps(dict(passed=True,mesh=data['mesh_package'],skeleton=data['skeleton_package'],
    morphs=morphs,materials=materials,bones=len(bind),errors=errors,source_sha256=source['sha256'],
    skeleton_rotation_rounding_limit=rotation_rounding_limit,
    saved_assets=saved,protected_hashes=source['protected_hashes'],
    scope='Full diagnostic B2 import and fresh readback, with placeholder materials and no physics/post-process binding. Not game Skeleton compatibility, cooked GPU morph execution or deployment.'),indent=2)+'\n')
unreal.log('CSS_B2_FULL_IMPORT_READBACK_PASSED')
