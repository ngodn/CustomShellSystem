"""Read-only V44B2 actual Blender skin evaluator for hand diagnostics.

Blender 5.2.2 / Python 3.13. Captured rotations are retained while local
translations come from B2. This does not replay the full game animation graph.
"""
import hashlib
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

ROOT = Path(__file__).resolve().parents[4]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
AUDIT = MOD / 'work/nextgen-audit'
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(MOD / 'tools'))
from export_seduxtress_eve import read_bones, TO_UE, EXPORT_SHAPES
from hand_skin_intersections import audit


def load(path):
    return json.loads(path.read_text())


def pair_set(result):
    return {tuple(sorted(tuple(sorted(t)) for t in p['triangles'])) for p in result['pairs']}


class SkinFixture:
    def __init__(self):
        self.blend = MOD / 'work/CSS_SeduXtress_ArmRestV44B2.blend'
        self.source_hash = hashlib.sha256(self.blend.read_bytes()).hexdigest()
        bpy.ops.wm.open_mainfile(filepath=str(self.blend), use_scripts=False)
        self.body = bpy.data.objects['Eve Body']
        self.rig = bpy.data.objects['SKEL_CSS_Base']
        self.bones, self.bind = read_bones(MOD / self.rig['CSS_bind_pose'])
        self.index = {b['name'].lower(): i for i, b in enumerate(self.bones)}
        assert len(self.bones) == 379
        for obj in (self.body, self.rig):
            obj.hide_viewport = False
            obj.hide_set(False)
        self.rig.animation_data_clear()
        for pb in self.rig.pose.bones:
            assert not pb.constraints
            pb.matrix_basis = Matrix.Identity(4)
        for modifier in list(self.body.modifiers):
            if modifier.type != 'ARMATURE':
                self.body.modifiers.remove(modifier)
            else:
                modifier.use_deform_preserve_volume = False
        self.keys = self.body.data.shape_keys.key_blocks
        self.body.data.shape_keys.animation_data_clear()
        self.defaults = {key.name: key.value for key in self.keys}
        self.basis = TO_UE @ self.rig.matrix_world
        self.curves = load(HERE / 'left-finger-correctives-v1.json')['parameters']

    def source_rotations(self, doc):
        transforms = doc['pose']['Snapshot']['LocalTransforms']
        return {m['shape']: (Quaternion(m['left_wxyz']) @
                Quaternion([transforms[m['index']]['Rotation'][k] for k in 'WXYZ']) @
                Quaternion(m['right_wxyz'])).normalized() for m in self.curves}

    def shapes(self, doc):
        rotations = self.source_rotations(doc)
        return {m['shape']: max(0.0, min(1.0, m['coefficient'] *
                rotations[m['shape']].to_euler(m['euler_order'])['XYZ'.index(m['axis'])]))
                for m in self.curves}

    def evaluate(self, doc, shapes=None, *, batch_pose=False):
        snapshot = doc['pose']['Snapshot']
        assert snapshot['bIsValid']
        assert [n.lower() for n in snapshot['BoneNames'][:379]] == list(self.index)
        world = []
        for bone, transform in zip(self.bones, snapshot['LocalTransforms'][:379], strict=True):
            q = Quaternion([transform['Rotation'][k] for k in 'WXYZ']).normalized()
            local = Matrix.LocRotScale(Vector(bone['translation']), q,
                                       Vector([transform['Scale3D'][k] for k in 'XYZ']))
            world.append(world[bone['parent']] @ local if bone['parent'] >= 0 else local)
        desired = {}
        for bone, rest, posed in zip(self.bones, self.bind, world, strict=True):
            pb = self.rig.pose.bones[bone['name']]
            target = self.basis.inverted() @ posed @ rest.inverted() @ self.basis @ pb.bone.matrix_local
            desired[pb.name] = target
            if batch_pose:
                # Blender's documented conversion supports supplied parent
                # matrices, avoiding a dependency-graph update for every bone.
                kwargs = dict(parent_matrix=desired[pb.parent.name],
                              parent_matrix_local=pb.parent.bone.matrix_local) if pb.parent else {}
                pb.matrix_basis = pb.bone.convert_local_to_pose(target, pb.bone.matrix_local,
                                                               invert=True, **kwargs)
            else:
                pb.matrix = target
                bpy.context.view_layer.update()
        values = self.shapes(doc) if shapes is None else shapes
        for key in self.keys:
            key.value = 0 if key.name in EXPORT_SHAPES else values.get(key.name, self.defaults[key.name])
        bpy.context.view_layer.update()
        evaluated = self.body.evaluated_get(bpy.context.evaluated_depsgraph_get())
        mesh = evaluated.to_mesh()
        result = audit(self.body, mesh, 'l')
        self.last_positions = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
        mesh.vertices.foreach_get('co', self.last_positions)
        self.last_positions = self.last_positions.reshape((-1, 3))
        evaluated.to_mesh_clear()
        return result

    def assert_unchanged(self):
        assert hashlib.sha256(self.blend.read_bytes()).hexdigest() == self.source_hash
