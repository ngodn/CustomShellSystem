"""Rewrite the stowed-item correction inside a built CSS package, in seconds.

Only the manifest inside the .pak changes, so the IoStore containers stay byte-exact and
this can be re-run as often as it takes to get the lift right. Without --lift it just
prints what a package currently carries.

Python 3.14.
"""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT
from css_package import verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('package', type=Path, help='a built package directory (pak + utoc + ucas)')
parser.add_argument('--lift', type=float, help='cm along the recorded push direction; scales the existing offset')
parser.add_argument('--max-push', type=float, help='cm the live correction may add on top')
args = parser.parse_args()

stem = next(args.package.glob('*.pak')).stem
with tempfile.TemporaryDirectory(prefix='css-retune-') as temporary:
    metadata = Path(temporary)
    subprocess.run([str(DEFAULT_REPAK), 'unpack', str(args.package / (stem + '.pak')), '--output', str(metadata)],
                   check=True, stdout=subprocess.DEVNULL)
    entry = next((metadata / PACKAGE_ROOT).glob('*/manifest.json'))
    manifest = json.loads(entry.read_text())
    variants = manifest['catalog']['outfits'][0]['variants']

    seen = {}
    for variant in variants:
        for socket, offset in (variant.get('attachments') or {}).items():
            length = sum(v * v for v in offset['location']) ** 0.5
            seen.setdefault(round(length, 3), []).append(variant['id'])
            if args.lift is not None and length > 1e-6:
                offset['location'] = [round(v * args.lift / length, 4) for v in offset['location']]
            if args.max_push is not None and 'collision' in offset:
                offset['collision']['max_push'] = args.max_push
    print(manifest['version'], 'lifts before:', {k: len(v) for k, v in sorted(seen.items())})
    if args.lift is None and args.max_push is None:
        raise SystemExit(0)

    entry.write_text(json.dumps(manifest, indent=2) + '\n')
    target = args.package / (stem + '.pak')
    backup = target.with_suffix('.pak.previous')
    shutil.copy2(target, backup)
    subprocess.run([str(DEFAULT_REPAK), 'pack', str(metadata), str(target), '--version', 'V8B'], check=True)

after = verify(args.package)
lifts = {round(sum(v * v for v in o['location']) ** 0.5, 3)
         for x in after['catalog']['outfits'][0]['variants'] for o in (x.get('attachments') or {}).values()}
print('lifts after:', sorted(lifts))
print('verified', after['version'])
