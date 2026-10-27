"""Pack the cooked CINE materials into an IoStore trio. Python 3.14, standard library only.

Usage: pack.py COOK_DIRECTORY NEW_OUTPUT_DIRECTORY [--retoc PATH] [--name CSSX_CINE_eins0fx_P]

Takes only /Game/CSSX/CINE from the cook (the cook also writes engine content, which stays
behind), checks the asset set is exactly what author_materials.py creates, converts with retoc
and verifies the container. Install is separate (install.py) and never automatic.
"""
import argparse
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
EXPECTED = {'MPC_Cine', 'Materials/M_Cine_Wire', 'Materials/M_Cine_Clay', 'Materials/M_Cine_XRay',
            'Materials/M_Cine_Solid', 'Materials/PP_Cine_Buffer_Post', 'Materials/PP_Cine_Buffer_Pre',
            'Core/Audio/SM_CINE_Render', 'Core/DT_CINE_Core'}

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('cook', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--retoc', type=Path, default=REPO / 'build/retoc-css-target/release/retoc')
parser.add_argument('--name', default='CSSX_CINE_eins0fx_P')
args = parser.parse_args()
if not re.fullmatch(r'CSSX_[A-Za-z0-9_]+_P', args.name):
    parser.error('Container name must look like CSSX_<Name>_<Author>_P')
retoc = args.retoc.resolve(strict=True)
source = (args.cook / 'project/Saved/Cooked/Windows/CineAssets/Content/CSSX/CINE').resolve(strict=True)
output = args.output.resolve()
if output.exists():
    parser.error('Output directory exists; use a new one')
files = [p for p in source.rglob('*') if p.is_file()]
if any(p.suffix not in ('.uasset', '.uexp', '.ubulk') for p in files):
    raise SystemExit(f'Unexpected cooked files: {[p.name for p in files if p.suffix not in (".uasset", ".uexp", ".ubulk")]}')
packages = {str(p.relative_to(source).with_suffix('')) for p in files if p.suffix == '.uasset'}
if packages != EXPECTED:
    raise SystemExit(f'Cooked packages differ: missing {sorted(EXPECTED - packages)}, extra {sorted(packages - EXPECTED)}')
if any(not (source / (name + '.uexp')).is_file() for name in packages):
    raise SystemExit('A cooked package has no export payload')
legacy = output / 'legacy'
shutil.copytree(source, legacy / 'MortalShell2/Content/CSSX/CINE')
utoc = output / (args.name + '.utoc')


def run(*command):
    result = subprocess.run([str(c) for c in command], capture_output=True, text=True)
    if result.returncode:
        raise SystemExit(f'{command[1]} failed: {result.stderr.strip() or result.stdout.strip()}')
    return result.stdout


run(retoc, 'to-zen', '--version', 'UE5_6', legacy, utoc)
run(retoc, 'verify', utoc)
trio = sorted(output.glob(args.name + '.*'))
if {p.suffix for p in trio} != {'.pak', '.utoc', '.ucas'}:
    raise SystemExit(f'Expected a pak/utoc/ucas trio, got {[p.name for p in trio]}')
manifest = {'container': args.name, 'packages': sorted('/Game/CSSX/CINE/' + name for name in packages),
            'files': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in trio}, 'installed': False}
(output / 'pack.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2))
