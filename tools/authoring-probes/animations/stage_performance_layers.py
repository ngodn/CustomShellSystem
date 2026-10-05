"""Create separate explicit-time layers for each installed outfit graph family."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
MODE = os.environ.get('CSS_PERFORMANCE_MODE', 'create')
if MODE not in ('create', 'verify'):
    raise ValueError('Expected create or verify')
report_name = os.environ.get('CSS_PERFORMANCE_REPORT',
                            'performance-layers.json' if MODE == 'create' else 'performance-layers-verify.json')
if Path(report_name).name != report_name or not report_name.endswith('.json'):
    raise ValueError('Expected a report filename')
report = WORK / report_name
if report.exists():
    raise FileExistsError(report)
sources = {
    'Eve': '/Game/CSS/AnimLab/ABP_Idle',
    'EveBikini': '/Game/CSS/EveTest/ABP_BikiniFeet2',
    'EveKnit': '/Game/CSS/EveTest/ABP_KnitFeet1',
    'Genessa': '/Game/CSS/UnholyGenessa/ABP_Wings2',
    'CommanderWhite': '/Game/CSS/CWTest/ABP_BodyHairB4',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk(package):
    return CONTENT / (package.removeprefix('/Game/') + '.uasset')


protected = {str(disk(path)): digest(disk(path)) for path in sources.values()}
results = []
for name, path in sources.items():
    output = '/Game/CSS/Locomotion/Adapters/ABP_' + name
    if MODE == 'create' and disk(output).exists():
        raise FileExistsError(output)
    source = unreal.load_asset(path)
    if MODE == 'verify':
        protected[str(disk(output))] = digest(disk(output))
    target = (unreal.CSSPerformanceLibrary.create_performance_layer(source, output)
              if MODE == 'create' else unreal.load_asset(output))
    if not target:
        raise ValueError('Cannot create performance graph for ' + path)
    verification = unreal.CSSPerformanceLibrary.verify_performance_layer(source, target)
    if not verification.startswith('PASS:'):
        raise ValueError(output + ': ' + verification)
    if MODE == 'create' and not unreal.EditorAssetLibrary.save_loaded_asset(target, False):
        raise ValueError('Cannot save ' + output)
    results.append(dict(source=path, output=output, sha256=digest(disk(output)), verification=verification))
    report.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
    print('PERFORMANCE_LAYER', MODE, name, verification, flush=True)
if any(digest(Path(path)) != sha for path, sha in protected.items()):
    raise ValueError('An original post-process asset changed')
report.write_text(json.dumps(dict(complete=True, contract='explicit-weight-v1', results=results, protected=protected,
                                 scope='Editor compilation and inactive defaults only. Runtime integration and physics acceptance remain pending.'), indent=2) + '\n')
