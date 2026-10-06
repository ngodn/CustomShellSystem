"""Preserve the older six-outfit physics graph while preparing body playback.

Run with UE 5.6.1 Python. This does not declare shared clips compatible with
the older skeleton or modify either skeleton's compatibility list.
"""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
MODE = os.environ.get('CSS_PERFORMANCE_MODE', 'create')
if MODE not in ('create', 'verify'):
    raise ValueError('Expected create or verify')
REPORT = WORK / f'seductress-{MODE}.json'
if REPORT.exists():
    raise FileExistsError(REPORT)

SOURCE = '/Game/CSS/CSS_SeduXtress_eins0fx_P/ABP_SeduXtress_BodyHairV42'
OUTPUT = '/Game/CSS/Locomotion/Adapters/ABP_Seductress'
SKELETON = '/Game/CSSAuthoring/Shared/Skeletons/SKEL_CSS_Base'
BASE = '/Game/CSS/SeduXtress/SK_BlackPearl2'
MESHES = json.loads((WORK / 'seductress/meshes.json').read_text())


def disk(package):
    if not package.startswith('/Game/') or '.' in package:
        raise ValueError('Expected a package path')
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


protected = {str(disk(path)): digest(disk(path)) for path in
             [SOURCE, SKELETON, BASE, '/Game/CSS/Shared/SKEL_Base', *MESHES]}
source = unreal.load_asset(SOURCE)
skeleton = unreal.load_asset(SKELETON)
if not source or not skeleton or source.get_editor_property('target_skeleton') != skeleton:
    raise ValueError('Source graph skeleton does not match the released binding')
if MODE == 'create':
    if disk(OUTPUT).exists():
        raise FileExistsError(OUTPUT)
    target = unreal.CSSPerformanceLibrary.create_performance_layer(source, OUTPUT)
else:
    protected[str(disk(OUTPUT))] = digest(disk(OUTPUT))
    target = unreal.load_asset(OUTPUT)
if not target:
    raise ValueError('Cannot load or create the older Seductress adapter')
verification = unreal.CSSPerformanceLibrary.verify_performance_layer(source, target)
if not verification.startswith('PASS:'):
    raise ValueError(verification)
if MODE == 'create' and not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
    raise ValueError('Cannot save adapter')

bindings = []
for path, exports in MESHES.items():
    decoded = [item for item in exports if item['Type'] == 'SkeletalMesh']
    if len(decoded) != 1:
        raise ValueError('Expected exactly one cooked mesh: ' + path)
    props = decoded[0]['Properties']
    if (props['Skeleton']['ObjectPath'].rsplit('.', 1)[0] != SKELETON or
            props['PostProcessAnimBlueprint']['ObjectPath'].rsplit('.', 1)[0] != SOURCE):
        raise ValueError('Unexpected released mesh binding: ' + path)
    mesh = unreal.load_asset(path)
    if not mesh or mesh.get_editor_property('skeleton') != skeleton:
        raise ValueError('Authoring mesh has a different skeleton: ' + path)
    post = mesh.get_editor_property('post_process_anim_blueprint')
    if not post or post.get_path_name() != SOURCE + '.' + SOURCE.rsplit('/', 1)[1] + '_C':
        raise ValueError('Authoring mesh has a different post-process: ' + path)
    bind = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(mesh))
    if not bind or len({bone['name'] for bone in bind}) != len(bind):
        raise ValueError('Invalid mesh bind pose: ' + path)
    bindings.append(dict(mesh=path, bones=bind))

baseline = json.loads(unreal.CSSRetargetLibrary.inspect_mesh_bind_pose(unreal.load_asset(BASE)))
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Protected source changed')
REPORT.write_text(json.dumps(dict(
    complete=True, source=SOURCE, output=OUTPUT, verification=verification,
    output_sha256=digest(disk(OUTPUT)), protected=protected,
    meshes=bindings, baseline_mesh=BASE, baseline_bones=baseline,
    scope='Graph structure/default preservation and authoring mesh bindings only. '
          'Retargeting, cooked graph comparison, playback and live acceptance remain pending.'
), indent=2) + '\n')
print('SEDUCTRESS_PERFORMANCE', MODE, verification, flush=True)
