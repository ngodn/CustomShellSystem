"""UE 5.6.1: copy verified sequences into the shared namespace without changing keys."""
import hashlib
import json
import os
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
CONTENT = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
MODE = os.environ.get('CSS_LIBRARY_MODE', 'create')
if MODE not in ('create', 'verify') or not WORK.is_relative_to(ROOT/'CustomShellSystem/work/cine-animation-port'):
    raise ValueError('Use an isolated animation work directory and create/verify mode')
plan = json.loads((WORK/'plan.json').read_text())


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk(package):
    return CONTENT/(package.removeprefix('/Game/')+'.uasset')


protected = dict(plan['receipts'])
for entry in plan['copies']:
    if not entry['destination'].startswith('/Game/CSS/Locomotion/') or '..' in entry['destination']:
        raise ValueError('Invalid output package')
    protected[str(disk(entry['source']))] = digest(disk(entry['source']))
    if MODE == 'verify':
        protected[str(disk(entry['destination']))] = digest(disk(entry['destination']))
    elif disk(entry['destination']).exists():
        raise FileExistsError(entry['destination'])
if any(digest(Path(path)) != sha for path, sha in protected.items()):
    raise ValueError('Input changed since preparation')
results = []
report = WORK/(MODE+'-result.json')
report.write_text(json.dumps(dict(complete=False, results=[]))+'\n')
for entry in plan['copies']:
    source = unreal.load_asset(entry['source'])
    if not isinstance(source, unreal.AnimSequence):
        raise ValueError('Source is not an animation sequence: '+entry['source'])
    target = (unreal.EditorAssetLibrary.duplicate_asset(entry['source'], entry['destination'])
              if MODE == 'create' else unreal.load_asset(entry['destination']))
    if not target:
        raise ValueError('Shared sequence missing: '+entry['destination'])
    result = unreal.CSSAnimationLibrary.compare_sequence_data(source, target)
    if not result.startswith('PASS:'):
        raise ValueError(entry['destination']+': '+result)
    keys = unreal.AnimationLibrary.get_num_keys(target)
    duration = unreal.AnimationLibrary.get_sequence_length(target)
    hz = 1/unreal.AnimationLibrary.get_time_at_frame(target, 1)
    if 'expected_keys' in entry and (keys != entry['expected_keys'] or abs(hz-entry['expected_hz']) > .0001
            or abs(duration-entry['expected_duration']) > .00001):
        raise ValueError('Shared animation timing differs: '+entry['destination'])
    if MODE == 'create' and not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
        raise ValueError('Cannot save shared animation')
    results.append(dict(id=entry['id'], asset=target.get_path_name(), result=result,
                        keys=keys, duration=duration, hz=hz, sha256=digest(disk(entry['destination']))))
    report.write_text(json.dumps(dict(complete=False, results=results), indent=2)+'\n')
    print('SHARED_SEQUENCE_PASS', entry['id'], flush=True)
if any(digest(Path(path)) != sha for path, sha in protected.items()):
    raise ValueError('Protected input changed')
report.write_text(json.dumps(dict(complete=True, results=results, protected=protected,
                                 installed=False, scope='Authored sequence equality. Cooking and live playback remain separate checks.'), indent=2)+'\n')
