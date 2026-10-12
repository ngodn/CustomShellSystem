"""Compare retained Eve rigs in UE 5.6.1 without opening or controlling the game.

Run with the editor's Python 3.11. Only transient rig instances are changed.
The input motion is synthetic, so this is not gameplay acceptance.
"""
import hashlib
import json
import math
import os
from pathlib import Path
import sys

import unreal

ROOT = Path(__file__).resolve().parents[2]
AUTHOR = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
sys.path.insert(0, str(AUTHOR / 'tools'))
from probe_controlrig_chain import key, set_value, transform
from probe_controlrig_body import angle

STAGE = Path(os.environ['CSS_MOTION_STAGE'])
NAMES = ['brust001', 'brust002', 'butt001', 'butt002',
         'thigh_twist_02_l', 'thigh_twist_02_r', 'belly']
PACKAGES = {'current': '/Game/CSS/SeduXtress/CR_Body',
            'v42': '/Game/CSS/CSS_SeduXtress_eins0fx_P/CR_SeduXtress_BodyV42'}
PRESETS = {'firm': [(2.6, .65, .6)] * 2 + [(2.5, .65, .65)] * 2
                  + [(2.8, .72, .4)] * 2 + [(2.7, .68, .45)],
           'natural': [(2.15, .48, 1)] * 2 + [(2.1, .5, 1)] * 2
                     + [(2.35, .58, .75)] * 2 + [(2.2, .52, .85)]}


def value_text(value):
    if isinstance(value, dict):
        return '(' + ','.join(f'{k}={value_text(v)}' for k, v in value.items()) + ')'
    if isinstance(value, list):
        return '(' + ','.join(map(value_text, value)) + ')'
    return str(value)


def evaluate(bp, public, mode, preset, fps, frame_deltas=None):
    owner = unreal.SceneComponent()
    rig = bp.create_control_rig()
    if not rig.rename(outer=owner):
        raise RuntimeError('Could not assign transient owner')
    hierarchy = rig.get_hierarchy()
    for name in ('Offsets', 'Moments', 'Gravity', 'ContactsEnabled', 'ContactCenters',
                 'ContactAxesX', 'ContactAxesY', 'ContactAxesZ', 'ContactPointRadii', 'ContactActive'):
        set_value(rig, name, value_text(public['CSSBody' + name]))
    set_value(rig, 'UseRegionSettings', True)
    values = PRESETS['firm' if preset == 'off' else preset]
    for channel, name in enumerate(('RegionFrequencies', 'RegionDampingRatios', 'RegionMotionAmounts')):
        set_value(rig, name, value_text([v[channel] for v in values]))
    set_value(rig, 'RegionEnabled', value_text([preset != 'off'] * 7))
    if mode == 'current_old_limits':
        set_value(rig, 'Limits', value_text([math.radians(v) for v in [6, 6, 10, 10, 3, 3, 5]]))
        set_value(rig, 'MaxTravel', value_text([1.5, 1.5, 1, 1, .5, .5, .5]))
    tuning = {name: rig.get_variable_as_string(name) for name in
              ('Limits', 'MaxTravel', 'PlanesEnabled', 'ContactsEnabled', 'Moments',
               'RegionFrequencies', 'RegionDampingRatios', 'RegionMotionAmounts', 'RegionEnabled')}
    peak, final = [0.] * 7, [0.] * 7
    samples = []
    timing = []
    elapsed = 0.
    deltas = frame_deltas if frame_deltas is not None else [0.] + [1 / fps] * (4 * fps)
    for frame, delta in enumerate(deltas):
        elapsed += delta
        t = elapsed if frame_deltas is not None else frame / fps
        hierarchy.reset_pose_to_initial(unreal.RigElementType.BONE)
        pelvis = hierarchy.get_global_transform(key('pelvis'), True)
        # A smooth 10 cm translation for two seconds, followed by two seconds at rest.
        pelvis.translation += unreal.Vector(0, 0, 10 * math.sin(2 * math.pi * t) if t < 2 else 0)
        hierarchy.set_global_transform(key('pelvis'), pelvis, False, True)
        incoming = [transform(hierarchy.get_global_transform(key(n))) for n in NAMES]
        set_value(rig, 'Enabled', True)
        set_value(rig, 'UpdateSerial', frame)
        set_value(rig, 'ResetEpoch', 0)
        rig.set_delta_time(delta)
        if not rig.execute('Forwards Solve'):
            raise RuntimeError('Native body rig execution failed')
        outgoing = [transform(hierarchy.get_global_transform(key(n))) for n in NAMES]
        final = [angle(a['rotation'], b['rotation']) for a, b in zip(incoming, outgoing)]
        if not all(math.isfinite(v) for v in final):
            raise ValueError('Non-finite native output')
        if any(math.dist(a['translation'], b['translation']) > 1e-4 for a, b in zip(incoming, outgoing)):
            raise ValueError('Rig moved a helper pivot')
        peak = [max(a, b) for a, b in zip(peak, final)]
        samples.append(final)
        if frame_deltas is not None:
            timing.append(dict(seconds=elapsed, delta=delta,
                               resets=int(rig.get_variable_as_string('ResetCount')),
                               substeps=int(rig.get_variable_as_string('Substeps')),
                               total_steps=int(rig.get_variable_as_string('TotalSteps'))))
    if preset == 'off' and max(peak) > .001:
        raise ValueError('Disabled region still moves relative to its input')
    return dict(mode=mode, preset=preset, fps=fps, tuning=tuning,
                peak_degrees=peak, final_degrees=final, samples_degrees=samples,
                **({'timing': timing} if frame_deltas is not None else {}))


def main():
    output = STAGE / 'response.json'
    if output.exists():
        raise FileExistsError(output)
    baseline = ROOT / 'work/three-outfit-rig-physics/secondary-bindings5/report.json'
    raw = baseline.read_bytes()
    public = json.loads(raw)['graphs']['/Game/CSS/eins0fx/EveSB/ABP_Secondary']['control_defaults']
    rigs = {name: unreal.load_asset(path) for name, path in PACKAGES.items()}
    if not all(isinstance(bp, unreal.ControlRigBlueprint) for bp in rigs.values()):
        raise ValueError('Retained source rig is missing')
    result = dict(scope='Direct native authoring-rig response, synthetic input; no AnimBP or game test.',
                  baseline_sha256=hashlib.sha256(raw).hexdigest(), packages=PACKAGES, cases=[])
    for mode in ('current', 'current_old_limits', 'v42'):
        for preset in ('firm', 'natural', 'off'):
            for fps in (30, 60, 120):
                result['cases'].append(evaluate(rigs['v42' if mode == 'v42' else 'current'],
                                                public, mode, preset, fps))
                (STAGE / 'progress.json').write_text(json.dumps(result) + '\n')
                unreal.log(f'Body comparison completed: {mode} {preset} {fps}')
    output.write_text(json.dumps(result) + '\n')


if __name__ == '__main__':
    main()
