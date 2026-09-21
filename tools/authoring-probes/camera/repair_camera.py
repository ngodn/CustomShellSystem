"""UE 5.6.1: restore animated camera helpers without changing body retargeting."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import unreal

ROOT = Path(__file__).resolve().parents[4]
WORK = Path(os.environ['CSS_CAMERA_WORK']).resolve()
assert WORK.is_relative_to(ROOT / 'CustomShellSystem/work')
MODE = os.environ.get('CSS_CAMERA_MODE', 'repair')
assert MODE in ('repair', 'verify')
PACKAGE = '/Game/CSS/Shared/SKEL_Base'
CONTENT = ROOT / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
FILE = CONTENT / 'CSS/Shared/SKEL_Base.uasset'
HELPERS = ('camera_pivot', 'camera_target')
skeleton = unreal.load_asset(PACKAGE)
assert skeleton
inspect = lambda: json.loads(unreal.CSSRetargetLibrary.inspect_skeleton(skeleton))
current = inspect()
if MODE == 'repair':
    assert not (WORK / 'skeleton-before.json').exists()
    shutil.copy2(FILE, WORK / 'SKEL_Base.uasset')
    (WORK / 'skeleton-before.json').write_text(json.dumps(current, indent=2))
    protected = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in (CONTENT / 'CSS').rglob('*.uasset') if p != FILE}
    (WORK / 'protected.json').write_text(json.dumps(protected, indent=2))
    assert unreal.CSSRetargetLibrary.set_translation_retargeting(
        skeleton, list(HELPERS), unreal.BoneTranslationRetargetingMode.ANIMATION)
    assert unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False)
before = json.loads((WORK / 'skeleton-before.json').read_text())
after = inspect()
assert len(before) == len(after) == 379
changed = []
for old, new in zip(before, after):
    expected = dict(old)
    if old['name'] in HELPERS:
        expected['translation_mode'] = unreal.BoneTranslationRetargetingMode.ANIMATION.value
        changed.append(old['name'])
    assert new == expected, old['name']
assert set(changed) == set(HELPERS)
assert not skeleton.get_editor_property('use_retarget_modes_from_compatible_skeleton')
protected = json.loads((WORK / 'protected.json').read_text())
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
(WORK / (MODE + '-result.json')).write_text(json.dumps(dict(
    passed=True, changed=changed, bone_count=len(after), protected_assets=len(protected),
    skeleton_sha256=hashlib.sha256(FILE.read_bytes()).hexdigest(),
    scope='Editor skeleton settings and unchanged references; cooked and live verification required.'), indent=2))
(WORK / 'cook.txt').write_text(PACKAGE + '\n')
