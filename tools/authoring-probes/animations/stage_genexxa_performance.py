"""Build GeneXXa's fixed-physics adapter and recheck existing graph adapters."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve(strict=True)
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
MODE = os.environ.get('CSS_PERFORMANCE_MODE', 'create')
if MODE not in ('create', 'verify', 'evaluate'):
    raise ValueError('Expected create, verify or evaluate')
report = WORK / f'genexxa-{MODE}.json'
if report.exists():
    raise FileExistsError(report)
source_path = '/Game/CSS/GeneXXa/ABP_Secondary'
target_path = '/Game/CSS/Locomotion/Adapters/ABP_GeneXXa'
existing = json.loads((WORK.parent / 'shared-v1/performance-layers.json').read_text())['results']


def disk(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


protected = {str(disk(path)): digest(disk(path)) for path in
             [source_path] + [row[key] for row in existing for key in ('source', 'output')]}
results = []
for row in existing:
    result = unreal.CSSPerformanceLibrary.verify_performance_layer(
        unreal.load_asset(row['source']), unreal.load_asset(row['output']))
    if not result.startswith('PASS:'):
        raise ValueError(row['output'] + ': ' + result)
    results.append(dict(source=row['source'], output=row['output'], verification=result))

source = unreal.load_asset(source_path)
if MODE == 'create':
    if disk(target_path).exists():
        raise FileExistsError(target_path)
    target = unreal.CSSPerformanceLibrary.create_performance_layer(source, target_path)
else:
    protected[str(disk(target_path))] = digest(disk(target_path))
    target = unreal.load_asset(target_path)
if not target:
    raise ValueError('Cannot load or create GeneXXa adapter')
result = unreal.CSSPerformanceLibrary.verify_performance_layer(source, target)
if not result.startswith('PASS: 0 existing CSS controls,'):
    raise ValueError('Expected the audited fixed-physics graph: ' + result)
if MODE == 'create' and not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
    raise ValueError('Cannot save GeneXXa adapter')
results.append(dict(source=source_path, output=target_path, verification=result,
                    sha256=digest(disk(target_path))))
evaluations = []
if MODE == 'evaluate':
    base_path = '/Game/CSS/Locomotion/Eve/idle/AN_Default'
    catalogs = [WORK.parent / name / 'catalog.json' for name in ('shared-v1', 'l5p-adapter-check')]
    clips = [entry for catalog in catalogs
             for entry in json.loads(catalog.read_text())['animation_library']['entries']
             if entry['playback'] == 'once']
    if not clips or len({entry['id'] for entry in clips}) != len(clips):
        raise ValueError('Missing or duplicate moving clips')
    protected[str(disk(base_path))] = digest(disk(base_path))
    for entry in clips:
        if len(entry['assets']) != 1:
            raise ValueError('Expected one audited clip skeleton')
        path = entry['assets'][0]['asset'].split('.')[0]
        protected[str(disk(path))] = digest(disk(path))
        for mesh_path in ('/Game/CSS/GeneXXa/SK_Nunexxa_V6', '/Game/CSS/GeneXXa/SK_Genexxa_V6'):
            protected[str(disk(mesh_path))] = digest(disk(mesh_path))
            value = unreal.CSSPerformanceLibrary.evaluate_performance_layer(
                unreal.load_asset(mesh_path), unreal.load_asset(source_path),
                unreal.load_asset(target_path), unreal.load_asset(base_path), unreal.load_asset(path))
            if not value:
                raise ValueError('Evaluation failed: ' + mesh_path + ' ' + entry['id'])
            row = json.loads(value)
            if (len(row['cases']) != 9 or row.get('finger_bones_checked') != 38 or
                    row.get('performance', '').split('.')[0] != path):
                raise ValueError('Incomplete evaluation')
            if not all(case['body_passed'] and case['fingers_passed'] for case in row['cases']):
                raise ValueError('Body or fingers differ from original graph')
            row['id'] = entry['id']
            evaluations.append(row)
            report.write_text(json.dumps(dict(complete=False, results=results,
                                              evaluations=evaluations), indent=2) + '\n')
            unreal.SystemLibrary.collect_garbage()
if any(digest(Path(path)) != checksum for path, checksum in protected.items()):
    raise ValueError('Protected original or existing adapter changed')
report.write_text(json.dumps(dict(complete=True, results=results, protected=protected, evaluations=evaluations,
    scope='Compilation, retained graph nodes/defaults and fresh-load verification. '
          'Evaluations compare preview-world body/finger poses where requested. '
          'Runtime binding, world physics and live acceptance remain pending.'), indent=2) + '\n')
print('GENEXXA_PERFORMANCE', MODE, result, flush=True)
