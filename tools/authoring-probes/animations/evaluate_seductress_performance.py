"""Evaluate an audition through all six original Seductress post-process graphs."""
import hashlib
import json
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
REVISION = os.environ['CSS_RETARGET_REVISION']
if not re.fullmatch(r'S[1-9][0-9]{0,2}', REVISION):
    raise ValueError('Expected a numbered audition revision')
REPORT = WORK / 'adapter-evaluation.json'
if REPORT.exists():
    raise FileExistsError(REPORT)
receipt = json.loads((WORK / 'verify-result.json').read_text())
if not receipt['complete']:
    raise ValueError('Fresh saved/repeated readback is incomplete')
SOURCE = '/Game/CSS/CSS_SeduXtress_eins0fx_P/ABP_SeduXtress_BodyHairV42'
LAYER = '/Game/CSS/Locomotion/Adapters/ABP_Seductress'
meshes = json.loads((WORK.parent / 'expanded-outfit-inventory/seductress/meshes.json').read_text())


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk(path):
    return CONTENT / (path.removeprefix('/Game/') + '.uasset')


protected = dict(receipt['protected'])
for path in (SOURCE, LAYER, *meshes):
    protected[str(disk(path))] = digest(disk(path))
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Verified input changed')
clips = []
for row in receipt['results']:
    path = '/Game/CSS/AnimLab/RT_' + REVISION + '_' + row['name']
    target = '/Game/CSS/Locomotion/Studies/Seductress' + REVISION + '/AN_' + row['name']
    if disk(target).exists() or unreal.EditorAssetLibrary.does_asset_exist(target):
        raise FileExistsError(target)
    original = unreal.load_asset(path)
    clip = unreal.EditorAssetLibrary.duplicate_asset(path, target)
    if not original or not clip:
        raise ValueError('Missing audition: ' + path)
    comparison = unreal.CSSAnimationLibrary.compare_sequence_data(original, clip)
    if not comparison.startswith('PASS:') or not unreal.EditorAssetLibrary.save_loaded_asset(clip, False):
        raise ValueError('Cannot preserve audition data: ' + comparison)
    clips.append((row['name'], clip))
    protected[str(disk(target))] = digest(disk(target))
source, layer = unreal.load_asset(SOURCE), unreal.load_asset(LAYER)
results = []
for index, (name, clip) in enumerate(clips):
    base = clips[(index + 1) % len(clips)][1]
    for mesh_path in meshes:
        value = unreal.CSSPerformanceLibrary.evaluate_performance_layer(
            unreal.load_asset(mesh_path), source, layer, base, clip)
        if not value:
            raise ValueError('Evaluation failed: ' + mesh_path + ' ' + name)
        row = json.loads(value)
        if (len(row['cases']) != 9 or row['finger_bones_checked'] != 38 or
                not all(c['body_passed'] and c['fingers_passed'] for c in row['cases'])):
            raise ValueError('Body/finger comparison failed: ' + mesh_path + ' ' + name)
        results.append(row)
        REPORT.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
        unreal.SystemLibrary.collect_garbage()
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Protected input changed')
REPORT.write_text(json.dumps(dict(complete=True, results=results, protected=protected,
    scope='Audition playback insertion compared with the original graph, including full clip and restoration cases. '
          'This does not accept the retargeted motion, contact, customization extremes or gameplay physics.'
), indent=2) + '\n')
