"""Bind four native locomotion carriers without changing their interpolation."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / 'CustomShellSystem'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_NATIVE_LOCOMOTION_BLEND_WORK']).resolve()
SEQUENCES = Path(os.environ['CSS_NATIVE_LOCOMOTION_SEQUENCES']).resolve()
for path in (WORK, SEQUENCES):
    assert path.is_relative_to(REPO / 'work/cine-animation-port')
assert not (WORK / 'result.json').exists()
data = json.loads((SEQUENCES / 'result.json').read_text())
assert json.loads((SEQUENCES / 'exit.json').read_text())['exit_code'] == 0
assert data['authored'] and len(data['clips']) == 20
PACKAGE = data['package']
native = unreal.load_asset('/Game/Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton')
assert native


def disk(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(name, value):
    (WORK / name).write_text(json.dumps(value, indent=2) + '\n')


protected = dict(data['protected'])
mapped = {}
for row in data['clips']:
    path = disk(row['package'])
    assert sha(path) == row['sha256']
    protected[str(path)] = row['sha256']
    clip = unreal.load_asset(row['package'])
    assert clip and clip.get_editor_property('skeleton') == native
    mapped[row['source']] = clip

sources = {'Idle': '/Game/CSS/Eve/Anim/BS_I2_Idle',
           **{slot: '/Game/CSS/Eve/Anim/BS_S1_' + slot for slot in ('Walk', 'Jog', 'Sprint')}}
for path in sources.values():
    protected[str(disk(path))] = sha(disk(path))
write('protected.json', protected)
results = []
for slot, path in sources.items():
    source = unreal.load_asset(path)
    assert source
    samples = source.get_editor_property('sample_data')
    replacements = [mapped[s.get_editor_property('animation').get_path_name().split('.')[0]] for s in samples]
    output = PACKAGE + '/BS_' + slot
    assert not disk(output).exists()
    candidate = unreal.CSSNativePerformanceLibrary.bind_native_locomotion_blend(
        source, replacements, native, output)
    assert candidate, slot
    def axes(asset):
        return [[axis.get_editor_property(key) for key in
                 ('display_name', 'min', 'max', 'grid_num', 'snap_to_grid', 'wrap_input')]
                for axis in asset.get_editor_property('blend_parameters')]
    inputs = [unreal.Vector(direction, speed, 0)
              for direction in (-190, -180, -135, -90, -45, -1, 0, 1, 45, 90, 135, 180, 190)
              for speed in (0, .1, 1, 15, 75, 150, 225, 300, 450, 650, 900, 1000)]
    before = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(source, inputs))
    after = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(candidate, inputs))
    expected = json.loads(json.dumps(before))
    for query in expected['queries']:
        for sample in query['samples']:
            sample['animation'] = mapped[sample['animation'].split('.')[0]].get_path_name()
    assert after == expected, slot
    assert axes(candidate) == axes(source)
    write(slot.lower() + '-queries.json', dict(source=before, candidate=after, axes=axes(candidate)))
    assert unreal.EditorAssetLibrary.save_loaded_asset(candidate, False)
    results.append(dict(slot=slot, source=path, package=output, samples=len(samples),
                        queries=len(inputs), sha256=sha(disk(output))))
    write('progress.json', dict(completed=len(results), total=4, blends=results))

assert all(sha(Path(path)) == digest for path, digest in protected.items())
write('result.json', dict(passed=True, blends=results, protected=protected,
      sequence_report=str(SEQUENCES / 'result.json'),
      scope='Authored carriers and interpolation match. Fresh reload, cooked poses and runtime acceptance pending.'))
packages = (SEQUENCES / 'sequence-cook.txt').read_text().splitlines()
(WORK / 'cook.txt').write_text('\n'.join(packages + [r['package'] for r in results]) + '\n')
