"""Build a bounded four-chain skirt trial from existing bones and fitted collision spheres."""
import json
import math
from pathlib import Path

work = Path(__file__).resolve().parents[2]/'work/eve26'
mesh = json.loads((work/'holiday-hip-bones.mesh.json').read_text())
bones = {bone['name']: bone for bone in mesh['bones']}
spheres = json.loads((work/'cloth-capsules.json').read_text())['spheres']
chains = []
for side, center in [('F',(0,-12,100)), ('B',(0,10,100)), ('L',(13,0,100)), ('R',(-13,0,100))]:
    contacts = sorted(spheres, key=lambda s: math.dist(s['center_cm'], center))[:16]
    bodies = []
    for segment in range(1,4):
        name = f'CSS_Cloth_Skirt_{side}_{segment:02}'
        child = bones[f'CSS_Cloth_Skirt_{side}_{segment+1:02}']
        assert child['parent'] == mesh['bones'].index(bones[name])
        bodies.append({'bone':name, 'box_extents':[4,1,1],
                       'joint_offset':[v*.5 for v in child['translation']],
                       'angular_min':[-4,-12,-12], 'angular_max':[4,12,12],
                       'collision_radius':.5})
    chains.append({'bodies':bodies,
                   'spheres':[{'bone':s['bone'],'offset':s['local_center_cm'],'radius':s['radius_cm']} for s in contacts],
                   'gravity_scale':.1, 'damping':.9, 'angular_spring':100,
                   'component_motion_scale':.1, 'simulation_space':'Component',
                   'preserve_rest_lengths':False})
out=work/'holiday-dynamics.json'
assert not out.exists()
out.write_text(json.dumps({'chains':chains,'scope':'Initial motion trial, not tuned or accepted. Collision coverage remains approximate.'},indent=2)+'\n')
