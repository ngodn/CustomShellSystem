"""Reproduce Lane E's offline checks; never author, cook, install or launch the game."""
from pathlib import Path
import ast
import hashlib
import json
import re
import subprocess
import time

HERE = Path(__file__).resolve().parent
CINE = HERE.parents[2]
REPO = CINE.parents[1]
def inputs():
    paths = [CINE / 'menu.json', CINE / 'assets/author_materials.py', CINE / 'assets/pack.py', REPO / 'extensions/core/CMakeLists.txt']
    for folder in ('src', 'tests', 'assets/core'):
        paths.extend(p for p in (CINE / folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    return {str(p.relative_to(REPO)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}

before = inputs()
results = []
checks = [
    ('configure.log', ['cmake', '-S', 'extensions/core', '-B', 'build/astra-e-host', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release']),
    ('build-final.log', ['cmake', '--build', 'build/astra-e-host', '-j', '6']),
    ('tests.log', ['ctest', '--test-dir', 'build/astra-e-host', '--output-on-failure']),
    ('content-checks.log', ['build/astra-e-host/cssx_cine_content_tests']),
    ('windows-build.log', ['python3', 'extensions/core/tools/cssx.py', 'build', '--dev']),
    ('generated-check.log', ['python3', 'extensions/cine/assets/core/generate.py', '--check']),
    ('source-check.log', ['python3', 'extensions/cine/assets/core/validate.py']),
    ('scope-check.log', ['python3', str(HERE / 'verify_scope.py')]),
]
for filename, command in checks:
    started = time.monotonic()
    with (HERE / filename).open('w') as output:
        result = subprocess.run(command, cwd=REPO, stdout=output, stderr=subprocess.STDOUT)
    warnings = re.findall(r'(?im)^.*\bwarning\b.*$', (HERE / filename).read_text()) if 'build' in filename else []
    results.append({'command': command, 'exit': result.returncode, 'seconds': round(time.monotonic() - started, 3), 'log': filename, 'warnings': warnings})
    (HERE / 'verification.json').write_text(json.dumps(results, indent=2) + '\n')
    print(filename, result.returncode, flush=True)
    if result.returncode or warnings:
        raise SystemExit('Verification failed: ' + filename)
for path in [CINE / 'assets/author_materials.py', CINE / 'assets/pack.py', *(CINE / 'assets/core').glob('*.py')]:
    ast.parse(path.read_text(), filename=str(path))
command = ['rg', '-n', '-F', '-f', str(HERE / 'moved-literals.txt'), 'src']
scan = subprocess.run(command, cwd=CINE, capture_output=True, text=True)
(HERE / 'literal-scan.txt').write_text('Command: ' + ' '.join(command) + '\nExit code: ' + str(scan.returncode) + ' (1 means no matches)\n' + scan.stdout + scan.stderr)
if scan.returncode != 1:
    raise SystemExit('Moved literals remain, or the scan failed')
if inputs() != before:
    raise SystemExit('Sources changed during validation; rerun on the finished tree')
(HERE / 'source-hashes.json').write_text(json.dumps(before, indent=2) + '\n')
artifacts = {}
for name in ('build/astra-e-host/cine.so', 'extensions/core/build/windows-dev/cine.dll'):
    path = REPO / name
    artifacts[name] = {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
(HERE / 'artifacts.json').write_text(json.dumps(artifacts, indent=2) + '\n')
print('Python syntax, moved-literal scan and unchanged source hashes passed', flush=True)
