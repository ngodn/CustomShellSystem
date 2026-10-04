#!/usr/bin/env python3
"""Exercise hidden Astral visual creation and cleanup in a running trial build."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from css_live_snapshot import Probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x') as log:
        probe = Probe(log)
        source = probe.send('astral.source')
        if source.get('status') != 'captured':
            raise RuntimeError('Select an appearance with gameplay Genessa first')
        parent = source['components'][0]['component']
        owner = source['pawn']
        component_class = probe.send('find', path='/Script/Engine.SkeletalMeshComponent')
        probe.send('describe', target=owner, function='K2_GetComponentsByClass')
        before = probe.call(owner, 'K2_GetComponentsByClass', ComponentClass=component_class)
        pose = probe.send('load', path='/Game/CSS/SharedAssets/Astral/ABP_CopyPose.ABP_CopyPose_C')
        for _ in range(3):
            result = probe.send('astral.visual.probe', parent=parent, pose_class=pose)
            if not result.get('passed'):
                raise RuntimeError(result)
        after = probe.call(owner, 'K2_GetComponentsByClass', ComponentClass=component_class)
        if before != after:
            raise RuntimeError('Owned skeletal components changed after cleanup')
    print(json.dumps({'passed': True, 'iterations': 3, 'components_preserved': True}))


if __name__ == '__main__':
    main()
