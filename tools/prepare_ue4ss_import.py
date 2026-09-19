"""Create the pinned UE4SS import library without replacing the retained headers."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--dll', type=Path, required=True)
parser.add_argument('--consumer', type=Path, action='append', required=True)
args = parser.parse_args()
pin = json.loads((ROOT / 'native/ue4ss-runtime.json').read_text())
source = args.dll.resolve()
if not all(path.resolve().is_relative_to(ROOT.parent) for path in [source, *args.consumer]):
    parser.error('Runtime inputs must remain inside the workspace')
if hashlib.sha256(source.read_bytes()).hexdigest() != pin['dll_sha256']:
    parser.error('DLL does not match the explicit runtime pin')
description = subprocess.check_output(['llvm-readobj', '--coff-exports', str(source)], text=True)
if 'Format: COFF-x86-64' not in description:
    parser.error('Expected an x64 PE runtime')
names = re.findall(r'^  Name: (.+)$', description, re.MULTILINE)
if not names or len(names) != len(set(names)) or any(any(c.isspace() for c in name) for name in names):
    parser.error('Unexpected export name format')
exports = set(names)
consumers = []
for path in args.consumer:
    imported = subprocess.check_output(['llvm-readobj', '--coff-imports', str(path)], text=True)
    blocks = [block for block in re.findall(r'Import \{\n(.*?)\n\}', imported, re.DOTALL)
              if re.search(r'^  Name: UE4SS\.dll$', block, re.MULTILINE | re.IGNORECASE)]
    if len(blocks) != 1:
        raise ValueError('Expected one UE4SS import descriptor: ' + str(path))
    symbols = re.findall(r'^  Symbol: (.+) \(\d+\)$', blocks[0], re.MULTILINE)
    missing = sorted(set(symbols) - exports)
    if not symbols or missing:
        raise ValueError('Missing exact imported names: ' + repr(missing))
    consumers.append({'file': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                      'required_exports': len(symbols), 'missing_exports': []})
library = ROOT / pin['import_library']
if library.parent.exists():
    raise FileExistsError('Preserve the previous import-library directory before regenerating')
library.parent.mkdir(parents=True)
definition = library.with_suffix('.def')
definition.write_text('LIBRARY UE4SS.dll\nEXPORTS\n' + '\n'.join(sorted(names)) + '\n')
shutil.copy2(source, library.with_suffix('.dll'))
subprocess.run(['llvm-dlltool', '-m', 'i386:x86-64', '-d', str(definition), '-l', str(library)], check=True)
report = {'runtime': pin, 'exports': len(names), 'consumers': consumers,
          'library_sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
          'definition_sha256': hashlib.sha256(definition.read_bytes()).hexdigest(),
          'scope': 'Exact imported symbol availability only; header layouts and fresh live behavior require separate evidence'}
(library.parent / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
