"""Fit compact, fixed collision shapes around all public morph corners.

Preserves bones, primitive types, constraint settings and original mesh data.
This is reference-pose geometry fitting, not gameplay or ragdoll acceptance.
"""
import copy
import hashlib
import itertools
import json
import os
import sys
from pathlib import Path

import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT/'CustomShellSystem/work/grip-grounding-v1'
OUT = Path(os.environ['CSS_BODY_REFINED_FIT_DIR']).resolve()
assert OUT.parent == WORK.resolve() and not OUT.exists()
OUT.mkdir()
sys.path.insert(0, str(Path(__file__).parent))
sys.path.insert(0, str(ROOT/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'))
from export_seduxtress_eve import read_bones
from body_physics_geometry import PUBLIC_MORPHS, skin_regions, sdf, rotator, to_rotator, vector, xyz, volume

source_path = WORK/'arm-rest-correctives-export-v1/candidate.mesh.json'
bind_path = WORK/'arm-rest-b2-full-import-v1/engine-b2-bind.json'
baseline_path = WORK/'b2-body-physics-fit-v4/candidate.json'
template_path = WORK/'b2-body-physics-reference-v1/template.json'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
hashes = {str(p): digest(p) for p in (source_path, bind_path, baseline_path, template_path)}
assert hashes[str(source_path)] == 'f49fd4e69a554f99bb2c309c78688192bd839285db178a9c83e013181b640d8a'
source = json.loads(source_path.read_text())
baseline = json.loads(baseline_path.read_text())
template = json.loads(template_path.read_text())
candidate = copy.deepcopy(baseline)
bones, world = read_bones(bind_path)
ids, regions = skin_regions(source, bones, baseline['bodies'])
by_name = {b['name'].lower(): i for i, b in enumerate(bones)}
points = np.asarray(source['points'], dtype=float)
deltas = np.zeros((6, len(points), 3))
for morph in source['morph_targets']:
    if morph['name'] in PUBLIC_MORPHS:
        values = np.asarray(morph['deltas'])
        deltas[PUBLIC_MORPHS.index(morph['name']), values[:, 0].astype(int)] = values[:, 1:]
corners = np.asarray(list(itertools.product((0., 1.), repeat=6)))
margin = .35


def fit(shape, kind, cloud, rotation):
    # Fit against the rotation actually serialized as an Unreal Rotator.
    result = copy.deepcopy(shape)
    result['Rotation'] = to_rotator(Matrix(rotation.tolist()).to_quaternion())
    rotation = np.asarray(rotator(result['Rotation']).to_matrix(), dtype=float)
    local = cloud@rotation
    center = (local.min(axis=0)+local.max(axis=0))/2
    q = local-center
    result['Center'] = vector(rotation@center)
    if kind == 'BoxElems':
        for axis, size in zip('XYZ', local.max(axis=0)-local.min(axis=0)+2*margin):
            result[axis] = float(size)
    else:
        radial = np.linalg.norm(q[:, :2], axis=1)
        minimum = float(radial.max()+margin)
        maximum = max(minimum, float(np.linalg.norm(q, axis=1).max()+margin))
        choices = []
        for radius in np.linspace(minimum, maximum, 33):
            half = max(0., float(np.max(np.abs(q[:, 2])-np.sqrt(np.maximum(0, radius**2-radial**2)))))+margin
            v = np.pi*radius**2*2*half+4/3*np.pi*radius**3
            choices.append((v, radius, half))
        _, radius, half = min(choices)
        result['Length'] = 2*half
        if kind == 'SphylElems':
            result['Radius'] = float(radius)
        else:
            result['Radius0'] = result['Radius1'] = float(radius)
    assert float(sdf(result, kind, cloud).max()) < .0001
    return result


def basis(direction, reference):
    z = direction/np.linalg.norm(direction)
    x = reference[:, 0]-z*np.dot(z, reference[:, 0])
    if np.linalg.norm(x) < .001:
        x = reference[:, 1]-z*np.dot(z, reference[:, 1])
    x /= np.linalg.norm(x)
    return np.column_stack((x, np.cross(z, x), z))


reports = []
for target, original, previous in zip(candidate['bodies'], template['bodies'], baseline['bodies'], strict=True):
    name = target['BoneName'].lower()
    inverse = np.asarray(world[by_name[name]].inverted(), dtype=float)
    vertices = np.asarray(regions[name])
    neutral = points[vertices]@inverse[:3, :3].T+inverse[:3, 3]
    rows = [(kind, index, shape) for kind, shapes in original['AggGeom'].items() for index, shape in enumerate(shapes)]
    assignment = np.argmin(np.stack([sdf(shape, kind, neutral) for kind, _, shape in rows]), axis=0)
    for part, (kind, index, source_shape) in enumerate(rows):
        selected = vertices[assignment == part]
        assert len(selected) >= 8
        cloud = points[selected]+np.einsum('ci,ivj->cvj', corners, deltas[:, selected])
        cloud = np.unique(cloud.reshape(-1, 3), axis=0)
        cloud = cloud@inverse[:3, :3].T+inverse[:3, 3]
        old_shape = previous['AggGeom'][kind][index]
        old_rotation = np.asarray(rotator(old_shape['Rotation']).to_matrix(), dtype=float)
        old_axis_fit = fit(old_shape, kind, cloud, old_rotation)
        _, axes = np.linalg.eigh(np.cov(cloud.T))
        if np.linalg.det(axes) < 0:
            axes[:, 0] *= -1
        rotations = [old_rotation, axes] if kind == 'BoxElems' else [old_rotation]+[basis(axes[:, a], old_rotation) for a in range(3)]
        options = [fit(old_shape, kind, cloud, r) for r in rotations]
        best = min(options, key=lambda shape: volume(shape, kind))
        # Deterministic local orientation refinement, preserving original shape type.
        for degrees in (10., 5., 2.):
            rotation = np.asarray(rotator(best['Rotation']).to_matrix(), dtype=float)
            options = [best]
            for axis in range(3 if kind == 'BoxElems' else 2):
                for sign in (-1, 1):
                    change = Matrix.Rotation(np.radians(sign*degrees), 3, 'XYZ'[axis])
                    options.append(fit(old_shape, kind, cloud, rotation@np.asarray(change, dtype=float)))
            best = min(options, key=lambda shape: volume(shape, kind))
        target['AggGeom'][kind][index] = best
        reports.append(dict(bone=name, kind=kind, index=index, neutral_vertices=len(selected),
            corner_cloud_points=len(cloud), neutral_volume_cm3=volume(old_shape, kind),
            original_axis_morph_volume_cm3=volume(old_axis_fit, kind), refined_volume_cm3=volume(best, kind),
            maximum_outside_cm=max(0., float(sdf(best, kind, cloud).max())), old=old_shape, new=best))
        print(name, kind, round(volume(best, kind)/volume(old_axis_fit, kind), 4), flush=True)

assert candidate['constraints'] == baseline['constraints']
assert candidate['collision_disable_table'] == baseline['collision_disable_table']
queries = []
for body in candidate['bodies']:
    name = body['BoneName'].lower()
    matrix = world[by_name[name]]
    for kind, shapes in body['AggGeom'].items():
        for index, shape in enumerate(shapes):
            center = Vector(xyz(shape['Center']))
            rotation = rotator(shape['Rotation'])
            for axis in range(3):
                direction = rotation@Vector([100. if a == axis else 0. for a in range(3)])
                start, end = matrix@(center-direction), matrix@(center+direction)
                key = f'{name}/{kind}/{index}/{axis}'
                queries.append(dict(id=key, start=list(start), end=list(end), expected=True))
                offset = Vector((1000, 1000, 1000))
                queries.append(dict(id=key+'/miss', start=list(start+offset), end=list(end+offset), expected=False))
# Include the retained independent neutral skin rays in the engine fixture.
queries.extend(q for q in json.loads((baseline_path.parent/'queries.json').read_text()) if '/skin/' in q['id'])
candidate.update(scope=__doc__, public_morph_corner_fit=True, morph_names=PUBLIC_MORPHS,
                 morph_range=[0., 1.], fit_verified=False)
(OUT/'candidate.json').write_text(json.dumps(candidate, indent=2)+'\n')
(OUT/'queries.json').write_text(json.dumps(queries, indent=2)+'\n')
assert all(digest(Path(p)) == h for p, h in hashes.items())
(OUT/'report.json').write_text(json.dumps(dict(passed=True, protected_hashes=hashes, scope=__doc__,
    margin_cm=margin, corners=64, shapes=reports, skin_vertices=len(ids),
    neutral_volume_cm3=sum(r['neutral_volume_cm3'] for r in reports),
    original_axis_morph_volume_cm3=sum(r['original_axis_morph_volume_cm3'] for r in reports),
    refined_volume_cm3=sum(r['refined_volume_cm3'] for r in reports),
    visual_review_pending=True, physics_queries_verified=False), indent=2)+'\n')
