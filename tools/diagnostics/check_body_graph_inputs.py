"""Exercise Peony's saved AnimBP with one-time preset writes in a private editor."""
import json
import os
from pathlib import Path
import unreal

stage = Path(os.environ['CSS_MOTION_STAGE'])
output = stage / 'graph-inputs.json'
if output.exists():
    raise FileExistsError(output)
blueprint = unreal.load_asset('/Game/CSS/eins0fx/EveSB/ABP_Secondary')
cases = []
for suffix in ('', '_Face'):
    mesh = unreal.load_asset('/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2' + suffix)
    for fps in (30, 60, 120):
        raw = unreal.CSSBodyInputsLibrary.evaluate(mesh, blueprint, fps)
        if raw.startswith('ERROR:'):
            (stage / 'failure.json').write_text(json.dumps(dict(mesh=suffix, fps=fps, error=raw)) + '\n')
            raise RuntimeError(raw)
        case = json.loads(raw)
        cases.append(case)
        (stage / 'progress.json').write_text(json.dumps(cases) + '\n')
        unreal.log(f'Body input graph completed: {suffix or "body"} {fps} fps')
output.write_text(json.dumps(dict(cases=cases, passed=all(c['passed'] for c in cases))) + '\n')
