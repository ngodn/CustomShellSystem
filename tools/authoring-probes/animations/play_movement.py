"""UE 5.6.1: advance candidate blends with the engine clock and real secondary rigs."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
assert not (WORK/'playback-result.json').exists()
protected = json.loads((WORK/'protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
blueprint = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
reference = unreal.load_asset('/Game/CSS/AnimLab/RT_D2_Walk')
spec = json.loads((WORK/'spec.json').read_text())
definition = spec['definitions'][0]
samples = definition['samples']
animations = [unreal.load_asset('/Game/CSS/AnimLab/RT_D2_'+s['clip']) for s in samples]
points = [unreal.Vector(s['direction'], s['speed'], 0) for s in samples]
rates = [s['rate'] for s in samples]
wrong_mesh = unreal.load_asset('/Game/CSS/AnimLab/SK_D1_EveSource')
assert wrong_mesh
negative = []
for name in ('Rate', 'Duplicate', 'Corner', 'Skeleton', 'Existing'):
    ps, rs = list(points), list(rates)
    target = wrong_mesh if name == 'Skeleton' else mesh
    package = '/Game/CSS/AnimLab/BS_M1_Bad'+name
    if name == 'Rate':
        rs[0] = 0
    elif name == 'Duplicate':
        ps[-1] = ps[0]
    elif name == 'Corner':
        ps[-1] = unreal.Vector(179, definition['max_speed'], 0)
    elif name == 'Existing':
        package = definition['package']
    rejected = unreal.CSSAnimationLibrary.create_movement_blend(target, animations, ps, rs,
        definition['max_speed'], package)
    assert not rejected, name
    if name != 'Existing':
        assert not unreal.EditorAssetLibrary.does_asset_exist(package)
    negative.append(name)

# The new clock option must not change the existing sequence sampler.
baseline = json.loads((ROOT/'CustomShellSystem/work/anim10/batch/walk-component.json').read_text())
regression = json.loads(unreal.CSSAnimationLibrary.evaluate_clip(mesh, reference, blueprint, 1))
assert not regression['advance_blend_clock']
for key in regression.keys()-{'advance_blend_clock'}:
    assert regression[key] == baseline[key], key

cases = [('walk0', 'Walk', 0, 184), ('walk22', 'Walk', 22.5, 150), ('walkback', 'Walk', -179, 150),
         ('jog0', 'Jog', 0, 540), ('jogleft', 'Jog', -67.5, 300), ('jogback', 'Jog', 180, 240),
         ('sprint0', 'Sprint', 0, 800), ('sprintright', 'Sprint', 22.5, 650), ('sprintback', 'Sprint', -135, 300)]
results = []
for name, slot, direction, speed in cases:
    output = WORK/(name+'-component.json')
    assert not output.exists()
    blend = unreal.load_asset('/Game/CSS/AnimLab/BS_M1_'+slot)
    assert blend
    query = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(blend,
        [unreal.Vector(direction, speed, 0)]))['queries'][0]
    grouped = {}
    for sample in query['samples']:
        weight, weighted_rate = grouped.get(sample['animation'], (0, 0))
        grouped[sample['animation']] = weight+sample['weight'], weighted_rate+sample['weight']*sample['rate']
    period = 0.
    for path, (weight, weighted_rate) in grouped.items():
        if weight <= 0:
            continue
        asset = unreal.load_asset(path)
        assert asset
        period += asset.get_play_length() / (weighted_rate/weight) * weight
    assert period > 0
    data = json.loads(unreal.CSSAnimationLibrary.evaluate_clip(mesh, reference, blueprint, 2,
        blend, unreal.Vector(direction, speed, 0), True))
    assert data['advance_blend_clock'] and len(data['frames']) == 145
    maximum = 0.
    for frame in data['frames']:
        actual = frame['blend_normalized_time']
        assert 0 <= actual <= 1
        expected = (frame['time']/period) % 1.
        difference = abs(actual-expected)
        maximum = max(maximum, min(difference, abs(1.-difference)))
    assert maximum < .0001, (name, period, maximum)
    assert data['frames'][0]['upstream'] != data['frames'][-1]['upstream']
    data['scope'] = 'Engine-clock blend playback on a stationary isolated component. Not gameplay, foot-contact or weapon-layer acceptance.'
    output.write_text(json.dumps(data, separators=(',', ':'))+'\n')
    results.append(dict(case=name, slot=slot, direction=direction, speed=speed, frames=len(data['frames']),
                        expected_cycle_seconds=period, normalized_clock_error=maximum))
    (WORK/'playback-progress.json').write_text(json.dumps(results, indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/'playback-result.json').write_text(json.dumps(dict(passed=True, cases=results,
    negative=negative, sequence_regression_exact=True,
    scope='Engine timing and component execution; visual, transition, weapon and game checks remain.'), indent=2)+'\n')
