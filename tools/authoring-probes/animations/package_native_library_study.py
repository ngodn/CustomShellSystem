"""Round-trip the new native body candidates without installing a trial."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cook', type=Path)
    parser.add_argument('authoring', type=Path)
    args = parser.parse_args()
    work, authoring = args.cook.resolve(strict=True), args.authoring.resolve(strict=True)
    for path in (work, authoring):
        if not path.is_relative_to(ROOT/'work/three-outfit-rig-physics'):
            raise ValueError('Expected a private rig workspace')
    receipt = json.loads((work/'exit.json').read_text())
    if receipt['returncode'] != 0 or not receipt['protected_unchanged']:
        raise ValueError('Cook failed')
    protected = json.loads((work/'protected.json').read_text())
    if any(sha(Path(p)) != digest for p, digest in protected.items()):
        raise ValueError('Cook source changed')
    packages = (work/'packages.txt').read_text().splitlines()
    if len(packages) != len(set(packages)) or any(not p.startswith('/Game/CSS/eins0fx/Shared/Locomotion/') for p in packages):
        raise ValueError('Unexpected cook inventory')
    output = work/'pack'
    output.mkdir()
    stage = output/'stage'
    payloads = {}
    for package in packages:
        for suffix in ('.uasset', '.uexp', '.ubulk'):
            relative = Path(package.removeprefix('/Game/')+suffix)
            source = work/'cooked/CSSAuthoring/Content'/relative
            if suffix == '.ubulk' and not source.exists():
                continue
            if suffix == '.uasset' and asset_info(source.read_bytes())['package'] != package:
                raise ValueError('Cooked identity changed')
            target = stage/'MortalShell2/Content'/relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.hardlink_to(source)
            payloads[str(target.relative_to(stage))] = sha(source)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output/'CSS_NativeLibraryStudy_P'
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', stage, trio/'CSS_NativeLibraryStudy_P.utoc', '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio/'CSS_NativeLibraryStudy_P.utoc')
    converter.base_containers(GAME, output/'containers')
    for file in trio.iterdir():
        (output/'containers'/file.name).symlink_to(file)
    converter.run(converter.retoc, 'to-legacy', output/'containers', output/'readback',
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/Shared/Locomotion/')
    for relative, expected in payloads.items():
        path = output/'readback'/relative
        if path.suffix == '.uasset':
            if asset_info(path.read_bytes())['package'] not in packages:
                raise ValueError('Readback package identity changed')
        elif sha(path) != expected:
            raise ValueError('Cooked payload changed during round trip')
    clips = [row for run in ('pilot3', 'batch1')
             for row in json.loads((authoring/run/'result.json').read_text())['clips']]
    if len(clips) != 59 or any(row['package'] not in packages for row in clips):
        raise ValueError('Native sequence inventory differs')
    entries = [dict(id=row['id'], assets=[dict(asset=row['package']+'.'+row['package'].rsplit('/', 1)[1])]) for row in clips]
    (work/'catalog.json').write_text(json.dumps(dict(animation_library=dict(entries=entries)), indent=2)+'\n')
    (output/'verification.json').write_text(json.dumps(dict(passed=True, packages=packages,
        payloads=payloads, files={p.name:sha(p) for p in trio.iterdir()},
        installed=False, scope='Game-only container round trip. Does not accept the outstanding Cinderella evaluation difference or gameplay.'), indent=2)+'\n')
    print('Native study round trip:', len(packages), 'packages')


if __name__ == '__main__':
    main()
