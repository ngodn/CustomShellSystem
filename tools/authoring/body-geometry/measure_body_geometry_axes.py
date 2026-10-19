#!/usr/bin/env python3
"""Measure the body geometry coefficients for the recipe's solver axes.

Host-side numpy port of the two measuring steps of the SeduXtress authoring
library, for any 6 to 8 axes:

- build_body_morph_layout.py: center offsets (affine) and effective inertia
  (quadratic) per helper, from fixed neutral triangle area times exported
  helper weight;
- fit_body_contact_ellipsoids.py --version 2: per region, the fixed-shape
  ellipsoid (basis and center weights kept from the measured six-axis fit),
  uniformly enlarged to contain the selected core at every one of the 2^n
  morph corners.

    measure_body_geometry_axes.py --recipe RECIPE --export MESH.json
        [--deltas DELTAS.npz] --measured DIR --layout-out L --ellipsoids-out E
        [--report R]

MESH.json is an export_seduxtress_eve.py mesh (points, faces, wedges,
influences, bones, morph_targets) with its .audit.json beside it. Axes the
export does not carry come from DELTAS.npz (the uuepbs authoring archive:
`used` maps export vertices to archive rows, deltas are Blender metres and
are converted to UE centimetres). DIR holds the six-axis measurements this
extends: body-helper-layout-v1.json (neutral offsets and moments, kept
exactly), body-contact-regions-v1.json (bind matrices) and
body-contact-ellipsoids-v2.json (core selection, fixed shape, reference
parent matrices).

Checks before writing: the archive's neutral points match the export, the
core selection reproduces the six-axis neutral centers, and the polynomial
coefficients reproduce direct moments of the morphed points at every corner
and at 64 seeded interiors.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_geometry_axes import NAMES, layout_inputs, read_recipe

TO_UE = np.array([100., -100., 100.])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def load_export(path, deltas_path, axes):
    mesh = json.loads(Path(path).read_text())
    audit = json.loads(Path(path).with_suffix('.audit.json').read_text())
    if audit['parts'][0]['name'] != 'Eve Body':
        raise ValueError('Export does not start with the original body')
    body = audit['parts'][0]['points']
    points = np.asarray(mesh['points'], dtype=np.float64)
    deltas = np.zeros((len(axes), len(points), 3))
    by_name = {m['name']: m['deltas'] for m in mesh['morph_targets']}
    archive = np.load(deltas_path) if deltas_path else None
    used = None
    if archive is not None:
        used = archive['used']
        if len(used) != body:
            raise ValueError('Delta archive does not cover the exported body')
        archived = archive['points_world'][used].astype(np.float64)*TO_UE
        error = float(np.linalg.norm(archived-points[:body], axis=1).max())
        if error > 1e-4:
            raise ValueError(f'Delta archive neutral differs from the export by {error} cm')
    sources = {}
    for i, name in enumerate(axes):
        if name in by_name:
            rows = np.asarray(by_name[name], dtype=np.float64)
            deltas[i, rows[:, 0].astype(int)] = rows[:, 1:]
            sources[name] = 'export'
        elif archive is not None and name in archive.files:
            deltas[i, :body] = archive[name][used].astype(np.float64)*TO_UE
            sources[name] = 'archive'
        else:
            raise ValueError(f'No deltas for solver axis {name}')
    triangles = np.asarray(mesh['wedges'])[np.asarray(mesh['faces'])[:, :3], 0].astype(int)
    triangles = triangles[np.all(triangles < body, axis=1)]
    influences = np.asarray(mesh['influences'])
    bones = [b['name'] for b in mesh['bones']]
    return dict(points=points, deltas=deltas, triangles=triangles, influences=influences, bones=bones,
                body=body, used=used, sources=sources)


def measure_layout(export, layout, regions, axes):
    points, triangles = export['points'], export['triangles']
    p = points[triangles]
    areas = np.linalg.norm(np.cross(p[:, 1]-p[:, 0], p[:, 2]-p[:, 0]), axis=1)/2
    vertex_area = np.zeros(len(points))
    for j in range(3):
        np.add.at(vertex_area, triangles[:, j], areas/3)
    helpers, supports = [], []
    for helper in layout['helpers']:
        index = export['bones'].index(helper['bone'])
        rows = export['influences'][export['influences'][:, 1] == index]
        weights = np.zeros(len(points))
        weights[rows[:, 0].astype(int)] = rows[:, 2]
        mass = vertex_area*weights
        ids = np.flatnonzero(mass > 0)
        mass = mass[ids]/mass.sum()
        bind = np.asarray(regions['bind_matrices'][helper['bone']], dtype=np.float64)
        if not np.isfinite(bind).all() or abs(np.linalg.det(bind[:3, :3])) < 1e-8:
            raise ValueError('Invalid helper bind')
        deltas = export['deltas'][:, ids]
        relative = points[ids]-bind[:3, 3]
        center = mass @ relative
        centers = np.einsum('v,ivc->ic', mass, deltas)
        linear = 4/3*np.einsum('v,vc,ivc->i', mass, relative, deltas) + 2/3*(centers @ center)
        quadratic = 2/3*np.einsum('v,ivc,jvc->ij', mass, deltas, deltas) + 1/3*(centers @ centers.T)
        quadratic = (quadratic+quadratic.T)/2
        helpers.append(dict(bone=helper['bone'], offset_cm=helper['joint_offset_cm'],
            offset_deltas_cm=(centers @ np.linalg.inv(bind[:3, :3]).T).tolist(),
            moment_cm2=helper['isotropic_box_dimension_cm']**2/6+helper['pivot_to_center_cm']**2,
            moment_linear_cm2=linear.tolist(), moment_quadratic_cm2=quadratic.tolist(),
            support_vertices=len(ids), neutral_mass_sum=float(mass.sum())))
        supports.append((ids, mass, bind))
    return dict(morphs=list(axes), helpers=helpers), supports


def review_layout(data, export, supports, axes):
    """Direct moments of the morphed points against the polynomial (build_body_morph_layout --review)."""
    count = len(axes)
    values = [[float(bool(mask & (1 << i))) for i in range(count)] for mask in range(1 << count)]
    values += np.random.default_rng(379).random((64, count)).tolist()
    offset_error = moment_error = 0.
    for morph in values:
        actual = layout_inputs(data, morph)
        for i, (ids, mass, bind) in enumerate(supports):
            moved = export['points'][ids]+np.einsum('i,ivc->vc', morph, export['deltas'][:, ids])
            neutral_center = mass @ export['points'][ids]
            center = mass @ moved
            deviations = moved-center
            trace = float(mass @ np.sum(deviations*deviations, axis=1))
            # Relative to the neutral: the kept neutral offset/moment come from the
            # measured six-axis layout, so compare the morph-driven change only.
            neutral_trace = float(mass @ np.sum((export['points'][ids]-neutral_center)**2, axis=1))
            expected_moment = (2/3*trace+float(np.sum((center-bind[:3, 3])**2)) -
                               (2/3*neutral_trace+float(np.sum((neutral_center-bind[:3, 3])**2))))
            expected_offset = np.linalg.inv(bind[:3, :3]) @ (center-neutral_center)
            moment_error = max(moment_error, abs(actual['Moments'][i]-data['helpers'][i]['moment_cm2']-expected_moment))
            offset_error = max(offset_error, float(np.linalg.norm(
                np.asarray(actual['Offsets'][i])-data['helpers'][i]['offset_cm']-expected_offset)))
    return dict(cases=len(values), maximum_offset_error_cm=offset_error, maximum_moment_error_cm2=moment_error)


def measure_ellipsoids(export, measured, axes):
    count = len(axes)
    if export['used'] is None:
        raise ValueError('Core selection indices need the archive vertex map')
    to_export = -np.ones(int(export['used'].max())+1, dtype=int)
    to_export[export['used']] = np.arange(len(export['used']))
    regions, checks = [], []
    for source in measured['regions']:
        indices = to_export[np.asarray(source['vertex_indices'])]
        if (indices < 0).any():
            raise ValueError('Core selection vertex missing from the export')
        neutral = export['points'][indices]
        shape = np.asarray(source['shape_basis_cm'], dtype=np.float64)
        center_weights = np.asarray(source['center_weights'], dtype=np.float64)
        singular_values = np.linalg.svd(shape, compute_uv=False)
        inverse = np.linalg.inv(shape)
        changes = export['deltas'][:, indices]
        corners = []
        for mask in range(1 << count):
            points = neutral + sum((changes[k] for k in range(count) if mask & (1 << k)), np.zeros_like(neutral))
            center = center_weights @ points
            normalized = (points-center) @ inverse.T
            scale = float(np.linalg.norm(normalized, axis=1).max()) + .01/singular_values.min()
            corners.append(dict(mask=mask, center_cm=center.tolist(), scale=scale,
                                semiaxes_cm=(singular_values*scale).tolist()))
        # The first 64 corners are the six-axis corners when the first six axes match.
        old = source['morph_ellipsoids']
        same = min(len(old), len(corners)) if measured['morphs'] == list(axes[:len(measured['morphs'])]) else 0
        checks.append(dict(region=source['name'],
            six_axis_center_difference_cm=max((float(np.linalg.norm(np.subtract(corners[m]['center_cm'], old[m]['center_cm'])))
                                               for m in range(same)), default=None),
            six_axis_scale_difference=max((abs(corners[m]['scale']-old[m]['scale']) for m in range(same)), default=None)))
        regions.append(dict(name=source['name'], parent=source['parent'], vertex_indices=source['vertex_indices'],
            selected_vertices=len(indices), shape_basis_cm=source['shape_basis_cm'], center_weights=source['center_weights'],
            reference_parent_matrix=source['reference_parent_matrix'], morph_ellipsoids=corners,
            fit=dict(method='fixed six-axis shape and center weights, scale enlarged to contain the core at every corner')))
    return dict(morphs=list(axes), morph_range=[0, 1], regions=regions), checks


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--recipe', type=Path, required=True)
    parser.add_argument('--export', type=Path, required=True)
    parser.add_argument('--deltas', type=Path)
    parser.add_argument('--measured', type=Path, required=True)
    parser.add_argument('--layout-out', type=Path, required=True)
    parser.add_argument('--ellipsoids-out', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    for path in (args.layout_out, args.ellipsoids_out, args.report):
        if path and path.exists():
            raise FileExistsError(path)
    axes = read_recipe(args.recipe)
    helper_layout = json.loads((args.measured/'body-helper-layout-v1.json').read_text())
    contact_regions = json.loads((args.measured/'body-contact-regions-v1.json').read_text())
    six = json.loads((args.measured/'body-contact-ellipsoids-v2.json').read_text())
    if [h['bone'] for h in helper_layout['helpers']] != NAMES or [r['name'] for r in six['regions']] != NAMES:
        raise ValueError('Measured helper identity differs')
    export = load_export(args.export, args.deltas, axes)
    layout, supports = measure_layout(export, helper_layout, contact_regions, axes)
    review = review_layout(layout, export, supports, axes)
    if review['maximum_offset_error_cm'] > 1e-6 or review['maximum_moment_error_cm2'] > 1e-6:
        raise AssertionError('Polynomial coefficients differ from direct moments: ' + json.dumps(review))
    ellipsoids, checks = measure_ellipsoids(export, six, axes)
    neutral = max(c['six_axis_center_difference_cm'] or 0 for c in checks)
    sources = dict(export=str(args.export), export_sha256=sha(args.export),
                   deltas=str(args.deltas) if args.deltas else None, deltas_sha256=sha(args.deltas) if args.deltas else None,
                   measured=str(args.measured), axis_sources=export['sources'])
    layout.update(sources=sources, scope='Fixed neutral area times exported helper weight on the given export. '
                  'Neutral offsets/moments kept from body-helper-layout-v1. No volumetric tissue or live acceptance claim.')
    ellipsoids.update(sources=sources, scope='Fixed-shape ellipsoids from the six-axis v2 fit, enlarged per corner. '
                      'No minimum-volume, collision response or live acceptance claim.')
    args.layout_out.write_text(json.dumps(layout, indent=1, allow_nan=False)+'\n')
    args.ellipsoids_out.write_text(json.dumps(ellipsoids, indent=1, allow_nan=False)+'\n')
    report = dict(axes=axes, corners=1 << len(axes), polynomial_review=review, ellipsoid_checks=checks,
                  largest_six_axis_center_difference_cm=neutral, **sources)
    if args.report:
        args.report.write_text(json.dumps(report, indent=1)+'\n')
    print(json.dumps(dict(axes=axes, corners=1 << len(axes), polynomial_review=review,
                          largest_six_axis_center_difference_cm=neutral,
                          largest_six_axis_scale_difference=max(c['six_axis_scale_difference'] or 0 for c in checks))))


if __name__ == '__main__':
    main()
