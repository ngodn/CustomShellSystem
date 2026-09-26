"""Compare freshly inspected native collision geometry with its input definition."""
import json
import math
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
source = json.loads((WORK/'cloth-capsules.json').read_text())
bodies = json.loads((WORK/'cloth-collision-native.json').read_text())
output = WORK/'cloth-collision-saved.json'
assert not output.exists()
count = 0
sphere_count = 0
for body in bodies:
    bone = body['boneName']
    capsules = body['aggGeom']['taperedCapsuleElems']
    sphere_count += len(body['aggGeom']['sphereElems'])
    expected = [p for p in source['connections'] if source['spheres'][p['sphere_indices'][0]]['bone'] == bone]
    assert len(capsules) == len(expected)
    for capsule, pair in zip(capsules, expected, strict=True):
        a,b = [source['spheres'][i] for i in pair['sphere_indices']]
        for key,value in [('radius0',a['radius_cm']),('radius1',b['radius_cm']),('length',math.dist(a['local_center_cm'],b['local_center_cm']))]:
            assert math.isclose(capsule[key],value,abs_tol=1e-4),(bone,key)
        r = capsule['rotation']
        pitch,yaw,roll = [math.radians(r[k]) for k in ('pitch','yaw','roll')]
        sp,cp,sy,cy,sr,cr = math.sin(pitch),math.cos(pitch),math.sin(yaw),math.cos(yaw),math.sin(roll),math.cos(roll)
        axis = (-(cr*sp*cy+sr*sy),cy*sr-cr*sp*sy,cr*cp)
        center = [capsule['center'][k] for k in ('x','y','z')]
        for sign,sphere in [(1,a),(-1,b)]:
            endpoint = [center[i]+sign*axis[i]*capsule['length']/2 for i in range(3)]
            assert math.dist(endpoint,sphere['local_center_cm'])<1e-4,(bone,endpoint)
        count += 1
connected = {i for pair in source['connections'] for i in pair['sphere_indices']}
assert sphere_count == len(source['spheres'])-len(connected)
assert count == len(source['connections'])
assert {r['boneName'] for r in bodies} == {s['bone'] for s in source['spheres']}
output.write_text(json.dumps({'body_count':len(bodies),'capsules':count,'standalone_spheres':sphere_count,
    'verified':'Saved bone identities, primitive counts and capsule radii, lengths and endpoint positions',
    'scope':'Native saved asset readback, not simulation or runtime acceptance'},indent=2)+'\n')
print(output.read_text())
