"""Round-trip a native animation study with game containers only, without installing."""
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
    parser.add_argument('work', type=Path)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    assert work.resolve().is_relative_to((ROOT / 'work/cine-animation-port').resolve())
    receipt = json.loads((work / 'stage-result.json').read_text())
    assert receipt['passed']
    for step in ('stage', 'cook'):
        outcome = json.loads((work / f'{step}-exit.json').read_text())
        assert outcome['exit_code'] == 0 and outcome['placeholder_unchanged']
    assert all(sha(Path(p)) == value for p, value in receipt['protected'].items())
    content = ROOT.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content'
    assert sha(content / (receipt['package'].removeprefix('/Game/') + '.uasset')) == receipt['candidate_sha256']
    assert sha(work / 'game-reference/SKEL_Human_Skeleton.uasset') == receipt['native_reference_sha256']
    packages = (work / 'cook.txt').read_text().splitlines()
    assert packages == [receipt['package'], '/Game/CSS/Locomotion/Studies/L5X/ACS_Motion']
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
    trio = output / 'CSS_NativeHumanStudy_P'
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', stage, trio / 'CSS_NativeHumanStudy_P.utoc',
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / 'CSS_NativeHumanStudy_P.utoc')
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
    report = dict(passed=True, packages=packages, skeleton_supplied_by_game=receipt['skeleton'],
                  payloads=payloads, files={p.name: sha(p) for p in trio.iterdir()},
                  outfit_containers_mounted=False, installed=False,
                  scope='Container and import round-trip against the game only. Independent motion decode and live playback pending.')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    (work / 'catalog.json').write_text(json.dumps(dict(animation_library=dict(entries=[dict(
        id='css.study.native.blackhole', assets=[dict(asset=receipt['package'] + '.AN_BlackHole')])])), indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
