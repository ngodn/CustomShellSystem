"""UE 5.6.1: create/read back provisional Eve blends and query engine weights."""
import hashlib
import json
import math
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
assert WORK.is_relative_to(ROOT/'CustomShellSystem/work')
MODE = os.environ.get('CSS_MOVEMENT_MODE', 'create')
assert MODE in ('create', 'readback') and not (WORK/(MODE+'-result.json')).exists()
spec = json.loads((WORK/'spec.json').read_text())
assert spec['schema'] == 1 and len(spec['definitions']) == 3
prior = ROOT/'CustomShellSystem/work/anim10/batch'
protected = json.loads((prior/'protected.json').read_text())
content = ROOT/'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
for label, _ in json.loads((prior/'batch.json').read_text()):
    p = content/('CSS/AnimLab/RT_D2_'+label+'.uasset')
    protected[str(p)] = hashlib.sha256(p.read_bytes()).hexdigest()
if MODE == 'create':
    if (WORK/'protected.json').exists():
        assert protected == json.loads((WORK/'protected.json').read_text())
    (WORK/'protected.json').write_text(json.dumps(protected, indent=2)+'\n')
else:
    assert protected == json.loads((WORK/'protected.json').read_text())
mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl2')
results = []
for definition in spec['definitions']:
    samples = definition['samples']
    animations = [unreal.load_asset('/Game/CSS/AnimLab/RT_D2_'+s['clip']) for s in samples]
    assert all(animations)
    points = [unreal.Vector(s['direction'], s['speed'], 0) for s in samples]
    rates = [s['rate'] for s in samples]
    if MODE == 'create':
        if unreal.EditorAssetLibrary.does_asset_exist(definition['package']):
            blend = unreal.load_asset(definition['package'])
        else:
            blend = unreal.CSSAnimationLibrary.create_movement_blend(mesh, animations, points, rates,
                definition['max_speed'], definition['package'])
            assert blend and unreal.EditorAssetLibrary.save_loaded_asset(blend, False)
    else:
        blend = unreal.load_asset(definition['package'])
        assert blend
    assert blend.get_editor_property('skeleton') == mesh.get_editor_property('skeleton')
    actual = blend.get_editor_property('sample_data')
    assert len(actual) == len(samples)
    for a, wanted in zip(actual, samples):
        assert a.get_editor_property('animation').get_name() == 'RT_D2_'+wanted['clip']
        assert math.isclose(a.get_editor_property('rate_scale'), wanted['rate'], rel_tol=1e-6)
        assert a.get_editor_property('sample_value') == unreal.Vector(wanted['direction'], wanted['speed'], 0)
    # Include every authored knot, interiors, clamped speed and wrapped seams.
    queries = [(s['direction'], s['speed']) for s in samples]
    queries += [(x, y) for x in (-540, -181, -180, -179, -67.5, -22.5, 0, 22.5, 67.5, 179, 180, 181, 540)
                for y in (-1, definition['max_speed']/3, definition['max_speed']*2/3, definition['max_speed']+1)]
    report = json.loads(unreal.CSSAnimationLibrary.inspect_movement_blend(blend,
        [unreal.Vector(x, y, 0) for x, y in queries]))
    assert len(report['queries']) == len(queries)
    for i, wanted in enumerate(samples):
        weights = report['queries'][i]['samples']
        assert sum(w['weight'] for w in weights if w['animation'].split('.')[-1] == 'RT_D2_'+wanted['clip']) > .9999
    def weights_at(direction, speed):
        query = next(q for q in report['queries'] if q['direction'] == direction and q['speed'] == speed)
        result = {}
        for s in query['samples']:
            key = (s['animation'], round(s['rate'], 6))
            result[key] = result.get(key, 0) + s['weight']
        return {key: value for key, value in result.items() if value > 1e-7}
    for y in (-1, definition['max_speed']/3, definition['max_speed']*2/3, definition['max_speed']+1):
        baseline = weights_at(-180, y)
        for x in (180, -540, 540):
            candidate = weights_at(x, y)
            assert baseline.keys() == candidate.keys()
            assert all(abs(baseline[k]-candidate[k]) < 1e-5 for k in baseline)
    path = WORK/(MODE+'-'+definition['slot'].lower()+'.json')
    path.write_text(json.dumps(report, indent=2)+'\n')
    if MODE == 'readback':
        assert report == json.loads((WORK/('create-'+definition['slot'].lower()+'.json')).read_text())
    results.append(dict(slot=definition['slot'], samples=len(samples), queries=len(queries)))
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK/(MODE+'-result.json')).write_text(json.dumps(dict(passed=True, blends=results,
    scope='Saved sample structure and engine interpolation weights, not motion/contact or game acceptance.'), indent=2)+'\n')
