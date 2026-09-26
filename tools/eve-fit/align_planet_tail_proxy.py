"""Align the author's tail cloth reference to the current exported tail."""
import hashlib
import json
from pathlib import Path
import numpy as np

work = Path(__file__).resolve().parents[2] / 'work/eve26'
out = work / 'planet-tail-proxy.json'
assert not out.exists()
author_path = work / 'planet-tail-author.json'
source = json.loads(author_path.read_text())
objects = {o['name']:o for o in source['objects']}
tail = objects['Eve Prototype Planet Diving Suit - Tail']
proxy = objects['Eve Prototype Planet Diving Suit - Tail Physics']
mesh = json.loads((work / 'planet-export/planet.mesh.json').read_text())
audit = json.loads((work / 'planet-export/planet.mesh.audit.json').read_text())
start = 0
for part in audit['parts']:
    if part['name'] == tail['name']:
        break
    start += part['points']
else:
    raise AssertionError('Tail part missing')
used = sorted({i for face in tail['faces'] for i in face})
assert len(used) == part['points'] == 840
original = np.array([tail['vertices'][i]['co'] for i in used])
target = np.array(mesh['points'][start:start+len(used)])
# Use the known export vertex order, then verify the fit against every point.
x = np.column_stack((original,np.ones(len(original))))
affine,_,rank,_ = np.linalg.lstsq(x,target,rcond=None)
assert rank == 4
errors = np.linalg.norm(x@affine-target,axis=1)
assert errors.max() < .02, f'Tail is not a matching affine surface: {errors.max()} cm'
positions = np.column_stack(([v['co'] for v in proxy['vertices']],np.ones(len(proxy['vertices']))))@affine
pins = [i for i,v in enumerate(proxy['vertices']) if v['weights'].get('Pin',0) >= .99]
assert pins == [0,1,2,3]
triangles = []
for face in proxy['faces']:
    assert len(face) == 4
    triangles.extend([[face[0],face[1],face[2]],[face[0],face[2],face[3]]])
report = {'author_sha256':hashlib.sha256(author_path.read_bytes()).hexdigest(),
    'scope':'Aligned authored proxy only; no native simulation or render mapping yet',
    'alignment_max_error_cm':float(errors.max()),'alignment_rms_cm':float(np.sqrt(np.mean(errors**2))),
    'affine':affine.tolist(),'positions_cm':positions.tolist(),'triangles':triangles,'pinned_vertices':pins,
    'source_render_vertices':used,'target_render_start':start}
out.write_text(json.dumps(report,indent=2)+'\n')
print('TAIL_PROXY_ALIGNED',report['alignment_max_error_cm'],len(positions),len(triangles))
