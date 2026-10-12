"""Demonstrate regional preset loss from an entry-time snapshot after native reinit."""
import json
import os
from pathlib import Path
import unreal

stage = Path(os.environ['CSS_MOTION_STAGE'])
output = stage / 'restore-comparison.json'
if output.exists():
    raise FileExistsError(output)
blueprint = unreal.load_asset('/Game/CSS/eins0fx/EveSB/ABP_Secondary')
results = []
for suffix in ('', '_Face'):
    mesh = unreal.load_asset('/Game/CSS/eins0fx/EveSB/PeonyV2/Meshes/SK_PeonyV2' + suffix)
    raw = unreal.CSSBodyInputsLibrary.compare_restore_snapshot(mesh, blueprint)
    if raw.startswith('ERROR:'):
        raise RuntimeError(raw)
    result = json.loads(raw)
    if [c['firm_preserved'] for c in result['cases']] != [False, True]:
        raise AssertionError('Expected stale restore to lose Firm and refreshed restore to preserve it')
    results.append(dict(mesh=mesh.get_path_name(), **result))
output.write_text(json.dumps(results, indent=2) + '\n')
