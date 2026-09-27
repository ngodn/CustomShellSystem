"""Prepare an isolated, bounded Knitwear cloth experiment for native evaluation."""
import hashlib
import json
from pathlib import Path

work = Path(__file__).resolve().parents[2]/'work/eve26'
out = work/'knit-cloth1'
assert not out.exists()
proxy_path = work/'knit-proxy3.json'
proxy = json.loads(proxy_path.read_text())
slot = proxy['slots']['Collar-1']
influence = slot['authored_maps']['dForce Influence']
pin = slot['authored_maps']['dForce Pin']
limits = [0. if a <= 1e-7 else 2.*a*(1.-p) for a,p in zip(influence,pin,strict=True)]
assert any(v == 0 for v in limits) and any(v > .1 for v in limits)
slot['max_distances'] = limits
slot['backstop_distances'] = [0.]*len(limits)
slot['backstop_radii'] = [3. if v > 0 else 0. for v in limits]
proxy['scope'] = 'Native trial: authored influence and pin scale a 2 cm cap; zero-distance non-legacy backstop radius3 cm. Not accepted settings or a dForce solver translation.'
source = '/Game/CSS/EveTest/SK_KFit2'
target = '/Game/CSS/EveTest/SK_KCloth1'
config = {'Collar-1':dict(Iterations=8,BendingStiffness=.2,AnimDriveStiffness=.25,
    AnimDriveDamping=.4,DampingCoefficient=.25,CollisionThickness=.15,
    FrictionCoefficient=.2,GravityScale=1.,SelfCollision=0)}
out.mkdir()
(out/'proxy.json').write_text(json.dumps(proxy,separators=(',',':'))+'\n')
(out/'config.json').write_text(json.dumps(config,indent=2)+'\n')
(out/'copy.json').write_text(json.dumps(dict(mapping={source:target},copy=[source]),indent=2)+'\n')
(out/'receipt.json').write_text(json.dumps(dict(proxy_sha256=hashlib.sha256(proxy_path.read_bytes()).hexdigest(),
    mesh_source=source,mesh_target=target,collision_recipe='knit-spheres2.json',
    max_distance_cm=max(limits),zero_pins=sum(v==0 for v in limits),
    scope='Coarse sphere coverage is insufficient alone. Evaluate backstop, collision, render mapping, morph behavior and cost before adoption. Self-collision disabled only for initial diagnostic.'),indent=2)+'\n')
print((out/'receipt.json').read_text())
