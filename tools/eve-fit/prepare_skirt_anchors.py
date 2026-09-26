"""Prepare a bounded waist-anchor trial for the connected Holiday proxy."""
import json
from pathlib import Path

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
proxy = json.loads((WORK / 'skirt-weights/pelvis-proxy.json').read_text())
slot = 'MI_CH_P_EVE_Christmas_01_01.001'
settings = {'Anchor': 20., 'Falloff': 12., 'MaxDistance': 18.,
            'BendingStiffness': .12, 'AnimDriveStiffness': .05,
            'AnimDriveDamping': .3, 'DampingCoefficient': .15,
            'CollisionThickness': .3, 'FrictionCoefficient': .3,
            'GravityScale': 1., 'SelfCollision': 1.}
top = max(p[2] for p in proxy['positions'])
distances = []
for _, _, z in proxy['positions']:
    blend = max(0., min(1., (top-settings['Anchor']-z)/settings['Falloff']))
    distances.append(settings['MaxDistance']*blend*blend*(3.-2.*blend))
assert min(distances) == 0 and max(distances) == 18
assert 0 < sum(d == 0 for d in distances) < len(distances)
outputs = {
    'skirt-config.json': {slot: settings},
    'skirt-proxies.json': {'slots': {slot: proxy}},
    'skirt-anchors.json': {
        'pin_above_z_cm': top-settings['Anchor'],
        'full_motion_below_z_cm': top-settings['Anchor']-settings['Falloff'],
        'pinned_vertices': sum(d == 0 for d in distances),
        'movable_vertices': sum(d > 0 for d in distances),
        'max_distances_cm': distances,
        'scope': 'Initial waist-anchor trial using the V3 formula; not simulated or installed',
    },
}
for name in outputs:
    assert not (WORK/name).exists(), name
for name, content in outputs.items():
    (WORK/name).write_text(json.dumps(content, separators=(',', ':'))+'\n')
print({k:v for k,v in outputs['skirt-anchors.json'].items() if k != 'max_distances_cm'})
