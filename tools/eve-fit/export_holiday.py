"""Export the fitted Holiday body and garments to a private cloth-development package."""
import hashlib
import argparse
import json
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'
WORK = ROOT / 'CustomShellSystem/work/eve26'
sys.path.insert(0, str(MOD / 'tools'))
sys.path.insert(0, str(MOD / 'gemini-work'))
import export_variant_clean as exporter
sys.path.insert(0, str(Path(__file__).resolve().parent))
from clean_weights import clean_weights

# Keep Gemini's reference exporter intact; use the audited cleanup for Eve exports.
exporter.clean_weights = clean_weights

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--blend', type=Path, default=WORK/'holiday-f10.blend')
parser.add_argument('--output', type=Path, default=WORK/'holiday.mesh.json')
parser.add_argument('--mesh-package', default='/Game/CSS/EveTest/SK_Holiday')
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
blend = args.blend.resolve()
output = args.output.resolve()
assert args.mesh_package.startswith('/Game/CSS/EveTest/')
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

exporter.export_variant(
    blend_path=blend,
    refskel_path=MOD / 'authoring/reference/SKEL_CSS_Base.refskel.json',
    output_path=output,
    target_objects=['Eve Body'] + [
        'Eve Christmas - ' + part for part in ('Dress', 'Arms', 'Legs', 'Panties')
    ],
    mesh_package=args.mesh_package,
    skeleton_package='/Game/CSS/EveTest/SKEL_Holiday_Import',
    bind_pose_path=MOD / 'work/CSS_SeduXtress_ArmRestV44B2.bindpose.json',
    left_hand_correctives=True,
)
after = hashlib.sha256(blend.read_bytes()).hexdigest()
assert before == after, 'Export changed the saved candidate'
receipt = {
    'candidate_sha256': before,
    'source_unchanged': True,
    'weight_cleanup_sha256': hashlib.sha256(
        Path(__file__).with_name('clean_weights.py').read_bytes()
    ).hexdigest(),
    'stage': 'Private cloth-development export, not a complete outfit or release',
    'shared_skeleton': '/Game/CSS/Shared/SKEL_Base',
    'shared_skeleton_modified': False,
    'omitted': ['hair', 'hat', 'earrings', 'footwear'],
    'exporter_sha256': hashlib.sha256(
        (MOD / 'gemini-work/export_variant_clean.py').read_bytes()
    ).hexdigest(),
}
receipt_path = WORK/'holiday.export.json' if output == WORK/'holiday.mesh.json' else output.with_name(output.stem+'.receipt.json')
assert not receipt_path.exists(), receipt_path
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
