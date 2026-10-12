"""Run native body-rig fixtures with variable frame times, without touching the game."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_body_rig_response import ROOT, STAGE, PACKAGES, evaluate


def deltas(pattern):
    result = [0.]
    elapsed = 0.
    while elapsed < 4 - 1e-9:
        delta = min(pattern[(len(result)-1) % len(pattern)], 4-elapsed)
        result.append(delta)
        elapsed += delta
    return result


def main():
    output = STAGE / 'frame-variation.json'
    if output.exists():
        raise FileExistsError(output)
    baseline = ROOT / 'work/three-outfit-rig-physics/secondary-bindings5/report.json'
    raw = baseline.read_bytes()
    public = json.loads(raw)['graphs']['/Game/CSS/eins0fx/EveSB/ABP_Secondary']['control_defaults']
    rigs = {name: unreal.load_asset(path) for name, path in PACKAGES.items()}
    if not all(isinstance(bp, unreal.ControlRigBlueprint) for bp in rigs.values()):
        raise RuntimeError('Retained rigs are missing')
    patterns = {'steady_30': [1/30],
                'uneven_no_hitch': [.018, .037, .029, .056, .041],
                'isolated_hitch': [1/30]*29 + [.077],
                'repeated_hitches': [.035, .046, .077, .033, .058, .085]}
    result = dict(scope='Synthetic frame-time patterns and input motion, not a replay of game frames',
                  baseline_sha256=hashlib.sha256(raw).hexdigest(), patterns=patterns, cases=[])
    for mode in ('current', 'v42'):
        for preset in ('firm', 'natural', 'off'):
            for name, pattern in patterns.items():
                row = evaluate(rigs[mode], public, mode, preset, None, deltas(pattern))
                row['pattern'] = name
                result['cases'].append(row)
                (STAGE / 'progress.json').write_text(json.dumps(result)+'\n')
                unreal.log(f'Frame variation completed: {mode} {preset} {name}')
    output.write_text(json.dumps(result)+'\n')


if __name__ == '__main__':
    main()
