"""Compare traced lattice corner influences with the source body's skin weights."""
import argparse
import json
import math
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists()
query = json.loads(a.input.read_text())['bodies'][0]
reports = []
for trace in query['trace_samples']:
    assert not trace['empty_cell'], 'Trace falls in an unweighted lattice cell'
    corners = trace['corners']
    assert len(corners) == 4
    bary = [c['barycentric_weight'] for c in corners]
    assert min(bary) >= -1e-6 and abs(sum(bary)-1) < 1e-6
    rest = [sum(b*c['rest_cm'][i] for b, c in zip(bary, corners)) for i in range(3)]
    posed = [sum(b*c['posed_cm'][i] for b, c in zip(bary, corners)) for i in range(3)]
    rest_error = math.dist(rest, trace['rest_cm'])
    posed_error = math.dist(posed, trace['lattice_cm'])
    assert max(rest_error, posed_error) < .001
    effective = {}
    for b, corner in zip(bary, corners):
        assert abs(sum(weight for _, weight in corner['weights'])-1) < .0001
        for bone, weight in corner['weights']:
            effective[bone] = effective.get(bone, 0)+b*weight
    source = dict(trace['skin_weights'])
    l1 = sum(abs(source.get(b, 0)-effective.get(b, 0)) for b in source.keys() | effective.keys())
    report = {'body_index': trace['body_index'], 'rest_reconstruction_error_cm': rest_error,
              'lattice_reconstruction_error_cm': posed_error,
              'skin_to_lattice_distance_cm': math.dist(trace['skin_cm'], trace['lattice_cm']),
              'source_weights': source,
              'effective_lattice_weights': dict(sorted(effective.items(), key=lambda pair: -pair[1])),
              'weight_l1_difference': l1}
    reports.append(report)
    print(report)
assert reports
a.output.write_text(json.dumps({
    'scope': 'Exact tetrahedron position reconstruction and barycentric mixture of corner bone weights at selected samples. Position error also depends on corner locations; weight disagreement alone does not prove which correction will work.',
    'input': str(a.input), 'sample_frame': query['sample_frame'], 'samples': reports
}, indent=2)+'\n')
