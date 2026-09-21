"""Compare active body/hair/hand dynamics across the editor package relocation."""
import hashlib
import json
import math
import os
from pathlib import Path
import unreal

CSS = Path(__file__).resolve().parents[2]
OUT = Path(os.environ['CSS_CLOSURE_WORK']).resolve()
assert OUT.is_relative_to(CSS / 'work') and not (OUT / 'motion.json').exists()
load = lambda p: json.loads(p.read_text())
prior = load(OUT / 'verified.json')
assert prior['passed']
old_mesh = unreal.load_asset('/Game/CSSAuthoring/DiagnosticReferences/SK_HeelSupportsV45C')
new_mesh = unreal.load_asset('/Game/CSS/SeduXtress/SK_BlackPearl')
old_bp = unreal.load_asset('/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeHandB2ReferenceV2')
new_bp = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
curves = [p['shape'] for p in load(CSS / 'tools/authoring-probes/hand-transfer/left-finger-correctives-v1.json')['parameters']]


def compare(a, b, where=''):
    if isinstance(a, dict):
        assert isinstance(b, dict) and a.keys() == b.keys(), where
        for k in a:
            compare(a[k], b[k], where + '/' + k)
    elif isinstance(a, list):
        assert isinstance(b, list) and len(a) == len(b), where
        for i, (x, y) in enumerate(zip(a, b, strict=True)):
            compare(x, y, where + '/' + str(i))
    elif isinstance(a, (int, float)) and not isinstance(a, bool):
        assert math.isfinite(a) and math.isfinite(b) and abs(a - b) <= 1e-6, (where, a, b)
    else:
        assert a == b, (where, a, b)


inputs = {}
fixture_dir = CSS / 'work/grip-grounding-v1/hand-postprocess-motion-v2'
assert load(fixture_dir / 'report.json')['passed']
for alpha in ('0.0', '0.5', '1.0'):
    source = fixture_dir / f'alpha-{alpha}-input.json'
    document = source.read_text()
    inputs[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
    before = unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(old_mesh, old_bp, document, curves, False)
    after = unreal.CSSControlRigLibrary.evaluate_hand_post_process_candidate(new_mesh, new_bp, document, curves, False)
    (OUT / f'motion-old{alpha}.json').write_text(before + '\n')
    (OUT / f'motion-new{alpha}.json').write_text(after + '\n')
    a, b = json.loads(before), json.loads(after)
    assert a['moving_fixture'] and b['moving_fixture'] and a['fps'] == b['fps'] == 30
    assert len(a['samples']) == len(b['samples']) == 112
    compare(a, b, alpha)
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in prior['protected_hashes'].items())
(OUT / 'motion.json').write_text(json.dumps(dict(passed=True, frames=336, inputs=inputs,
    scope='Full old/new component outputs during active body/hair dynamics, hand resets and six public morphs. Editor execution only, not cooked or game execution.'), indent=2) + '\n')
print('CSS_MOVING_COPY_PASS', flush=True)
