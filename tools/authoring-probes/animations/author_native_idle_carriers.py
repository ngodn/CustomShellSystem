"""Create constant native idle carriers without changing existing gait blends."""
import hashlib
import json
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan = json.loads(Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).read_text())
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
if content != Path(plan['content']).resolve():
    raise ValueError('Wrong authoring project')
tools = unreal.AssetToolsHelpers.get_asset_tools()
native = unreal.load_asset(plan['native_skeleton'])
template = unreal.load_asset('/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/Studies/NL2/BS_Idle')
if not template or not native:
    raise ValueError('Missing reference assets')
inputs = [unreal.Vector(direction, speed, 0)
          for direction in (-190, -180, -135, -90, -45, -1, 0, 1, 45, 90, 135, 180, 190)
          for speed in (0, .1, 1, 15, 75, 150, 225, 300, 450, 650, 900, 1000)]
rows = []
requested = json.loads(os.environ.get('CSS_NATIVE_CARRIER_IDS', 'null'))
if requested is not None and (not isinstance(requested, list) or not requested or
        len(requested) != len(set(requested)) or
        not set(requested) <= {entry['id'] for entry in plan['entries']}):
    raise ValueError('Invalid carrier selection')
for entry in plan['entries']:
    if (entry['id'] not in requested if requested is not None else entry['category'] != 'idle'):
        continue
    clip = unreal.load_asset(entry['destination'])
    if not clip or clip.get_editor_property('skeleton') != native:
        raise ValueError('Missing native idle: ' + entry['id'])
    label = hashlib.sha256(entry['id'].encode()).hexdigest()[:12]
    output = entry['destination'].rsplit('/', 1)[0] + '/BS_' + clip.get_name().removeprefix('AN_')
    if unreal.EditorAssetLibrary.does_asset_exist(output):
        raise ValueError('Carrier already exists')
    blend = unreal.CSSRigAuditLibrary.clone_constant_idle_carrier(template, clip, output)
    if not blend:
        raise RuntimeError('Carrier creation rejected')
    probe = tools.duplicate_asset('BS_' + label, '/Game/CSS/AnimLab', blend)
    query = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(probe, inputs))
    for item in query['queries']:
        if not item['samples'] or any(s['animation'] != clip.get_path_name() for s in item['samples']):
            raise ValueError('Carrier evaluates a different clip')
    if blend.get_editor_property('skeleton') != native:
        raise ValueError('Carrier skeleton changed')
    samples = blend.get_editor_property('sample_data')
    if len(samples) != 4 or any(s.get_editor_property('animation') != clip or s.get_editor_property('rate_scale') != 1 for s in samples):
        raise ValueError('Carrier sample or playback rate changed')
    if not unreal.EditorAssetLibrary.save_loaded_asset(blend, False):
        raise RuntimeError('Cannot save carrier')
    path = content / (output.removeprefix('/Game/') + '.uasset')
    rows.append(dict(id=entry['id'], package=output, sequence=entry['destination'],
                     sha256=hashlib.sha256(path.read_bytes()).hexdigest(), queries=query))
if len(rows) != (len(requested) if requested is not None else 7):
    raise ValueError('Incomplete carrier selection')
(work / 'result.json').write_text(json.dumps(dict(authored=True, carriers=rows), indent=2) + '\n')
