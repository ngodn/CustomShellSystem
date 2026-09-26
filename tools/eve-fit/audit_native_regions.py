"""Compare native regional lattice samples with their skinned source surfaces."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args(sys.argv[sys.argv.index('--')+1:])
a.output.mkdir(exist_ok=False)
w = Path(__file__).resolve().parents[2] / 'work/eve26'
rows = []
for region, suffix in (('pelvis', 'Pelv'), ('thigh_l', 'ThighL'), ('thigh_r', 'ThighR')):
    body = json.loads((w/'skin-regions2'/f'{region}.json').read_text())
    for tag in ('rest', '64', '68'):
        path = w/f'region-{suffix}-{tag}.json'
        native = json.loads(path.read_text())
        assert native['fresh_load'] and len(native['bodies']) == 1
        q = native['bodies'][0]
        posed, mapped = q['sample_positions_cm'], q['sample_lattice_positions_cm']
        assert len(posed) == len(mapped) == len(body['positions'])
        assert all(point is not None for point in mapped), 'Surface comparison requires all mapped vertices'
        errors = [math.dist(x, y) for x, y in zip(posed, mapped, strict=True)]
        tree = BVHTree.FromPolygons(mapped, body['indices'], all_triangles=True)
        gaps = [tree.find_nearest(Vector(point))[3] for point in posed]
        phi = [s[1] for s in q['samples']]
        assert all(math.isfinite(v) for v in errors+gaps+phi)
        valid_phi = [v for v in phi if abs(v) < 1e6]
        worst = sorted(range(len(errors)), key=lambda i: errors[i], reverse=True)[:8]
        row = dict(region=region, frame=tag, input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                   root=q['root_bone'], used_bones=q['bones'], samples=len(posed),
                   max_mapping_error_cm=max(errors), max_nearest_surface_gap_cm=max(gaps),
                   p95_nearest_surface_gap_cm=sorted(gaps)[int(.95*(len(gaps)-1))],
                   cloth_query_misses=len(phi)-len(valid_phi),
                   min_phi_cm=min(valid_phi), max_phi_cm=max(valid_phi),
                   worst=[dict(index=i, rest_cm=body['positions'][i], mapping_error_cm=errors[i],
                               surface_gap_cm=gaps[i], phi_cm=phi[i]) for i in worst])
        rows.append(row)
        if tag != 'rest':
            (a.output/f'{region}-{tag}-surface.json').write_text(json.dumps(dict(
                scope='Native direct lattice-mapped source triangulation, not the implicit collider boundary. Includes closed cut caps. Nearest surface gaps are unsigned.',
                body_positions_cm=posed, mapped_positions_cm=mapped, nearest_distance_cm=gaps), separators=(',', ':'))+'\n')
        print(region, tag, 'max mapping', max(errors), 'surface gap', max(gaps), flush=True)
(a.output/'audit.json').write_text(json.dumps(dict(
    scope='Native sampled regional diagnostics, not cloth acceptance. Right source samples precede a maximum 0.000935 cm import weld. Surface gap uses approximate mapped triangulation; phi uses the native implicit query.',
    cases=rows), indent=2)+'\n')
