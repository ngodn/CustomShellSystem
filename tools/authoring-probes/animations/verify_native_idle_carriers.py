"""Read back native idle carriers, their interpolation and rejection cases."""
import hashlib
import json
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan = json.loads(Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).read_text())
report_path = Path(os.environ['CSS_NATIVE_CARRIERS_REPORT']).resolve(strict=True)
report = json.loads(report_path.read_text())
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
if content != Path(plan['content']).resolve() or not report['authored']:
    raise ValueError('Wrong project or unverified carriers')
native = unreal.load_asset(plan['native_skeleton'])
template = unreal.load_asset('/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/Studies/NL2/BS_Idle')
tools = unreal.AssetToolsHelpers.get_asset_tools()
inputs = [unreal.Vector(direction, speed, 0)
          for direction in (-190, -180, -135, -90, -45, -1, 0, 1, 45, 90, 135, 180, 190)
          for speed in (0, .1, 1, 15, 75, 150, 225, 300, 450, 650, 900, 1000)]
protected = dict(plan['protected'])
results = []
for index, row in enumerate(report['carriers']):
    path = content / (row['package'].removeprefix('/Game/') + '.uasset')
    protected[str(path)] = row['sha256']
    blend = unreal.load_asset(row['package'])
    clip = unreal.load_asset(row['sequence'])
    if not blend or not clip or blend.get_editor_property('skeleton') != native:
        raise ValueError('Invalid saved native carrier')
    samples = blend.get_editor_property('sample_data')
    if len(samples) != 4 or any(s.get_editor_property('animation') != clip or s.get_editor_property('rate_scale') != 1 for s in samples):
        raise ValueError('Saved samples changed')
    probe = tools.duplicate_asset('BS_Check' + str(index), '/Game/CSS/AnimLab', blend)
    query = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(probe, inputs))
    if query != row['queries']:
        raise ValueError('Saved interpolation changed')
    if unreal.CSSRigAuditLibrary.clone_constant_idle_carrier(template, clip, row['package']):
        raise ValueError('Existing carrier overwrite was accepted')
    results.append(dict(id=row['id'], queries=len(inputs), unchanged=True))
reject = '/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/RejectedCarrier'
source_clip = unreal.load_asset(plan['entries'][0]['source'])
movement = unreal.load_asset('/Game/CSS/eins0fx/Shared/Locomotion/NativeHuman/Studies/NL2/BS_Walk')
if not movement or not source_clip:
    raise ValueError('Missing rejection fixtures')
cases = dict(
    missing_source=unreal.CSSRigAuditLibrary.clone_constant_idle_carrier(None, clip, reject) is None,
    mismatched_skeleton=unreal.CSSRigAuditLibrary.clone_constant_idle_carrier(template, source_clip, reject) is None,
    nonconstant_source=unreal.CSSRigAuditLibrary.clone_constant_idle_carrier(movement, clip, reject) is None,
    outside_namespace=unreal.CSSRigAuditLibrary.clone_constant_idle_carrier(template, clip, '/Game/BadCarrier') is None)
if not all(cases.values()):
    raise ValueError('Invalid carrier input accepted')
for path, expected in protected.items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != expected:
        raise ValueError('Protected input changed: ' + path)
(work / 'result.json').write_text(json.dumps(dict(passed=True, carriers=results, rejection_cases=cases,
    protected_unchanged=True), indent=2) + '\n')
