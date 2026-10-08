"""Convert selected existing library entries in an isolated UE 5.6.1 project."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

work = Path(os.environ['CSS_NATIVE_LIBRARY_WORK']).resolve(strict=True)
plan_path = Path(os.environ['CSS_NATIVE_LIBRARY_PLAN']).resolve(strict=True)
plan = json.loads(plan_path.read_text())
selected = set(json.loads(os.environ['CSS_NATIVE_LIBRARY_IDS']))
entries = [e for e in plan['entries'] if e['id'] in selected]
if len(entries) != len(selected) or not entries:
    raise ValueError('Unknown or empty selection')
content = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())).resolve()
if content != Path(plan['content']).resolve():
    raise ValueError('Wrong authoring project')


def disk(package):
    if not package.startswith('/Game/') or '..' in package.split('/'):
        raise ValueError('Invalid asset path')
    return content / (package.removeprefix('/Game/') + '.uasset')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def snapshot(asset):
    raw = unreal.CSSRigAuditLibrary.snapshot_animation(asset)
    if raw.startswith('ERROR:'):
        raise RuntimeError(raw)
    return json.loads(raw)


for path, expected in plan['protected'].items():
    if sha(Path(path)) != expected:
        raise ValueError('Protected input changed: ' + path)
source, target, native, settings = [unreal.load_asset(plan[k]) for k in
                                     ('source_mesh', 'target_mesh', 'native_skeleton', 'compression')]
if not all((source, target, native, settings)):
    raise ValueError('Missing retarget dependency')
if unreal.CSSRetargetLibrary.refresh_diagnostic_virtual_bones(target) != 9:
    raise ValueError('Unexpected native virtual bone definitions')
if unreal.CSSRetargetLibrary.inspect_skeleton(target.get_editor_property('skeleton')) != unreal.CSSRetargetLibrary.inspect_skeleton(native):
    raise ValueError('Native and diagnostic reference differ')
if unreal.CSSRetargetLibrary.inspect_virtual_bones(target.get_editor_property('skeleton')) != unreal.CSSRetargetLibrary.inspect_virtual_bones(native):
    raise ValueError('Native virtual bone definitions differ')

tools = unreal.AssetToolsHelpers.get_asset_tools()


def create(name, kind, factory):
    package = '/Game/CSS/AnimLab/NativeLibrary'
    if unreal.EditorAssetLibrary.does_asset_exist(package + '/' + name):
        raise ValueError('Scratch asset already exists')
    asset = tools.create_asset(name, package, kind, factory)
    if not asset:
        raise RuntimeError('Cannot create retarget asset')
    return asset


rigs = []
for name, mesh in [('Source', source), ('Target', target)]:
    rig = create('IK_' + name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    if not ctrl.set_skeletal_mesh(mesh) or not ctrl.set_retarget_root('pelvis'):
        raise RuntimeError('Cannot configure IK rig')
    for chain, _, _, first, last in plan['chains']:
        if str(ctrl.add_retarget_chain(chain, first, last, 'None')) != chain:
            raise RuntimeError('Cannot add retarget chain')
    rigs.append(rig)
retarget = create('RT_Library', unreal.IKRetargeter, unreal.IKRetargetFactory())
ctrl = unreal.IKRetargeterController.get_controller(retarget)
mode = unreal.RetargetSourceOrTarget
ctrl.set_ik_rig(mode.SOURCE, rigs[0])
ctrl.set_ik_rig(mode.TARGET, rigs[1])
ctrl.set_preview_mesh(mode.SOURCE, source)
ctrl.set_preview_mesh(mode.TARGET, target)
ctrl.add_default_ops()
for chain, *_ in plan['chains']:
    if not ctrl.set_source_chain(chain, chain):
        raise RuntimeError('Cannot map chain')
ctrl.auto_align_all_bones(mode.TARGET)

rows = []
for entry in entries:
    output = entry['destination']
    if disk(output).exists():
        raise ValueError('Candidate already exists: ' + output)
    original = unreal.load_asset(entry['source'])
    if not original or original.get_editor_property('skeleton') != source.get_editor_property('skeleton'):
        raise ValueError('Source skeleton differs: ' + entry['id'])
    before = snapshot(original)
    label = hashlib.sha256(entry['id'].encode()).hexdigest()[:12]
    raw = unreal.CSSAnimationLibrary.retarget_clip(source, target, original, retarget,
        '/Game/CSS/AnimLab/AN_Native_' + label, True, True)
    if not raw:
        raise RuntimeError('Retarget rejected: ' + entry['id'])
    # The established exact-binding helper stages in its historical namespace.
    # Rename the new private asset to the canonical namespace before saving it.
    staging = '/Game/CSS/Locomotion/NativeHuman/Batch/AN_' + label
    candidate = unreal.CSSAnimationLibrary.bind_native_human_clip(raw, native, staging)
    if not candidate:
        raise RuntimeError('Native binding rejected: ' + entry['id'])
    bound = snapshot(candidate)
    if not tools.rename_assets([unreal.AssetRenameData(candidate, output.rsplit('/', 1)[0], output.rsplit('/', 1)[1])]):
        raise RuntimeError('Canonical rename failed')
    if snapshot(candidate) != bound or candidate.get_path_name().split('.', 1)[0] != output:
        raise ValueError('Canonical rename changed authored keys')
    candidate.set_editor_property('bone_compression_settings', settings)
    if not math.isclose(candidate.get_play_length(), original.get_play_length(), abs_tol=1e-6):
        raise ValueError('Retarget changed duration')
    for key in ('keys', 'frames', 'rate_numerator', 'rate_denominator'):
        if before[key] != bound[key]:
            raise ValueError('Retarget changed timing')
    if candidate.get_editor_property('retarget_source_asset') or candidate.get_editor_property('skeleton') != native:
        raise ValueError('Candidate retains diagnostic mesh binding')
    if not unreal.EditorAssetLibrary.save_loaded_asset(candidate, False):
        raise RuntimeError('Cannot save candidate')
    if snapshot(original) != before:
        raise ValueError('Source animation changed')
    snapshot_path = work / (label + '-authored.json')
    snapshot_path.write_text(json.dumps(bound, indent=2) + '\n')
    rows.append(dict(id=entry['id'], source=entry['source'], package=output,
        keys=bound['keys'], fps=[bound['rate_numerator'], bound['rate_denominator']],
        seconds=candidate.get_play_length(), tracks=len(bound['tracks']),
        exact_binding=True, rename_preserves_keys=True, sha256=sha(disk(output)),
        snapshot=str(snapshot_path), snapshot_sha256=sha(snapshot_path)))
    (work / 'progress.json').write_text(json.dumps(dict(completed=len(rows), total=len(entries), clips=rows), indent=2) + '\n')
    unreal.log('CSS_NATIVE_LIBRARY ' + entry['id'])
for path, expected in plan['protected'].items():
    if sha(Path(path)) != expected:
        raise ValueError('Protected input changed during retargeting')
(work / 'result.json').write_text(json.dumps(dict(authored=True, clips=rows,
    plan_sha256=sha(plan_path), scope='Native body candidates with original timing and exact diagnostic-to-game binding. Compression/cook, visible contact, facial capability and gameplay acceptance remain.'), indent=2) + '\n')
