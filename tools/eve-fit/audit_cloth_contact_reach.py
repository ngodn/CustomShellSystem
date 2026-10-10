"""Find provably incompatible motion bounds and sphere/capsule contact.

Run in Blender. This reads captured poses and authored cloth data, never assets.
A negative result only excludes this sufficient test for infeasibility. It does
not establish a feasible solution, body coverage, or acceptable cloth motion.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import numpy as np
from mathutils import Matrix, Quaternion, Vector


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capsule_phi(points, start, end, r0, r1):
    axis = end - start
    length = float(np.linalg.norm(axis))
    if length <= 1e-8:
        raise ValueError('Degenerate capsule')
    t = np.clip((points - start) @ axis / length**2, 0, 1)
    phi = np.linalg.norm(points - start - t[:, None] * axis, axis=1) - (r0 + t * (r1 - r0))
    # This engine capsule interpolates radius along its projected center line.
    # Its phi is not a signed Euclidean distance when the radii differ.
    lipschitz = float(np.sqrt(1 + ((r1 - r0) / length)**2))
    return phi, lipschitz


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('mesh', 'proxy', 'collision', 'motion', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--slot', required=True)
    parser.add_argument('--thickness', type=float, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.output.exists():
        raise FileExistsError(args.output)
    if not np.isfinite(args.thickness) or args.thickness < 0:
        raise ValueError('Invalid contact thickness')
    paths = (args.mesh, args.proxy, args.collision, args.motion)
    mesh, proxy, recipe, motion = [json.loads(p.read_text()) for p in paths]
    slot = proxy['slots'][args.slot]
    if motion['diagnostic_no_body_collision'] or motion['diagnostic_max_distance_scale'] != 1:
        raise ValueError('Requires body contact and unscaled distance bounds')
    names = {b['name']: i for i, b in enumerate(mesh['bones'])}
    bind = []
    for bone in mesh['bones']:
        q = bone['rotation']
        local = Matrix.LocRotScale(Vector(bone['translation']), Quaternion((q[3], *q[:3])), Vector(bone['scale']))
        bind.append(bind[bone['parent']] @ local if bone['parent'] >= 0 else local)
    inverse = np.asarray([np.asarray(m.inverted()) for m in bind])
    influence = [(i, names[b], w) for i, weights in enumerate(slot['weights']) for b, w in weights]
    vi, bi = np.asarray([(i, b) for i, b, _ in influence], dtype=int).T
    weights = np.asarray([w for _, _, w in influence])
    rest = np.asarray(slot['positions'])
    bounds = np.asarray(slot['max_distances'])
    if len(rest) != motion['particles'] or len(bounds) != len(rest):
        raise ValueError('Particle counts disagree')
    sums = np.zeros(len(rest))
    np.add.at(sums, vi, weights)
    if not np.allclose(sums, 1, atol=1e-6):
        raise ValueError('Unnormalized skin weights')
    spheres = recipe['spheres']
    connected = {i for c in recipe['connections'] for i in c['sphere_indices']}
    rows = []
    max_local_scale_error = 0.
    for frame in motion['frames']:
        snapshot = frame['pose']['Snapshot']
        entries = dict(zip(snapshot['BoneNames'], snapshot['LocalTransforms'], strict=True))
        pose = []
        for bone in mesh['bones']:
            t = entries[bone['name']]
            max_local_scale_error = max(max_local_scale_error, *(abs(t['Scale3D'][k] - 1) for k in 'XYZ'))
            local = Matrix.LocRotScale(Vector([t['Translation'][k] for k in 'XYZ']),
                Quaternion([t['Rotation'][k] for k in 'WXYZ']), Vector([t['Scale3D'][k] for k in 'XYZ']))
            pose.append(pose[bone['parent']] @ local if bone['parent'] >= 0 else local)
        transforms = np.asarray([np.asarray(m) for m in pose]) @ inverse
        skin = np.zeros_like(rest)
        np.add.at(skin, vi, (np.einsum('nij,nj->ni', transforms[bi, :3, :3], rest[vi])
            + transforms[bi, :3, 3]) * weights[:, None])
        centers = np.asarray([list(pose[names[s['bone']]] @ Vector(s['local_center_cm'])) for s in spheres])
        shapes = []
        for c in recipe['connections']:
            i, j = c['sphere_indices']
            phi, factor = capsule_phi(skin, centers[i], centers[j], spheres[i]['radius_cm'], spheres[j]['radius_cm'])
            shapes.append(([i, j], phi, factor))
        for i, sphere in enumerate(spheres):
            if i not in connected:
                shapes.append(([i], np.linalg.norm(skin - centers[i], axis=1) - sphere['radius_cm'], 1.))
        witnesses = []
        for indices, phi, factor in shapes:
            # If phi cannot reach thickness anywhere in the allowed ball,
            # every allowed position violates this one collider constraint.
            deficit = (args.thickness - phi) / factor - bounds
            for vertex in np.flatnonzero(deficit > 1e-4):
                witnesses.append(dict(vertex=int(vertex), spheres=indices,
                    motion_bound_cm=float(bounds[vertex]), exit_lower_bound_cm=float((args.thickness-phi[vertex])/factor),
                    deficit_cm=float(deficit[vertex]), fixed=bool(bounds[vertex] < .1)))
        rows.append(dict(frame=frame['frame'], witnesses=witnesses))
    report = dict(scope=__doc__, formula='(thickness - phi(skin)) / Lipschitz(phi) > max_distance',
        thickness_cm=args.thickness, rows=rows, max_local_scale_error=max_local_scale_error,
        collider_scope='Authored radii with bone-transformed centers; native radius scaling is not read back here.',
        inputs={str(p): digest(p) for p in paths},
        script_sha256=digest(Path(__file__)))
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(output=str(args.output), frames=len(rows),
        dynamic_conflicts=sum(sum(not w['fixed'] for w in r['witnesses']) for r in rows),
        fixed_overlaps=sum(sum(w['fixed'] for w in r['witnesses']) for r in rows))))


if __name__ == '__main__':
    main()
