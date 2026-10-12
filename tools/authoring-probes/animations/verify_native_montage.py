"""Check montage ownership without touching a running game or saving assets."""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_NATIVE_MONTAGE_WORK']).resolve()
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
assert WORK.resolve().is_relative_to((ROOT / 'CustomShellSystem/work/cine-animation-port').resolve())
assert not (WORK / 'result.json').exists()
mesh_path = '/Game/CSSAuthoring/DiagnosticReferences/SK_GameHuman1199_V1'
clip_path = '/Game/CSS/Locomotion/NativeHuman/Studies/NH2/AN_BlackHole'
paths = [CONTENT / (p.removeprefix('/Game/') + '.uasset') for p in (mesh_path, clip_path)]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
protected = {str(p): sha(p) for p in paths}
mesh, clip = unreal.load_asset(mesh_path), unreal.load_asset(clip_path)
assert mesh and clip
report = json.loads(unreal.CSSNativePerformanceLibrary.evaluate_montage_ownership(mesh, clip))
expected = {
    'CreateSlotAnimationAsDynamicMontage_WithBlendSettings': {
        'Asset', 'SlotNodeName', 'BlendInSettings', 'BlendOutSettings', 'InPlayRate',
        'LoopCount', 'InBlendOutTriggerTime', 'ReturnValue'},
    'Montage_Play': {'MontageToPlay', 'InPlayRate', 'ReturnValueType', 'InTimeToStartMontageAt',
                     'bStopAllMontages', 'ReturnValue'},
    'Montage_Stop': {'InBlendOutTime', 'Montage'},
    'Montage_SetPosition': {'Montage', 'NewPosition'},
    'Montage_IsActive': {'Montage', 'ReturnValue'},
}
report['signature_names_match'] = all(set(report['signatures'][name]) == fields for name, fields in expected.items())
report['return_enum_byte'] = report['signatures']['Montage_Play']['ReturnValueType'] == 1
report['protected_unchanged'] = all(sha(Path(p)) == checksum for p, checksum in protected.items())
report['protected'] = protected
(WORK / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
assert report['passed'] and len(report['cases']) == 18 and all(report['cases'].values()), report
assert report['signature_names_match'] and report['return_enum_byte'] and report['protected_unchanged'], report
print('PASS: 18 native montage lifecycle and post-process handoff cases and reflected parameter names.')
