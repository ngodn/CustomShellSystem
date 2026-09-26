"""Summarize native signed-distance samples without treating a saved collider as a fit."""
import argparse
import hashlib
import json
import math
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists()
w = Path(__file__).resolve().parents[2]/'work/eve26'
body = json.loads((w/'body-collider.json').read_text())
native = json.loads(a.input.read_text())
assert native['fresh_load'] and len(native['bodies']) == 1
rows = native['bodies'][0]['samples']
assert len(rows) == len(body['positions'])
assert all(len(row) == 5 and all(math.isfinite(v) for v in row) for row in rows)
arm = [sum(weight for name, weight in weights
           if any(n in name for n in ('arm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')))
       for weights in body['weights']]
roi = [i for i, point in enumerate(body['positions']) if 90 <= point[2] <= 125 and arm[i] < .25]

def summarize(ids):
    assert ids
    finite = [rows[i][1] for i in ids if abs(rows[i][1]) < 1e6]
    absolute = sorted(abs(v) for v in finite)
    return {
        'samples': len(ids), 'cloth_query_misses': len(ids)-len(finite),
        'outside_over_3mm': sum(v > .3 for v in finite),
        'inside_over_3mm': sum(v < -.3 for v in finite),
        'min_phi_cm': min(finite) if finite else None,
        'max_phi_cm': max(finite) if finite else None,
        'p95_abs_phi_cm': absolute[int(.95*(len(absolute)-1))] if absolute else None,
        'ordinary_and_cloth_query_disagree': sum(abs(rows[i][0]-rows[i][1]) > .001 for i in ids),
        'worst': [{'body_index': i, 'source_vertex': body['source_vertices'][i],
                   'rest_cm': body['positions'][i], 'phi_cm': rows[i][0], 'cloth_phi_cm': rows[i][1]}
                  for i in sorted(ids, key=lambda i: abs(rows[i][1]), reverse=True)[:8]]}

frame = native['bodies'][0].get('sample_frame', -1)
report = {'input_sha256': hashlib.sha256(a.input.read_bytes()).hexdigest(), 'sample_frame': frame,
          'scope': 'Body vertex samples at one recorded pose, or rest when frame is -1. Negative phi means collider outside the sampled body point; positive means undercoverage. Includes creases and internal surfaces. No surface containment or simulated-cloth proof.',
          'whole_body': summarize(list(range(len(rows)))), 'skirt_region': summarize(roi),
          'height_bands': [{'rest_z_cm': [lo, hi], **summarize([i for i in roi if lo <= body['positions'][i][2] < hi])}
                           for lo, hi in ((90, 105), (105, 118), (118, 125.001))]}
query = native['bodies'][0]
if 'sample_lattice_positions_cm' in query:
    lattice = query['sample_lattice_positions_cm']
    posed = query['sample_positions_cm']
    assert len(lattice) == len(posed) == len(rows)
    distances = {i: math.dist(lattice[i], posed[i]) for i in roi if lattice[i] is not None}
    values = sorted(distances.values())
    report['lattice_mapping'] = {
        'samples': len(values), 'outside_grid': len(roi)-len(values),
        'max_position_error_cm': max(values), 'p95_position_error_cm': values[int(.95*(len(values)-1))],
        'worst': [{'body_index': i, 'rest_cm': body['positions'][i], 'skin_cm': posed[i],
                   'lattice_cm': lattice[i], 'error_cm': distances[i]}
                  for i in sorted(distances, key=distances.get, reverse=True)[:8]],
        'scope': 'Direct lattice mapping versus skinning, separate from signed-distance lookup. Empty lattice cells return the undeformed point per engine behavior.'}
a.output.write_text(json.dumps(report, indent=2)+'\n')
for key in ('whole_body', 'skirt_region'):
    print(key, {k: v for k, v in report[key].items() if k != 'worst'})
if 'lattice_mapping' in report:
    print('lattice_mapping', {k: v for k, v in report['lattice_mapping'].items() if k != 'worst'})
