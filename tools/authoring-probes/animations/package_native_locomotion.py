"""Package the four native locomotion choices for independent game-only readback."""
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
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('blends', type=Path)
    parser.add_argument('readback', type=Path)
    parser.add_argument('cook', type=Path)
    args = parser.parse_args()
    blends, readback, work = [p.resolve(strict=True) for p in (args.blends, args.readback, args.cook)]
    for path in (blends, readback, work):
        assert path.resolve().is_relative_to((ROOT / 'work/cine-animation-port').resolve())
        assert json.loads((path / 'exit.json').read_text())['exit_code'] == 0
    creation = json.loads((blends / 'result.json').read_text())
    verified = json.loads((readback / 'result.json').read_text())
    assert creation['passed'] and verified['passed']
    assert len(verified['clips']) == 20 and len(creation['blends']) == 4
    content = ROOT.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
    canonical = content / 'Sparta/Characters/Humans/_Shared/SKEL_Human_Skeleton.uasset'
    reference = ROOT / 'work/cine-animation-port/native-binding-v6/game-reference/SKEL_Human_Skeleton.uasset'
    protected = {str(reference if Path(p) == canonical else Path(p)): checksum
                 for p, checksum in verified['protected'].items()}
    for row in verified['clips']:
        protected[row['raw']] = row['sha256']
    assert all(sha(Path(p)) == checksum for p, checksum in protected.items())
    cook = json.loads((work / 'exit.json').read_text())
    assert cook['placeholder_unchanged'] and sha(canonical) == cook['placeholder_sha256']
    packages = (work / 'cook.txt').read_text().splitlines()
    expected = {r['package'] for r in verified['clips'] + creation['blends']}
    expected.add('/Game/CSS/Locomotion/Studies/L5X/ACS_Motion')
    assert len(packages) == len(set(packages)) == 25 and set(packages) == expected
    output = work / 'pack'
    output.mkdir(exist_ok=False)
    stage = output / 'stage'
    payloads = {}
    for package in packages:
        for extension in ('.uasset', '.uexp'):
            relative = Path(package.removeprefix('/Game/') + extension)
            source = work / 'cooked/CSSAuthoring/Content' / relative
            destination = stage / 'MortalShell2/Content' / relative
            if extension == '.uasset':
                assert asset_info(source.read_bytes())['package'] == package
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.hardlink_to(source)
            payloads[str(destination.relative_to(stage))] = sha(source)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output / 'CSS_NativeLocomotionStudy_P'
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', stage, trio / 'CSS_NativeLocomotionStudy_P.utoc',
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / 'CSS_NativeLocomotionStudy_P.utoc')
    converter.base_containers(GAME, output / 'containers')
    for path in trio.iterdir():
        (output / 'containers' / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', output / 'containers', output / 'readback',
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, checksum in payloads.items():
        path = output / 'readback' / relative
        assert path.is_file()
        if path.suffix == '.uexp':
            assert sha(path) == checksum
        else:
            asset_info(path.read_bytes())
    report = dict(passed=True, packages=packages, payloads=payloads, protected=protected,
                  files={p.name: sha(p) for p in trio.iterdir()},
                  outfit_containers_mounted=False, installed=False,
                  scope='Game-only container round-trip. Full decoded motion and live playback remain pending.')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    entries = [dict(id='css.study.native.' + r['label'].lower(),
                    assets=[dict(asset=r['package'] + '.' + r['package'].rsplit('/', 1)[1])])
               for r in verified['clips']]
    (work / 'catalog.json').write_text(json.dumps(dict(animation_library=dict(entries=entries)), indent=2) + '\n')
    print('Native locomotion round-trip passed:', len(packages), 'packages')


if __name__ == '__main__':
    main()
