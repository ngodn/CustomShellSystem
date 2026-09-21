"""UE 5.6.1: prepare Eve movement assets for cooking and timed playback review."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE = os.environ.get('CSS_MOVEMENT_MODE', 'create')
assert MODE in ('create', 'readback') and not (WORK/(MODE+'-result.json')).exists()
phase_work = ROOT/'CustomShellSystem/work/anim12'
assert json.loads((phase_work/'readback-phase-result.json').read_text())['passed']
assert json.loads((phase_work/'readback-result.json').read_text())['passed']
spec = json.loads((phase_work/'spec.json').read_text())
phase = json.loads((phase_work/'phase.json').read_text())['clips']
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected = json.loads((ROOT/'CustomShellSystem/work/anim11/protected.json').read_text())
for label, entry in phase.items():
    if entry['start_frame']:
        path = content/('CSS/AnimLab/RT_P1_'+label+'.uasset')
        protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
if MODE == 'create':
    (WORK/'protected.json').write_text(json.dumps(protected, indent=2)+'\n')
else:
    assert protected == json.loads((WORK/'protected.json').read_text())
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
blueprint = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
tools = unreal.AssetToolsHelpers.get_asset_tools()
directory = '/Game/CSS/Eve/Anim'
clips = {}
for label, entry in phase.items():
    source = unreal.load_asset('/Game/CSS/AnimLab/RT_'+('P1' if entry['start_frame'] else 'D2')+'_'+label)
    assert source
    path = directory+'/AN_'+label
    if MODE == 'create':
        assert not unreal.EditorAssetLibrary.does_asset_exist(path)
        clip = tools.duplicate_asset('AN_'+label, directory, source)
        assert clip and unreal.EditorAssetLibrary.save_loaded_asset(clip, False)
    else:
        clip = unreal.load_asset(path)
    assert clip and clip.get_editor_property('skeleton') == mesh.get_editor_property('skeleton')
    assert clip.get_play_length() == source.get_play_length()
    assert not clip.get_editor_property('enable_root_motion') and clip.get_editor_property('rate_scale') == 1
    clips[label] = clip

# Forward speeds come from the actual SB peaceful BlendSpace, not the rejected
# lowest-foot-quartile estimate. Match cycle cadence across directions.
source_samples = json.loads((ROOT/'CustomShellSystem/work/anim6/blend-sources.json').read_text())['IdleRun_BS_Peaceful2D']['SampleData']
nominal = {}
for slot, name in [('Walk','Proto_Walk'),('Jog','Proto_Run'),('Sprint','Proto_Sprint')]:
    values = [s['SampleValue']['X'] for s in source_samples if
        s.get('Animation', {}).get('ObjectName') == "AnimSequence'"+name+"'" and
        s['SampleValue']['Y'] == 0 and s['RateScale'] == 1]
    assert len(values) == 1 and values[0] > 0
    nominal[slot] = values[0]
definitions = []
for definition in spec['definitions']:
    slot = definition['slot']
    samples = definition['samples']
    points, rates, assets = [], [], []
    for sample in samples:
        # Idle belongs to the game graph. This override releases when stationary;
        # blending a seven-second idle into fast gait cycles distorts cadence.
        label = sample['clip']
        if sample['speed'] == 0:
            label = next(s['clip'] for s in samples if s['direction'] == sample['direction'] and s['speed'] > 0)
        clip = clips[label]
        rate = max(.05, sample['speed']/nominal[slot])*clip.get_play_length()/clips[slot].get_play_length()
        assert .05-1e-6 <= rate <= 3
        sample.update(clip=label, rate=rate, asset=clip.get_path_name())
        assets.append(clip); rates.append(rate); points.append(unreal.Vector(sample['direction'], sample['speed'], 0))
    path = directory+'/BS_'+slot
    if MODE == 'create':
        blend = unreal.CSSAnimationLibrary.create_movement_blend(mesh, assets, points, rates, definition['max_speed'], path)
        assert blend and unreal.EditorAssetLibrary.save_loaded_asset(blend, False)
    else:
        blend = unreal.load_asset(path)
    assert blend
    actual = blend.get_editor_property('sample_data')
    assert len(actual) == len(samples)
    for saved, sample in zip(actual, samples, strict=True):
        assert saved.get_editor_property('animation').get_path_name() == sample['asset']
        assert math.isclose(saved.get_editor_property('rate_scale'), sample['rate'], rel_tol=1e-6)
        assert saved.get_editor_property('sample_value') == unreal.Vector(sample['direction'], sample['speed'], 0)
    definition.update(package=path, nominal_forward_speed=nominal[slot])
    definitions.append(definition)
document = dict(definitions=definitions, scope='Candidate for cooking and live review; gameplay motion remains unaccepted.')
if MODE == 'create':
    (WORK/'spec.json').write_text(json.dumps(document, indent=2)+'\n')
else:
    assert document == json.loads((WORK/'spec.json').read_text())

cases = [('walk0','Walk',0,184),('walk22','Walk',22.5,150),('walkback','Walk',-179,150),
         ('jog0','Jog',0,540),('jogleft','Jog',-67.5,300),('jogback','Jog',180,240),
         ('sprint0','Sprint',0,800),('sprintright','Sprint',22.5,650),('sprintback','Sprint',-135,300)]
results = []
if MODE == 'readback':
    reference = unreal.load_asset('/Game/CSS/AnimLab/RT_D2_Walk')
    assert reference
    for name, slot, direction, speed in cases:
        blend = unreal.load_asset(directory+'/BS_'+slot)
        output = WORK/(name+'-component.json')
        data = json.loads(output.read_text()) if output.exists() else json.loads(
            unreal.CSSAnimationLibrary.evaluate_clip(mesh, reference, blueprint, 2,
                blend, unreal.Vector(direction, speed, 0), True))
        assert len(data['frames']) == 145 and data['advance_blend_clock']
        # Triangulation can assign neighboring animations different speed-row
        # mixtures. Use the engine's grouped weighted rates for its actual clock.
        query = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(blend,
            [unreal.Vector(direction, speed, 0)]))['queries'][0]
        grouped = {}
        for sample in query['samples']:
            weight, rate = grouped.get(sample['animation'], (0., 0.))
            grouped[sample['animation']] = weight+sample['weight'], rate+sample['weight']*sample['rate']
        period = sum(weight*unreal.load_asset(path).get_play_length()/(rate/weight)
                     for path, (weight, rate) in grouped.items() if weight > 0)
        nominal_period = clips[slot].get_play_length()/(speed/nominal[slot])
        errors = []
        for frame in data['frames']:
            difference = abs(frame['blend_normalized_time']-(frame['time']/period)%1)
            errors.append(min(difference, abs(1-difference)))
        assert max(errors) < .0001, (name, max(errors))
        if not output.exists():
            output.write_text(json.dumps(data, separators=(',', ':'))+'\n')
        results.append(dict(case=name, frames=145, cycle_seconds=period,
            nominal_cycle_seconds=nominal_period, clock_error=max(errors)))
        (WORK/'playback-progress.json').write_text(json.dumps(results, indent=2)+'\n')
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p,h in protected.items())
packages = [c.get_path_name().split('.')[0] for c in clips.values()]+[d['package'] for d in definitions]
(WORK/'cook.txt').write_text('\n'.join(packages)+'\n')
(WORK/(MODE+'-result.json')).write_text(json.dumps(dict(passed=True, packages=packages, cases=results), indent=2)+'\n')
