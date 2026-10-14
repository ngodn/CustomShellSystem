"""Measure paired body-solver bypass/enable cases in the isolated UE editor."""
import json
import os
from pathlib import Path
import unreal

stage = Path(os.environ['CSS_MOTION_STAGE'])
output = stage/'body-cost.json'
if output.exists():
    raise FileExistsError(output)
blueprint = unreal.load_asset('/Game/CSS/eins0fx/EveSB/ABP_Secondary')
cases = []
for suffix in ('', '_Face'):
    mesh = unreal.load_asset('/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2'+suffix)
    for fps in (30, 60, 120):
        for repeat in range(4):
            for enabled in ((False, True) if repeat % 2 == 0 else (True, False)):
                raw = unreal.CSSBodyInputsLibrary.measure_body_cost(mesh, blueprint, fps, enabled, 600)
                if raw.startswith('ERROR:'):
                    (stage/'failure.json').write_text(json.dumps(dict(mesh=suffix, fps=fps,
                        repeat=repeat, enabled=enabled, error=raw))+'\n')
                    raise RuntimeError(raw)
                case = json.loads(raw)
                case['repeat'] = repeat
                cases.append(case)
                (stage/'progress.json').write_text(json.dumps(cases)+'\n')
                unreal.log(f'Body timing: {suffix or "body"} {fps} fps, repeat {repeat}, enabled {enabled}')
output.write_text(json.dumps(dict(cases=cases, passed=all(c['passed'] for c in cases)))+'\n')
