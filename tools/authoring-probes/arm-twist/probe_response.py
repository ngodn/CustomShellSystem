"""Read-only Blender 5.2.2 probe of source and sampled game arm twist."""
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix, Quaternion, Vector

import os
OUT = Path(os.environ['CSS_ARM_TWIST_AUDIT_DIR']).resolve()
if OUT.parent != Path(__file__).resolve().parents[3] / 'work/grip-grounding-v1':
    raise ValueError('Audit directory must be a direct workspace grip-grounding child')
ROOT = OUT.parents[3]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import read_bones

source = MOD / 'reference/body-type-variant-EVE/eve_beta10.blend'
target = MOD / 'work/CSS_SeduXtress_HandBindV43.blend'
hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (source, target)}
bpy.ops.wm.open_mainfile(filepath=str(source))
rig = bpy.data.objects['Eve Armature']
properties = {k: str(rig[k]) for k in rig.keys() if any(x in k for x in ('ArmIk', 'ForearmFollow', 'ArmStretch'))}
for side in ('L', 'R'):
    rig[f'MhaArmIk_{side}'] = 0.0
    rig[f'MhaArmStretch_{side}'] = 0.0
rig.update_tag()
bpy.context.view_layer.update()
names = [f'{part}.{kind}.twk.L' for part in ('upper_arm', 'forearm') for kind in ('bend', 'twist')] + ['hand.twk.L']
original_basis = {p.name: p.matrix_basis.copy() for p in rig.pose.bones}
neutral = {n: rig.pose.bones[n].matrix.copy() for n in names}
source_cases = []
for control in ('upper_arm.fk.L', 'forearm.fk.L', 'hand.fk.L'):
    for degrees in (-60, 60):
        for pb in rig.pose.bones:
            pb.matrix_basis = original_basis[pb.name]
        pb = rig.pose.bones[control]
        pb.matrix_basis = original_basis[control] @ Quaternion((0, 1, 0), math.radians(degrees)).to_matrix().to_4x4()
        rig.update_tag()
        bpy.context.view_layer.update()
        rows = []
        for name in names:
            posed = rig.pose.bones[name].matrix.copy()
            delta = posed @ neutral[name].inverted()
            q = delta.to_quaternion()
            rows.append(dict(name=name, angle_deg=math.degrees(q.angle), axis=list(q.axis),
                             head_shift_cm=(posed.translation-neutral[name].translation).length*100))
        source_cases.append(dict(control=control, degrees=degrees, response=rows))

bones, bind = read_bones(MOD / 'work/CSS_SeduXtress_HandBindV43.bindpose.json')
index = {b['name'].lower(): i for i, b in enumerate(bones)}
game_cases = []
fixture = OUT.parent / 'game-foundation-v42-bind-v2'
for sample in range(5):
    path = fixture / f'foundation_css_modes-s{sample}-compressed-ik1.json'
    snap = json.loads(path.read_text())['pose']['Snapshot']
    lookup = {n.lower(): t for n, t in zip(snap['BoneNames'], snap['LocalTransforms'], strict=True)}
    world = []
    for b in bones:
        t = lookup[b['name'].lower()]
        local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']), Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
        world.append(world[b['parent']] @ local if b['parent'] >= 0 else local)
    rows = []
    for side in ('l', 'r'):
        for part, child in (('upperarm', 'lowerarm'), ('lowerarm', 'hand')):
            parent = index[f'{part}_{side}']
            endpoint = index[f'{child}_{side}']
            skin_parent = world[parent] @ bind[parent].inverted()
            length = (bind[endpoint].translation-bind[parent].translation).length
            direction = (bind[endpoint].translation-bind[parent].translation).normalized()
            for name in [f'{part}_twist_{n:02}_{side}' for n in (1, 2)] + [f'{child}_{side}']:
                i = index[name]
                skin = world[i] @ bind[i].inverted()
                delta = skin_parent.to_quaternion().inverted() @ skin.to_quaternion()
                offset = bind[i].translation-bind[parent].translation
                rows.append(dict(name=name, relative_skin_angle_deg=math.degrees(delta.angle),
                                 relative_skin_axis=list(delta.axis), bind_fraction=offset.dot(direction)/length,
                                 bind_axis_error_cm=(offset-direction*offset.dot(direction)).length))
    game_cases.append(dict(sample=sample, source=str(path), response=rows))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in hashes.items())
result = dict(source_properties=properties, source_cases=source_cases, game_cases=game_cases,
              input_hashes=hashes, files_unchanged=True, blender_version=bpy.app.version_string,
              python_version=sys.version, scope='Read-only response probe. Game samples are the bounded H2 fixture, not full gameplay.')
(OUT / 'twist-response.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result), flush=True)
