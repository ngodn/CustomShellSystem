"""Build a CSS body shape from the VaM morph library.

End to end: Genesis 2 Female base + a tier selection -> a deformation carried
onto whichever body CSS actually ships.

    tools/build_figure_body.py \
        --g2f      work/v1.0.0-body/g2f.glb \
        --library  reference/CSS-v1.0.0-plan \
        --target   path/to/beaute_body.obj \
        --breast   busty --glutes full --figure base \
        --out      work/v1.0.0-body/out/

Four stages, each of which can fail loudly rather than quietly:

1. **Verify the base.** The mesh must weld to exactly 21556 vertices, and a set
   of known region morphs must land in one cluster on it. The second check is
   the one that matters: vertex *count* being right does not mean vertex *order*
   is, and a wrong order produces a body that looks shredded rather than obviously
   broken. See `gltf_mesh.morph_lands_where_expected`.
2. **Stack the morphs.** `vam_figure` resolves the tier selection into weights.
3. **Align.** G2F and the target are different bodies, so landmark pairs drive a
   similarity fit first. G2F's landmarks come from the morph library itself: the
   vertices `Busty Breasts 1` moves are, by definition, its bust.
4. **Transfer.** `shape_transfer` re-emits the target through the donor's
   before/after surfaces.

Anatomy detail (nipple, areola, labia) is deliberately not handled here. It needs
region-masked treatment at a much smaller scale than a figure morph, and mixing
the two is what smeared the nipple in the current Beaute pipeline.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import gltf_mesh
import shape_transfer
import vam_figure
import vam_morph

G2F_VERTICES = vam_morph.G2F_BASE_VERTICES

# Region morphs used to prove the vertex order and to place landmarks. Each is a
# morph that touches one part of the body and nothing else.
LANDMARK_MORPHS = {
    'bust': 'Busty Breasts 1',
    'glutes': 'Booty Shape 1',
    'navel': 'MonsterShinkai_Navel - Body',
}


def load_library(paths) -> dict[str, vam_morph.Morph]:
    library: dict[str, vam_morph.Morph] = {}
    for entry in paths:
        path = Path(entry)
        if path.is_dir():
            for pack in sorted(path.glob('*.var')):
                for morph in vam_morph.load_package(pack):
                    library[morph.name] = morph
            for loose in sorted(path.rglob('*.vmb')):
                morph = vam_morph.load(loose)
                library.setdefault(morph.name, morph)
        elif path.suffix.lower() == '.var':
            for morph in vam_morph.load_package(path):
                library[morph.name] = morph
        else:
            morph = vam_morph.load(path)
            library[morph.name] = morph
    return library


def load_target(path: Path):
    """Read the body CSS ships. OBJ or glb, welded either way."""
    if path.suffix.lower() == '.obj':
        points, triangles = [], []
        for line in path.read_text().splitlines():
            if line.startswith('v '):
                points.append([float(v) for v in line.split()[1:4]])
            elif line.startswith('f '):
                corner = [int(p.split('/')[0]) - 1 for p in line.split()[1:]]
                for i in range(1, len(corner) - 1):
                    triangles.append([corner[0], corner[i], corner[i + 1]])
        return np.asarray(points, dtype=np.float64), np.asarray(triangles, dtype=np.int64)
    mesh = gltf_mesh.load(path)
    points, triangles, _ = gltf_mesh.weld(mesh.points, mesh.triangles)
    return points, triangles


def verify_base(points: np.ndarray, library: dict, expect: int = G2F_VERTICES) -> dict:
    """Confirm the base mesh is G2F and in the order the morphs expect."""
    report: dict = {'vertices': int(len(points)), 'expected': expect,
                    'count_ok': len(points) == expect, 'regions': {}}
    if not report['count_ok']:
        return report
    clustered = []
    for label, name in LANDMARK_MORPHS.items():
        morph = library.get(name)
        if morph is None or not morph.targets_base():
            continue
        result = gltf_mesh.morph_lands_where_expected(points, morph.indices)
        report['regions'][label] = result
        clustered.append(result['clustered'])
    report['order_ok'] = bool(clustered) and all(clustered)
    report['checked_regions'] = len(clustered)
    return report


def landmarks_from_morphs(points: np.ndarray, library: dict) -> dict[str, np.ndarray]:
    """Where each named region sits on G2F, from the morphs that move it."""
    found = {}
    for label, name in LANDMARK_MORPHS.items():
        morph = library.get(name)
        if morph is None or not morph.targets_base():
            continue
        found[label] = shape_transfer.region_centroid(points, morph.indices)
    return found


def target_landmarks(audit_path: Path) -> dict[str, np.ndarray]:
    """Matching points on the CSS body, read from its repair audit."""
    audit = json.loads(audit_path.read_text())
    fills = {f['label']: f for f in audit.get('fills', [])}
    out: dict[str, np.ndarray] = {}
    if 'nipple_left' in fills and 'nipple_right' in fills:
        out['bust'] = (np.asarray(fills['nipple_left']['center'])
                       + np.asarray(fills['nipple_right']['center'])) / 2.0
    navel = audit.get('navel')
    if isinstance(navel, dict) and 'center' in navel:
        out['navel'] = np.asarray(navel['center'])
    if 'vulva' in fills:
        out['vulva'] = np.asarray(fills['vulva']['center'])
    return out


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--g2f', required=True, type=Path)
    parser.add_argument('--library', action='append', required=True)
    parser.add_argument('--target', type=Path, help='the CSS body to deform')
    parser.add_argument('--target-audit', type=Path, help='its repair audit, for landmarks')
    parser.add_argument('--figure', default=vam_figure.FIGURE.default)
    parser.add_argument('--breast', default=vam_figure.BREAST.default)
    parser.add_argument('--glutes', default=vam_figure.GLUTES.default)
    parser.add_argument('--nipples', type=float, default=0.0)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--max-distance', type=float, default=0.0,
                        help='bind radius in TARGET units; 0 picks 4%% of the body height')
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args(argv)

    args.out.mkdir(parents=True, exist_ok=True)
    library = load_library(args.library)
    print(f'library: {len(library)} morphs')

    mesh = gltf_mesh.load(args.g2f)
    print(f'base as exported: {mesh}')
    base, base_tris, _ = gltf_mesh.weld(mesh.points, mesh.triangles)
    print(f'base welded: {len(base)} vertices, {len(base_tris)} triangles')

    check = verify_base(base, library)
    for label, result in check['regions'].items():
        print(f"  {label:<8} spread ratio {result['spread_ratio']:.3f} "
              f"-> {'clustered' if result['clustered'] else 'SCATTERED'}")
    (args.out / 'base-verify.json').write_text(json.dumps(check, indent=2, default=str))

    if not check['count_ok']:
        print(f"\nBase has {check['vertices']} vertices, not {G2F_VERTICES}. "
              f"The morphs index the {G2F_VERTICES}-vertex base, so nothing here would "
              f"apply correctly. Stopping.", file=sys.stderr)
        return 1
    if not check.get('order_ok'):
        print('\nVertex count is right but region morphs land scattered, which means the '
              'order differs from what the morphs expect. Applying them would shred the '
              'mesh. Stopping.', file=sys.stderr)
        return 1
    print('base verified: count and order both good')
    if args.verify_only:
        return 0

    selection = {'figure': args.figure, 'breast': args.breast, 'glutes': args.glutes}
    stack = vam_figure.resolve(selection, {'nipples': args.nipples} if args.nipples else None)
    strain = vam_figure.strain(stack, library)
    print(f'\nstack {selection}: {len(stack)} morphs, '
          f"largest combined move {strain['largest_cm']:.2f} cm over "
          f"{strain['moved_vertices']} vertices")
    if strain['missing']:
        print(f"  missing from the library: {strain['missing']}", file=sys.stderr)
        return 1

    graft_needed = vam_figure.needs_graft(stack, library)
    usable = {n: w for n, w in stack.items() if n not in graft_needed}
    if graft_needed:
        print(f'  skipping {len(graft_needed)} morph(s) needing the genitalia geograft: '
              f'{graft_needed}')

    # No unit conversion here on purpose. The morphs and the G2F base both come
    # out of DAZ/VaM in the same units, so a delta adds straight onto a position.
    # The target body may well be in different units (the Beaute body is in
    # metres, cooked meshes are in centimetres), and that difference is absorbed
    # by the scale term of the landmark similarity fit below rather than by
    # guessing here. Converting at this point would double-apply it.
    morphed = base.copy()
    for name, weight in usable.items():
        morphed = np.asarray(library[name].apply(morphed, weight), dtype=np.float64)
    moved = np.linalg.norm(morphed - base, axis=1)
    print(f'morphed base: {int((moved > 1e-6).sum())} vertices moved, '
          f'largest {moved.max():.2f}')
    gltf_mesh.write_obj(args.out / 'g2f_morphed.obj', morphed, base_tris)
    gltf_mesh.write_obj(args.out / 'g2f_base.obj', base, base_tris)

    if not args.target:
        print(f'\nwrote {args.out}/g2f_base.obj and g2f_morphed.obj (no --target given)')
        return 0

    target_points, target_tris = load_target(args.target)
    print(f'\ntarget: {len(target_points)} vertices, {len(target_tris)} triangles')

    donor_marks = landmarks_from_morphs(base, library)
    marks = target_landmarks(args.target_audit) if args.target_audit else {}
    shared = sorted(set(donor_marks) & set(marks))
    if len(shared) >= 3:
        rotation, translation, scale = shape_transfer.similarity_transform(
            np.array([donor_marks[k] for k in shared]),
            np.array([marks[k] for k in shared]))
        print(f'aligned on {shared}: scale {scale:.4f}')
        base_a = scale * base @ rotation.T + translation
        morphed_a = scale * morphed @ rotation.T + translation
    else:
        print(f'only {len(shared)} shared landmark(s); using the meshes as they are. '
              f'Pass --target-audit for a proper alignment.')
        base_a, morphed_a = base, morphed

    # Units differ between sources (metres here, centimetres for cooked meshes),
    # so the bind radius is taken from the target's own height rather than a
    # constant that silently means 6 m on one mesh and 6 cm on another.
    height = float(target_points[:, 2].max() - target_points[:, 2].min())
    max_distance = args.max_distance if args.max_distance > 0 else height * 0.04
    print(f'bind radius {max_distance:.4f} (target height {height:.4f})')
    result, report = shape_transfer.transfer(
        target_points, base_a, morphed_a, base_tris, max_distance=max_distance,
        falloff=max_distance * 0.35)
    print(f"transfer: {report['moved_vertices']}/{report['total_vertices']} vertices moved, "
          f"largest {report['largest_move']:.2f}, mean {report['mean_move']:.3f}")

    gltf_mesh.write_obj(args.out / 'target_morphed.obj', result, target_tris)
    (args.out / 'transfer-report.json').write_text(json.dumps(
        {'selection': selection, 'stack': stack, 'strain': strain,
         'alignment_landmarks': shared, 'transfer': report}, indent=2, default=str))
    print(f'\nwrote {args.out}/target_morphed.obj')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
