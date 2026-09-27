"""Stage and round-trip the private F13 fitting assets. Python 3.14.

This does not install a mod, create CSS metadata, or replace release archives.
"""
import hashlib
import argparse
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import Converter, DEFAULT_REPAK, asset_info
from convert_beaute import DEFAULT_GAME


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify-existing', action='store_true')
    args = parser.parse_args()
    work = ROOT / 'work/eve26'
    output = work / 'p13pack2'
    if args.verify_existing:
        verify(output, work / 'p13cook/CSSAuthoring/Content/CSS/EveTest')
        return
    output.mkdir(exist_ok=False)
    stage = output / 'stage/MortalShell2/Content/CSS/EveTest'
    stage.mkdir(parents=True)
    source = work / 'p13cook/CSSAuthoring/Content/CSS/EveTest'
    assets = ('SK_PFit13', 'PA_PTailRear')
    for name in assets:
        for suffix in ('.uasset', '.uexp'):
            shutil.copy2(source / (name + suffix), stage / (name + suffix))
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    container = output / 'CSS_EveFit13_P.utoc'
    converter.run(converter.retoc, 'to-zen', output / 'stage', container,
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', container)
    converter.base_containers(DEFAULT_GAME, output / 'containers')
    installed = DEFAULT_GAME / 'Content/Paks/~mods/CSS_EveStellarBlade_eins0fx_P'
    dependencies = {}
    for suffix in ('.utoc', '.ucas'):
        dependency = installed / ('CSS_EveStellarBlade_eins0fx_P' + suffix)
        dependencies[dependency.name] = sha(dependency)
        (output / 'containers' / dependency.name).symlink_to(dependency)
    (output / 'dependencies.json').write_text(json.dumps(dependencies, indent=2) + '\n')
    for suffix in ('.utoc', '.ucas'):
        (output / 'containers' / container.with_suffix(suffix).name).symlink_to(container.with_suffix(suffix))
    converter.run(converter.retoc, 'to-legacy', output / 'containers', output / 'readback',
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/EveTest/')
    verify(output, source)


def verify(output, source):
    container = output / 'CSS_EveFit13_P.utoc'
    dependencies = json.loads((output / 'dependencies.json').read_text())
    for name, expected in dependencies.items():
        assert sha(output / 'containers' / name) == expected, 'Installed dependency changed'
    rows = []
    for name in ('SK_PFit13', 'PA_PTailRear'):
        readback = output / 'readback/MortalShell2/Content/CSS/EveTest' / name
        cooked = source / name
        before = asset_info(cooked.with_suffix('.uasset').read_bytes())
        after = asset_info(readback.with_suffix('.uasset').read_bytes())
        # retoc reconstructs this editor flag from object flags, rather than storing it in Zen.
        flags = []
        assert before['package'] == after['package']
        assert len(before['exports']) == len(after['exports'])
        for original, decoded in zip(before['exports'], after['exports'], strict=True):
            flags.append(dict(name=original['name'], before=original['asset'], after=decoded['asset']))
            assert {k:v for k,v in original.items() if k != 'asset'} == {k:v for k,v in decoded.items() if k != 'asset'}, f'{name}: export identity changed'
        assert sha(cooked.with_suffix('.uexp')) == sha(readback.with_suffix('.uexp')), f'{name}: payload changed'
        rows.append(dict(asset=name, exports=after, reconstructed_asset_flags=flags,
                         payload_sha256=sha(readback.with_suffix('.uexp'))))
    (output / 'verification.json').write_text(json.dumps(dict(
        installed_eve_dependencies=dependencies,
        assets=rows, containers={p.name: sha(p) for p in (container, container.with_suffix('.ucas'))},
        scope='IoStore integrity, decoded header exports and byte-identical cooked payload round-trip. CSS metadata, dependencies and game validation pending.'
    ), indent=2) + '\n')


if __name__ == '__main__':
    main()
