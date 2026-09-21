"""UE 5.6.1: evaluate Eve clips through the real secondary-motion AnimBP."""
import hashlib
import json
import os
import re
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ.get('CSS_ANIM_WORK', str(ROOT/'CustomShellSystem/work/anim3'))).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
assert not (WORK/'component-result.json').exists()
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
protected = json.loads((WORK/'protected.json').read_text())
blueprint_file = content/'CSS/SeduXtress/ABP_Secondary.uasset'
protected[str(blueprint_file)] = hashlib.sha256(blueprint_file.read_bytes()).hexdigest()
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
blueprint = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
config = json.loads((WORK/'batch-config.json').read_text()) if (WORK/'batch-config.json').exists() else {}
revision = config.get('revision', '')
assert re.fullmatch(r'[A-Za-z0-9]{0,8}', revision)
tag = revision+'_' if revision else ''
results = []
batch = json.loads((WORK/'batch.json').read_text()) if (WORK/'batch.json').exists() else [
    ('Walk', ''), ('Jog', ''), ('Sprint', ''), ('Idle', '')]
assert 1 <= len(batch) <= 64 and len({label for label, _ in batch}) == len(batch)
for label, _ in batch:
    assert re.fullmatch(r'[A-Za-z][A-Za-z0-9]{0,15}', label)
    loops = 1 if label == 'Idle' else config.get('component_loops', 3)
    assert isinstance(loops, int) and 1 <= loops <= 3
    output = WORK/(label.lower()+'-component.json')
    assert not output.exists()
    animation = unreal.load_asset('/Game/CSS/AnimLab/RT_'+tag+label)
    assert animation
    value = unreal.CSSAnimationLibrary.evaluate_clip(mesh, animation, blueprint, loops)
    assert value, label
    data = json.loads(value)
    data['scope'] = 'Compressed animation on an isolated component with Eve secondary motion. Stationary owner, no game locomotion or gameplay validation.'
    output.write_text(json.dumps(data, separators=(',', ':'))+'\n')
    results.append(dict(clip=label, frames=len(data['frames']), loops=loops,
                        filters=[len(f['bones']) for f in data['filters']],
                        unaffected_position_cm=data['unaffected_position_cm'],
                        unaffected_angle_rad=data['unaffected_angle_rad']))
    (WORK/'component-progress.json').write_text(json.dumps(results, indent=2)+'\n')
    unreal.log('Eve component completed: '+label)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/'component-result.json').write_text(json.dumps(dict(passed=True, clips=results,
    protected=protected, scope='Offline compressed component execution, no installed-game acceptance.'), indent=2)+'\n')
