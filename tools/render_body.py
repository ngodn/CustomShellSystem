"""Render a body OBJ for review: full figure plus anatomy close-ups.

Run under the Blender python:

    build/render-env/bin/python tools/render_body.py -- \
        --obj work/v1.0.0-body/target/torso.obj \
        --audit .../SeductressV2_Body.audit.json \
        --out work/v1.0.0-body/renders/before

Matte clay with a hard key light, because the question these renders answer is
"what shape is this", not "how does the skin look". A glossy or textured render
hides exactly the millimetre-scale relief that matters for an areola, and the
current body's problem is shape rather than shading.

Close-ups frame each landmark from slightly off its own normal, so the silhouette
of the relief reads rather than being pointed at head-on.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_obj(path: Path):
    """Import without axis conversion.

    Blender's OBJ importer defaults to the Y-up convention and rotates the mesh
    on the way in. These OBJs are written from Blender world space already, so
    that rotation puts the geometry somewhere the audit's landmarks do not point,
    and every close-up renders empty background. Forward Y / up Z is the identity.
    """
    before = set(bpy.data.objects)
    try:
        bpy.ops.wm.obj_import(filepath=str(path), forward_axis='Y', up_axis='Z')
    except (AttributeError, TypeError):
        bpy.ops.import_scene.obj(filepath=str(path), axis_forward='Y', axis_up='Z')
    made = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    if not made:
        raise SystemExit(f'nothing imported from {path}')
    if len(made) > 1:
        bpy.ops.object.select_all(action='DESELECT')
        for o in made:
            o.select_set(True)
        bpy.context.view_layer.objects.active = made[0]
        bpy.ops.object.join()
        made = [bpy.context.view_layer.objects.active]
    return made[0]


def clay(obj, shade_smooth=True):
    material = bpy.data.materials.new('Clay')
    material.use_nodes = True
    bsdf = material.node_tree.nodes['Principled BSDF']
    bsdf.inputs['Base Color'].default_value = (0.62, 0.60, 0.58, 1)
    bsdf.inputs['Roughness'].default_value = 0.62
    bsdf.inputs['Specular IOR Level'].default_value = 0.25
    obj.data.materials.clear()
    obj.data.materials.append(material)
    if shade_smooth:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True


def light_rig(target: Vector, size: float):
    for name, offset, energy in (
        ('key', Vector((1.0, -1.4, 0.9)), 1.0),
        ('fill', Vector((-1.2, -0.9, 0.1)), 0.32),
        ('rim', Vector((0.2, 1.3, 0.6)), 0.55),
    ):
        data = bpy.data.lights.new(name, type='AREA')
        data.energy = energy * size * size * 900
        data.size = size * 1.4
        lamp = bpy.data.objects.new(name, data)
        bpy.context.collection.objects.link(lamp)
        lamp.location = target + offset * size * 3.0
        direction = (target - lamp.location).normalized()
        lamp.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    world = bpy.data.worlds.new('W')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (0.05, 0.055, 0.065, 1)
    bpy.context.scene.world = world


def camera(location: Vector, look_at: Vector, lens=85.0):
    data = bpy.data.cameras.new('cam')
    data.lens = lens
    data.clip_start = 0.001        # close-ups sit ~12 cm from the surface
    cam = bpy.data.objects.new('cam', data)
    bpy.context.collection.objects.link(cam)
    cam.location = location
    cam.rotation_euler = (look_at - location).normalized().to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = cam
    return cam


def render(path: Path, width=900, height=1200, samples=48):
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    try:
        scene.cycles.device = 'GPU'
    except Exception:
        pass
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.resolution_x = width
    scene.render.resolution_y = height
    scene.render.film_transparent = False
    scene.render.filepath = str(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser()
    parser.add_argument('--obj', required=True, type=Path)
    parser.add_argument('--audit', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--samples', type=int, default=48)
    parser.add_argument('--label', default='')
    args = parser.parse_args(argv)

    clear()
    obj = import_obj(args.obj)
    clay(obj)

    coords = np.array([obj.matrix_world @ v.co for v in obj.data.vertices])
    centre = Vector(coords.mean(0))
    size = float(np.linalg.norm(coords.max(0) - coords.min(0)))
    light_rig(centre, size)

    # Full figure, front three-quarter.
    camera(centre + Vector((size * 0.55, -size * 1.35, size * 0.12)), centre, lens=80)
    render(args.out / 'full.png', 900, 1200, args.samples)
    print(f'wrote {args.out}/full.png')

    if not args.audit or not args.audit.exists():
        return 0

    audit = json.loads(args.audit.read_text())
    for fill in audit.get('fills', []):
        label = fill.get('label')
        if label not in ('nipple_left', 'nipple_right', 'vulva'):
            continue
        point = Vector(fill['center'])
        normal = Vector(fill['normal']).normalized()
        # Off-axis so relief shows as silhouette rather than flat-on.
        side = normal.cross(Vector((0, 0, 1)))
        side = side.normalized() if side.length > 1e-6 else Vector((1, 0, 0))
        offset = (normal * math.cos(math.radians(55)) + side * math.sin(math.radians(55)))
        camera(point + offset * 0.12 + Vector((0, 0, 0.012)), point, lens=110)
        render(args.out / f'{label}.png', 700, 700, args.samples)
        print(f'wrote {args.out}/{label}.png')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
