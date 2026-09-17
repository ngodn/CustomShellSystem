"""Give a CSS body real anatomy by conforming its regions onto a donor.

Separate from `build_figure_body` on purpose. A figure morph moves a whole body
region by several centimetres and wants a wide, smooth bind. Anatomy is the
opposite: a few hundred vertices, millimetres of relief, and every bit of that
relief is the point. Running both through one smoothing pass is what flattened
the nipple in the current Beaute pipeline.

    tools/conform_anatomy.py \
        --target work/.../Seductress_Source_Fitted_Torso.obj \
        --audit  .../SeductressV2_Body.audit.json \
        --donor  work/v1.0.0-body/g2f_posed.obj \
        --out    work/v1.0.0-body/out/

Measured on the Beaute torso before any of this ran:

    nipple_left    426 verts within 3 cm, relief +11.29 mm
    nipple_right   425 verts within 3 cm, relief +11.97 mm
    vulva          565 verts within 3 cm, relief +11.57 mm

A real nipple stands 5-9 mm proud of the breast and a vulva does not stand proud
at all, so those three numbers are the defect in one line: authored fills that
bulge, rather than anatomy. `measure` reports the same numbers after a conform so
the change is a number rather than an opinion.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import shape_transfer

# Region radius in metres, and how strongly each is pulled onto the donor.
# Radii are deliberately snug: a wider region drags breast or thigh shape into
# something that should only change an areola.
REGIONS = {
    'nipple_left': {'radius': 0.030, 'strength': 1.0},
    'nipple_right': {'radius': 0.030, 'strength': 1.0},
    'vulva': {'radius': 0.035, 'strength': 1.0},
}


def load_obj(path: Path):
    points, triangles = [], []
    for line in Path(path).read_text().splitlines():
        if line.startswith('v '):
            points.append([float(v) for v in line.split()[1:4]])
        elif line.startswith('f '):
            corner = [int(p.split('/')[0]) - 1 for p in line.split()[1:]]
            for i in range(1, len(corner) - 1):
                triangles.append([corner[0], corner[i], corner[i + 1]])
    return np.asarray(points, dtype=np.float64), np.asarray(triangles, dtype=np.int64)


def landmarks(audit_path: Path) -> dict[str, dict]:
    audit = json.loads(Path(audit_path).read_text())
    out = {}
    for fill in audit.get('fills', []):
        label = fill.get('label')
        if label in REGIONS:
            normal = np.asarray(fill['normal'], dtype=np.float64)
            out[label] = {'centre': np.asarray(fill['center'], dtype=np.float64),
                          'normal': normal / max(np.linalg.norm(normal), 1e-12)}
    return out


def measure(points: np.ndarray, centre: np.ndarray, normal: np.ndarray,
            radius: float) -> dict:
    """Relief of a region's core above its surrounding ring, in millimetres.

    Positive means the middle stands proud of the skin around it. This is the
    single number that says whether a nipple reads as anatomy or as a cone, and
    whether a vulva is modelled or is a bulge.
    """
    index = shape_transfer.select_region(points, centre, radius)
    if not len(index):
        return {'vertices': 0, 'relief_mm': 0.0}
    local = points[index] - centre
    height = local @ normal
    lateral = np.linalg.norm(local - np.outer(height, normal), axis=1)
    ring = lateral > radius * 0.75
    core = lateral < radius * 0.20
    base = float(np.median(height[ring])) if ring.any() else 0.0
    tip = float(np.max(height[core])) if core.any() else 0.0
    return {
        'vertices': int(len(index)),
        'relief_mm': (tip - base) * 1000.0,
        'core_vertices': int(core.sum()),
        'ring_vertices': int(ring.sum()),
    }


def conform(target_points, target_triangles, donor_points, donor_triangles,
            marks: dict, regions: dict | None = None, feather: float = 0.35):
    """Conform every named region of the target onto the donor."""
    regions = regions or REGIONS
    points = target_points.copy()
    reports = {}
    for label, mark in marks.items():
        spec = regions.get(label)
        if spec is None:
            continue
        before = measure(points, mark['centre'], mark['normal'], spec['radius'])
        points, detail = shape_transfer.conform_region_along_normal(
            points, target_triangles, donor_points, donor_triangles,
            centre=mark['centre'], radius=spec['radius'],
            strength=spec['strength'], feather=feather)
        after = measure(points, mark['centre'], mark['normal'], spec['radius'])
        reports[label] = {'before': before, 'after': after, 'transfer': detail}
    return points, reports


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--target', required=True, type=Path)
    parser.add_argument('--audit', required=True, type=Path)
    parser.add_argument('--donor', type=Path,
                        help='mesh carrying real anatomy; omit to only measure')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--feather', type=float, default=0.35)
    args = parser.parse_args(argv)

    args.out.mkdir(parents=True, exist_ok=True)
    target_points, target_triangles = load_obj(args.target)
    marks = landmarks(args.audit)
    print(f'target: {len(target_points)} vertices, landmarks {sorted(marks)}')

    if not args.donor:
        report = {label: measure(target_points, m['centre'], m['normal'],
                                 REGIONS[label]['radius']) for label, m in marks.items()}
        for label, row in report.items():
            print(f"  {label:<13} {row['vertices']:>4} verts, relief {row['relief_mm']:+6.2f} mm")
        (args.out / 'anatomy-before.json').write_text(json.dumps(report, indent=2))
        return 0

    donor_points, donor_triangles = load_obj(args.donor)
    print(f'donor: {len(donor_points)} vertices')
    result, reports = conform(target_points, target_triangles,
                              donor_points, donor_triangles, marks, feather=args.feather)

    for label, row in reports.items():
        print(f"  {label:<13} relief {row['before']['relief_mm']:+6.2f} -> "
              f"{row['after']['relief_mm']:+6.2f} mm   "
              f"({row['transfer']['moved_vertices']} moved, "
              f"{row['transfer']['missed']} missed)")

    import gltf_mesh
    gltf_mesh.write_obj(args.out / 'target_anatomy.obj', result, target_triangles)
    (args.out / 'anatomy-report.json').write_text(json.dumps(reports, indent=2, default=str))
    print(f'wrote {args.out}/target_anatomy.obj')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
