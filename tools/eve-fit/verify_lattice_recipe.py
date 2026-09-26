"""Verify fresh native lattice readback against its offline recipe and original SDF samples."""
import argparse
import hashlib
import json
import math
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--recipe',type=Path,required=True)
p.add_argument('--readback',type=Path,required=True)
p.add_argument('--reference',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a = p.parse_args()
assert not a.output.exists()
recipe = json.loads(a.recipe.read_text())
native = json.loads(a.readback.read_text())
reference = json.loads(a.reference.read_text())
assert native['fresh_load'] and reference['fresh_load']
assert len(native['bodies']) == len(reference['bodies']) == 1
body,old = native['bodies'][0],reference['bodies'][0]
assert body['sample_frame'] == -1
assert old.get('sample_frame') == -1 or ('sample_frame' not in old and old['sample_scope'].startswith('Rest pose,'))
assert body['root_bone'] == recipe['root_bone'] == old['root_bone']
assert body['level_set_grid'] == old['level_set_grid']
saved = body['lattice_geometry']
assert saved['counts'] == recipe['grid']['counts']
expected = {tuple(n['index']):n for n in recipe['grid']['nodes']}
assert len(expected) == len(saved['nodes'])
position_error,weight_error = 0.,0.
seen = set()
for node in saved['nodes']:
    index = tuple(node['index'])
    assert index not in seen
    seen.add(index)
    target = expected[index]
    position_error = max(position_error,math.dist(node['rest_cm'],target['rest_cm']))
    weights,wanted = dict(node['weights']),dict(target['weights'])
    assert weights.keys() == wanted.keys()
    weight_error = max(weight_error,max(abs(weights[k]-wanted[k]) for k in weights))
assert position_error < .001 and weight_error < 1e-6
assert len(body['samples']) == len(old['samples'])
distance_error = max(abs(x[j]-y[j]) for x,y in zip(body['samples'],old['samples'],strict=True) for j in (0,1))
assert distance_error < .002,distance_error
report = {'recipe_sha256':hashlib.sha256(a.recipe.read_bytes()).hexdigest(),
          'readback_sha256':hashlib.sha256(a.readback.read_bytes()).hexdigest(),
          'nodes':len(expected),'position_max_cm':position_error,'weight_max':weight_error,
          'rest_distance_difference_max_cm':distance_error,
          'rest_distance_tolerance_cm':.002,
          'scope':'Saved lattice recipe and rest-sample agreement only. No motion, cloth or performance acceptance.'}
a.output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
