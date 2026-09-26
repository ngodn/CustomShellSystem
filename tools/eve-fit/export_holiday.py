"""Export the fitted Holiday body and garments to a private cloth-development package."""
import hashlib
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
WORK = ROOT / 'CustomShellSystem/work/eve26'
sys.path.insert(0, str(MOD / 'tools'))
sys.path.insert(0, str(MOD / 'gemini-work'))
from export_variant_clean import export_variant

blend = WORK / 'holiday-f10.blend'
output = WORK / 'holiday.mesh.json'
assert not output.exists(), output
before = hashlib.sha256(blend.read_bytes()).hexdigest()
# The exporter requires this measured legacy-hair audit even for garment-only exports.
audit_source = MOD / 'work/nextgen-audit/hair-transform.json'
audit_copy = WORK / 'nextgen-audit/hair-transform.json'
audit_copy.parent.mkdir(exist_ok=True)
if audit_copy.exists():
    assert audit_copy.read_bytes() == audit_source.read_bytes()
else:
    shutil.copyfile(audit_source, audit_copy)

export_variant(
    blend_path=blend,
    refskel_path=MOD / 'authoring/reference/SKEL_CSS_Base.refskel.json',
    output_path=output,
    target_objects=['Eve Body'] + [
        'Eve Christmas - ' + part for part in ('Dress', 'Arms', 'Legs', 'Panties')
    ],
    mesh_package='/Game/CSS/EveTest/SK_Holiday',
    skeleton_package='/Game/CSS/EveTest/SKEL_Holiday_Import',
    bind_pose_path=MOD / 'work/CSS_SeduXtress_ArmRestV44B2.bindpose.json',
    left_hand_correctives=True,
)
after = hashlib.sha256(blend.read_bytes()).hexdigest()
assert before == after, 'Export changed the saved candidate'
receipt = {
    'candidate_sha256': before,
    'source_unchanged': True,
    'stage': 'Private cloth-development export, not a complete outfit or release',
    'shared_skeleton': '/Game/CSS/Shared/SKEL_Base',
    'shared_skeleton_modified': False,
    'omitted': ['hair', 'hat', 'earrings', 'footwear'],
    'exporter_sha256': hashlib.sha256(
        (MOD / 'gemini-work/export_variant_clean.py').read_bytes()
    ).hexdigest(),
}
(WORK / 'holiday.export.json').write_text(json.dumps(receipt, indent=2) + '\n')
