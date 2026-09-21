"""Prepare provisional direction/speed sample tables from corrected Eve tracks."""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from review_source_tracks import rotation

ROOT = Path(__file__).resolve().parents[4]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
assert args.output.resolve().is_relative_to(ROOT/'CustomShellSystem/work') and not args.output.exists()
source = ROOT/'CustomShellSystem/work/anim10/batch'
bind = json.loads((source/'target-bind.json').read_text())
names = [b['name'] for b in bind]
calibration = {}


def estimate(label):
    if label in calibration:
        return calibration[label]['speed_cm_s']
    data = json.loads((source/(label.lower()+'-motion.json')).read_text())
    samples = []
    for frame in data['frames']:
        pose = frame['pose']['Snapshot']
        assert pose['BoneNames'][:len(bind)] == names
        world = []
        for i, t in enumerate(pose['LocalTransforms'][:len(bind)]):
            matrix = np.eye(4)
            matrix[:3, :3] = rotation(list(t['Rotation'].values()))
            matrix[:3, 3] = list(t['Translation'].values())
            parent = bind[i]['parent']
            world.append(world[parent] @ matrix if parent >= 0 else matrix)
        samples.append([world[names.index(n)][:3, 3] for n in ('foot_l', 'foot_r')])
    positions = np.array(samples)
    speeds = []
    for foot in range(2):
        z = positions[:, foot, 2]
        velocity = np.gradient(positions[:, foot, :2], 1/data['fps'], axis=0)
        low = z <= np.quantile(z, .25)
        low[0] = low[-1] = False
        speeds.extend(np.linalg.norm(velocity[low], axis=1).tolist())
    value = float(np.median(speeds))
    assert math.isfinite(value) and 30 < value < 1000
    difference = positions[:-1, 0, 2] - positions[:-1, 1, 2]
    calibration[label] = dict(speed_cm_s=value, p10_cm_s=float(np.quantile(speeds, .1)),
                              p90_cm_s=float(np.quantile(speeds, .9)),
                              left_foot_high_phase=float(np.argmax(difference)/(len(positions)-1)))
    return value


directions = [(-180, 'B'), (-135, 'BL'), (-90, 'L'), (-45, 'FL'), (0, 'F'),
              (45, 'FR'), (90, 'R'), (135, 'BR'), (180, 'B')]
definitions = []
for slot, maximum, prefix in [('Walk', 300, 'W'), ('Jog', 650, 'J'), ('Sprint', 900, 'J')]:
    samples = []
    for direction, suffix in directions:
        label = prefix + suffix
        if direction == 0:
            label = slot
        elif slot == 'Sprint' and abs(direction) == 45:
            label = 'SL' if direction < 0 else 'SR'
        speed = estimate(label)
        for y in (0, maximum/4, maximum/2, maximum):
            rate = y/speed if y else 1.
            assert .05 <= rate <= 8
            samples.append(dict(direction=direction, speed=y, clip=label if y else 'Idle', rate=rate))
    definitions.append(dict(slot=slot, package='/Game/CSS/AnimLab/BS_M1_'+slot,
                            max_speed=maximum, samples=samples))
args.output.write_text(json.dumps(dict(schema=1, definitions=definitions, calibration=calibration,
    scope='Provisional offline blends. Rates estimate foot speed, not measured shoe contact or accepted cadence. '
          'Sprint lateral/back samples use directional run clips. Phase alignment and weapon/gameplay review remain open.'), indent=2)+'\n')
print(f'Prepared {len(definitions)} blends and {len(calibration)} stride estimates')
