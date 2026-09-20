"""Select the game human collision template, preserving its source and indices.

This writes an unfitted diagnostic input, not a Physics Asset or game change.
"""
import collections
import copy
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
WORK=ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT=WORK/'b2-body-physics-reference-v1'
OUT.mkdir(exist_ok=True);assert not (OUT/'report.json').exists()
load=lambda p:json.loads(p.read_text())
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
source_path=WORK/'combat-physics-decode-v1/decoded/PA_Sester_Genessa_V5_Capsules_NoCloth.json'
assert load(WORK/'combat-physics-decode-v1/exit.json')['exit_code']==0
source=load(source_path)
asset=next(x for x in source if x['Type']=='PhysicsAsset')
by_name={x['Name']:x for x in source}
def resolve(ref):
    # Owned exports use the object name, not the package's numeric export index.
    name=ref['ObjectName'].split(':')[-1].rstrip("'")
    return by_name[name]
body_refs=asset['Properties']['SkeletalBodySetups']
bodies=[resolve(x) for x in body_refs]
assert len(bodies)==60 and all(x['Type']=='SkeletalBodySetup' for x in bodies)
core={'pelvis','spine_02','spine_03','spine_04','spine_05','neck_01','neck_02','head'}
for side in ('l','r'):
    core.update(f'{name}_{side}' for name in ('clavicle','upperarm','lowerarm','hand','thigh','calf','foot'))
assert len(core)==22
selected=[(i,x) for i,x in enumerate(bodies) if x['Properties']['BoneName'].lower() in core]
assert {x['Properties']['BoneName'].lower() for _,x in selected}==core
assert all(x['Properties'].get('PhysicsType','EPhysicsType::PhysType_Default')=='EPhysicsType::PhysType_Default' for _,x in selected)
removed=[x for x in bodies if x['Properties']['BoneName'].lower() not in core]
assert len(removed)==38 and all(x['Properties']['BoneName'].startswith('GenessaCloth_') for x in removed)
assert sum(x['Properties'].get('PhysicsType')=='EPhysicsType::PhysType_Simulated' for x in removed)==25
index_map={old:new for new,(old,_) in enumerate(selected)}
constraints=[];discarded_constraints=[]
for ref in asset['Properties']['ConstraintSetup']:
    row=resolve(ref);assert row['Type']=='PhysicsConstraintTemplate'
    constraint=row['Properties']['DefaultInstance']
    pair=[constraint[n].lower() for n in ('ConstraintBone1','ConstraintBone2')]
    if all(n in core for n in pair):constraints.append(copy.deepcopy(row['Properties']))
    else:discarded_constraints.append(row['Name'])
assert len(constraints)==21 and len(discarded_constraints)==38
pairs=[]
for row in asset['CollisionDisableTable']:
    a,b=row['Key']['Indices']
    if a in index_map and b in index_map:
        pairs.append(dict(indices=[index_map[a],index_map[b]],value=row['Value']))
assert all(0<=i<22 for row in pairs for i in row['indices'])
assert len({tuple(sorted(row['indices'])) for row in pairs})==len(pairs)
bind_path=WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json'
bind=load(bind_path);names={b['name'].lower():b for b in bind}
assert core<=set(names)
shape_counts=collections.Counter()
for _,body in selected:
    for kind,shapes in body['Properties']['AggGeom'].items():
        assert kind in ('SphylElems','BoxElems','TaperedCapsuleElems'),kind
        shape_counts[kind]+=len(shapes)
        for shape in shapes:
            assert shape['CollisionEnabled']=='ECollisionEnabled::QueryAndPhysics'
            for key in ('Radius','Radius0','Radius1','Length','X','Y','Z'):
                if key in shape:
                    # Capsule Length excludes its round ends; zero is valid.
                    assert shape[key]>=0 if key=='Length' else shape[key]>0,(body['Name'],key)
input_data=dict(scope=__doc__,fit_verified=False,source_physics_package=asset['Package'],
    bodies=[copy.deepcopy(x['Properties']) for _,x in selected],constraints=constraints,
    bounds_bodies=[index_map[i] for i in asset['Properties']['BoundsBodies'] if i in index_map],
    collision_disable_table=pairs,solver_settings=copy.deepcopy(asset['Properties'].get('SolverSettings',{})),
    target_bones={n:names[n] for n in sorted(core)})
(OUT/'template.json').write_text(json.dumps(input_data,indent=2)+'\n')
report=dict(passed=True,source_sha256=digest(source_path),target_bind_sha256=digest(bind_path),
    original_bodies=60,selected_bodies=22,removed_cloth_bodies=38,removed_explicitly_simulated_bodies=25,
    selected_constraints=21,collision_disabled_pairs=len(pairs),shapes=dict(shape_counts),
    all_bone_names_present=True,body_properties_unchanged=True,fit_verified=False,
    next='Fit collider geometry and constraint frames to the preserved B2 body, then verify real queries and gameplay. No game change occurred.')
(OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
