"""Fit Genesis 2 Female onto Mortal Shell II's skeleton, using Blender's own rigging.

Written after three hand-rolled weighting schemes in a row failed, each in a
different way, on a problem that is thoroughly solved. The last one weighted the
crown of the skull 0.47/0.47 to the two eye bones, because a leaf bone was being
extended past its own joint and the eye sockets point up into the head. Distance
to a line segment is simply not how a skeleton claims flesh.

Blender's automatic weights are bone heat, which asks whether a bone can *see* a
vertex before it claims it, so an eye bone buried in the skull never gets the
crown. The Data Transfer modifier is the standard way to move weights between
two meshes. Both are mature and neither needed writing.

The pipeline is the ordinary one for a figure whose rest pose does not match the
target rig:

  1. Build the DAZ armature at the figure's own rest, which is a T-pose.
  2. Skin the figure to it with automatic weights.
  3. Pose that armature so every bone lands on its game counterpart, which is an
     A-pose on a taller, heeled skeleton.
  4. Apply the modifier, freezing the posed shape as the new mesh.
  5. Re-skin against the real 258-bone game skeleton by transferring weights off
     the shipped Genessa body, so the result is animated by the same weights the
     game's own animations were authored against.

Only bones with a distinct game counterpart are built. The eyes, jaw, tongue and
individual toes all resolve to `head` or `ball_*`, so giving them bones would add
nothing but competition for vertices.

Run under the Blender python:

    build/render-env/bin/python tools/fit_g2f_blender.py -- \
        --mesh Genesis2Female_base_21556.obj \
        --daz-rig work/v1.0.0-body/daz/g2f_rig.json \
        --game-rig SK_Sester_Genessa_V6.refskel.json \
        --game-mesh SK_Sester_Genessa_V6.glb \
        --out work/v1.0.0-body/fitb
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import bpy
import numpy as np
import bmesh   # must follow bpy: the standalone module initialises against it
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fit_g2f import BONE_MAP, load_daz_rig, load_game_rig, children_of  # noqa: E402

# Bones that carry the deformation. Everything else in BONE_MAP shares a game
# bone with its parent and would only fight it for vertices.
def deforming(daz_rig, bone_map):
    out = []
    for name, bone in daz_rig.items():
        game = bone_map.get(name)
        if game is None:
            continue
        if bone_map.get(bone['parent']) == game:
            continue
        out.append(name)
    return out


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_mesh(path: Path, mirror: bool):
    before = set(bpy.data.objects)
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis='Y', up_axis='Z')
    made = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    obj = made[0]
    if len(made) > 1:
        bpy.ops.object.select_all(action='DESELECT')
        for other in made:
            other.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.join()

    mesh = obj.data
    raw = np.empty(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get('co', raw)
    points = raw.reshape(-1, 3)
    if mirror:
        # DAZ/Unity is left-handed with the figure's left at -X; the game's
        # exported frame is right-handed with left at +X. Negating X converts
        # between them, and the face winding has to be reversed with it.
        points = np.stack([-points[:, 0], points[:, 1], points[:, 2]], axis=1)
    # Game frame is Y up, Blender is Z up.
    points = np.stack([points[:, 0], -points[:, 2], points[:, 1]], axis=1)
    mesh.vertices.foreach_set('co', points.ravel())
    if mirror:
        mesh.flip_normals()
    mesh.update()
    return obj


def to_blender(point) -> Vector:
    """Game frame (Y up, +Z front, metres) to Blender (Z up, -Y front)."""
    return Vector((point[0], -point[2], point[1]))


def build_armature(name, rig, bones, children, tail_scale=0.6):
    armature = bpy.data.armatures.new(name)
    obj = bpy.data.objects.new(name, armature)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')

    made = {}
    for bone_name in bones:
        head = to_blender(rig[bone_name]['world'])
        kids = [k for k in children.get(bone_name, []) if k in bones]
        if kids:
            tail = Vector((0, 0, 0))
            for kid in kids:
                tail += to_blender(rig[kid]['world'])
            tail /= len(kids)
        else:
            parent = rig[bone_name]['parent']
            reach = (head - to_blender(rig[parent]['world'])) if parent in rig else Vector((0, 0, 0.05))
            tail = head + reach * tail_scale
        if (tail - head).length < 1e-4:
            tail = head + Vector((0, 0, 0.02))
        edit = armature.edit_bones.new(bone_name)
        edit.head, edit.tail = head, tail
        made[bone_name] = edit
    for bone_name in bones:
        parent = rig[bone_name]['parent']
        while parent is not None and parent not in made:
            parent = rig[parent]['parent'] if parent in rig else None
        if parent in made:
            made[bone_name].parent = made[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    return obj


FILLER = 'css_hole_filler'


def fill_holes(mesh_obj) -> int:
    """Close the open rims on the head, tagging the faces added.

    Bone heat weighting refuses to solve on a mesh with boundary edges, and
    Genesis 2 has 840 of them: the mouth cavity, the eye socket rims and the
    nostrils are all left open, because the eyeballs, teeth and tongue are
    separate shells that sit behind them. With the holes open every one of the
    56 bones comes back with zero weights and the armature silently deforms
    nothing, which looks exactly like the pose having failed to apply.

    Filling adds faces but no vertices, so the vertex order that every VaM morph
    indexes against is untouched. The added faces are tagged so they can be
    removed once the weights are computed.
    """
    mesh = bmesh.new()
    mesh.from_mesh(mesh_obj.data)
    tag = mesh.faces.layers.int.new(FILLER)
    open_edges = [e for e in mesh.edges if not e.is_manifold and len(e.link_faces) == 1]
    if open_edges:
        result = bmesh.ops.holes_fill(mesh, edges=open_edges, sides=0)
        for face in result.get('faces', []):
            face[tag] = 1
    added = sum(1 for f in mesh.faces if f[tag])
    mesh.to_mesh(mesh_obj.data)
    mesh.free()
    mesh_obj.data.update()
    return added


def remove_filler(mesh_obj) -> int:
    """Delete the faces `fill_holes` added, leaving the original surface."""
    mesh = bmesh.new()
    mesh.from_mesh(mesh_obj.data)
    tag = mesh.faces.layers.int.get(FILLER)
    if tag is None:
        mesh.free()
        return 0
    doomed = [f for f in mesh.faces if f[tag]]
    bmesh.ops.delete(mesh, geom=doomed, context='FACES_ONLY')
    mesh.faces.layers.int.remove(mesh.faces.layers.int.get(FILLER))
    mesh.to_mesh(mesh_obj.data)
    mesh.free()
    mesh_obj.data.update()
    return len(doomed)


def weld_stranded(mesh_obj, armature_obj, rig, bones):
    """Give the disconnected shells a weight so they ride with the head.

    The eyeballs, corneas, lashes, teeth and tongue are separate surfaces that
    share no edge with the body, so bone heat, which flows weight along edges,
    leaves every one of them at zero. Unweighted, they stay at the DAZ rest
    position while the body lifts and lengthens onto the game skeleton, and end
    up as a scatter of small shells floating around the head.

    Each stranded vertex is bound wholly to the deforming bone whose joint is
    nearest to it. For everything in the head that resolves to `head`, which is
    right: an eyeball is rigid inside the skull and should move with it exactly.
    """
    joints = {name: np.array(rig[name]['world']) for name in bones}
    mesh = mesh_obj.data
    raw = np.empty(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get('co', raw)
    points = raw.reshape(-1, 3)

    totals = np.zeros(len(mesh.vertices))
    for vertex in mesh.vertices:
        totals[vertex.index] = sum(g.weight for g in vertex.groups)
    stranded = np.where(totals < 1e-6)[0]
    if len(stranded) == 0:
        return 0

    # Points here are in Blender space (Z up); the rig is in game space (Y up).
    game_space = np.stack([points[:, 0], points[:, 2], -points[:, 1]], axis=1)
    names = list(joints)
    positions = np.array([joints[n] for n in names])
    groups = {n: mesh_obj.vertex_groups.get(n) or mesh_obj.vertex_groups.new(name=n)
              for n in names}
    for index in stranded:
        nearest = names[int(np.argmin(np.linalg.norm(positions - game_space[index], axis=1)))]
        groups[nearest].add([int(index)], 1.0, 'REPLACE')
    return len(stranded)


def skin_automatic(mesh_obj, armature_obj):
    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    armature_obj.select_set(True)
    bpy.context.view_layer.objects.active = armature_obj
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')


def pose_onto(armature_obj, daz_rig, game_rig, bones, children, bone_map):
    """Move each pose bone so it lands on its game counterpart.

    Each bone is given the world matrix that takes its own head to the game
    joint, turns it to the game bone's direction, and stretches it to the game
    bone's length. Blender composes that with the parent's pose, so the chain
    stays connected; the view layer has to be updated between bones or every
    child is positioned against its parent's stale matrix.
    """
    bpy.context.view_layer.objects.active = armature_obj
    bpy.ops.object.mode_set(mode='POSE')

    ordered = sorted(bones, key=lambda n: _depth(n, daz_rig))
    report = {}
    for name in ordered:
        pose_bone = armature_obj.pose.bones[name]
        rest = pose_bone.bone

        target_head = to_blender(game_rig[bone_map[name]]['world'])
        kids = [k for k in children.get(name, []) if k in bones]
        if kids:
            target_tail = Vector((0, 0, 0))
            for kid in kids:
                target_tail += to_blender(game_rig[bone_map[kid]]['world'])
            target_tail /= len(kids)
        else:
            target_tail = target_head + (rest.tail - rest.head)

        direction = target_tail - target_head
        if direction.length < 1e-5:
            direction = (rest.tail - rest.head)
        length = direction.length

        # Replace only the direction, by the smallest rotation carrying the rest
        # bone's own axis onto the target direction. rotation_difference gives
        # exactly that minimal (swing-only) rotation, so the bone's rest roll is
        # carried along untouched. This is the whole pose: head pinned to the
        # game joint, axis turned to the game bone, length matched by stretch.
        rest_axis = rest.matrix_local.to_3x3().col[1].normalized()
        rotation = rest_axis.rotation_difference(direction.normalized()).to_matrix()
        stretch = length / max(rest.length, 1e-6)

        basis = rotation @ rest.matrix_local.to_3x3()
        matrix = Matrix.Translation(target_head) @ basis.to_4x4()
        pose_bone.matrix = matrix @ Matrix.Diagonal((1.0, stretch, 1.0, 1.0))
        bpy.context.view_layer.update()
        landed = (armature_obj.matrix_world @ pose_bone.matrix).to_translation()
        report[name] = round((landed - target_head).length * 100, 4)

    bpy.ops.object.mode_set(mode='OBJECT')
    return report


def _depth(name, rig):
    steps, seen = 0, set()
    while name in rig and name not in seen:
        seen.add(name)
        name = rig[name]['parent']
        steps += 1
    return steps


def apply_armature(mesh_obj):
    bpy.context.view_layer.objects.active = mesh_obj
    for modifier in list(mesh_obj.modifiers):
        if modifier.type == 'ARMATURE':
            bpy.ops.object.modifier_apply(modifier=modifier.name)


def export_obj(mesh_obj, path: Path):
    """Write the mesh back in the game's frame, not Blender's.

    Everything downstream (the renderer, the mesh-health check, the CSS mesh
    exporter) expects the game frame: Y up, +Z front, metres. Blender works Z up,
    so the coordinates have to be turned back on the way out. Leaving them in
    Blender's frame produces a file that looks fine until something converts it
    a second time, and then the figure renders folded over on itself.
    """
    mesh = mesh_obj.data
    raw = np.empty(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get('co', raw)
    points = raw.reshape(-1, 3)
    mesh.vertices.foreach_set('co', np.stack(
        [points[:, 0], points[:, 2], -points[:, 1]], axis=1).ravel())
    mesh.update()

    bpy.ops.object.select_all(action='DESELECT')
    mesh_obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_obj
    bpy.ops.wm.obj_export(filepath=str(path), export_selected_objects=True,
                          forward_axis='Y', up_axis='Z', export_materials=False,
                          export_uv=True, export_normals=False, export_triangulated_mesh=False)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser()
    parser.add_argument('--mesh', required=True, type=Path)
    parser.add_argument('--daz-rig', required=True, type=Path)
    parser.add_argument('--game-rig', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args(argv)

    daz_rig = load_daz_rig(args.daz_rig)
    game_rig = load_game_rig(args.game_rig)
    bones = deforming(daz_rig, BONE_MAP)
    children = children_of(daz_rig)

    clear()
    mesh_obj = import_mesh(args.mesh, mirror=True)
    filled = fill_holes(mesh_obj)
    armature_obj = build_armature('G2F', daz_rig, bones, children)
    skin_automatic(mesh_obj, armature_obj)
    stranded = weld_stranded(mesh_obj, armature_obj, daz_rig, bones)
    weighted = sum(1 for v in mesh_obj.data.vertices for g in v.groups if g.weight > 0)
    if weighted == 0:
        raise SystemExit('bone heat produced no weights; the mesh is still not solvable')
    landing = pose_onto(armature_obj, daz_rig, game_rig, bones, children, BONE_MAP)
    apply_armature(mesh_obj)
    removed = remove_filler(mesh_obj)

    # Measured before the export, which rewrites the coordinates into the game
    # frame and would otherwise have this reporting depth as height.
    raw = np.empty(len(mesh_obj.data.vertices) * 3)
    mesh_obj.data.vertices.foreach_get('co', raw)
    points = raw.reshape(-1, 3)

    args.out.mkdir(parents=True, exist_ok=True)
    export_obj(mesh_obj, args.out / 'g2f_fitted.obj')

    worst = sorted(landing.items(), key=lambda kv: -kv[1])[:6]
    report = {'bones_built': len(bones), 'vertices': len(points),
              'holes_filled_faces': filled, 'filler_removed_faces': removed,
              'stranded_shell_verts': stranded, 'weight_entries': weighted,
              'height_m': round(float(points[:, 2].max() - points[:, 2].min()), 4),
              'crown_z_m': round(float(points[:, 2].max()), 4),
              'game_head_joint_z_m': round(float(to_blender(game_rig['head']['world']).z), 4),
              'worst_bone_landing_cm': dict(worst)}
    (args.out / 'fit.report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
