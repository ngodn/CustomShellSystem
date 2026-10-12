"""Run the retained native input fixture on a manifest-derived Eve mesh matrix.

UE 5.6.1 embedded Python. Source assets are mounted read-only by the launcher.
"""
import json
import os
from pathlib import Path
import unreal

stage = Path(os.environ['CSS_MOTION_STAGE'])
output = stage / 'graph-inputs.json'
if output.exists():
    raise FileExistsError(output)
cases = []
for row in json.loads((stage / 'matrix.json').read_text()):
    mesh = unreal.load_asset(row['mesh'])
    blueprint = unreal.load_asset(row['blueprint'])
    if not mesh or not blueprint:
        raise RuntimeError('Missing fixture asset: ' + str(row))
    actual = mesh.get_editor_property('post_process_anim_blueprint')
    if not actual or actual.get_path_name().removesuffix('_C') != blueprint.get_path_name():
        raise RuntimeError('Saved mesh secondary binding differs: ' + row['mesh'])
    for fps in (30, 60, 120):
        raw = unreal.CSSBodyInputsLibrary.evaluate(mesh, blueprint, fps)
        if raw.startswith('ERROR:'):
            (stage / 'failure.json').write_text(json.dumps(dict(row=row, fps=fps, error=raw)))
            raise RuntimeError(raw)
        cases.append(json.loads(raw))
        (stage / 'progress.json').write_text(json.dumps(cases) + '\n')
        unreal.log('Eve input check: {} {} fps'.format(row['variant'], fps))
output.write_text(json.dumps(dict(cases=cases, passed=all(c['passed'] for c in cases))) + '\n')
