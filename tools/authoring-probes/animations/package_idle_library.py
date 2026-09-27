"""Add cooked idle sequences to the accepted Eve package. Python 3.14.

Creates a review candidate only. Never installs or replaces release archives.
"""
import copy
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import Converter, DEFAULT_REPAK, asset_info, digest
from css_package import verify
from convert_beaute import DEFAULT_GAME


def main():
    work = ROOT / 'work/eve-idle1'
    baseline = ROOT / 'work/eve26/six-v120'
    stem = 'CSS_EveStellarBlade_eins0fx_P'
    source = baseline / stem
    original = verify(source)
    entries = json.loads((work / 'e5/export-result.json').read_text())['entries']
    assert [e['id'] for e in entries] == [f'eve_idle_{n}' for n in range(700, 714)]
    output = work / 'pack1'
    output.mkdir(exist_ok=False)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    inputs = output / 'inputs'
    converter.base_containers(DEFAULT_GAME, inputs)
    baseline_hashes = {p.name: digest(p) for p in source.iterdir() if p.is_file()}
    for suffix in ('.utoc', '.ucas'):
        (inputs / (stem + suffix)).symlink_to(source / (stem + suffix))
    legacy = output / 'legacy'
    converter.run(converter.retoc, 'to-legacy', inputs, legacy,
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/')
    old_paths = {str(p.relative_to(legacy)) for p in legacy.rglob('*.uasset')}
    expected = json.loads((ROOT / 'work/eve26/six-assets1/verification.json').read_text())['assets']
    assert old_paths == set(expected), 'Baseline asset coverage differs'
    old_hashes = {str(p.relative_to(legacy)): digest(p) for p in legacy.rglob('*') if p.is_file()}
    new_paths = set()
    for entry in entries:
        relative = entry['clip'].split('.')[0].removeprefix('/Game/')
        path = 'MortalShell2/Content/' + relative + '.uasset'
        assert path not in old_paths
        new_paths.add(path)
        for suffix in ('.uasset', '.uexp'):
            cooked = work / 'cook1/CSSAuthoring/Content' / (relative + suffix)
            destination = legacy / 'MortalShell2/Content' / (relative + suffix)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(cooked, destination)
    trio = output / stem
    trio.mkdir()
    target = trio / (stem + '.utoc')
    converter.run(converter.retoc, 'to-zen', legacy, target, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', target)
    checks = output / 'checks'
    converter.base_containers(DEFAULT_GAME, checks)
    for suffix in ('.utoc', '.ucas'):
        (checks / (stem + suffix)).symlink_to(trio / (stem + suffix))
    decoded = output / 'readback'
    converter.run(converter.retoc, 'to-legacy', checks, decoded,
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/')
    assert {str(p.relative_to(decoded)) for p in decoded.rglob('*.uasset')} == old_paths | new_paths
    for path in sorted(old_paths | new_paths):
        before, after = legacy / path, decoded / path
        a, b = asset_info(before.read_bytes()), asset_info(after.read_bytes())
        if path in old_paths:
            assert a == b, path
        else:
            assert a['package'] == b['package'], path
            assert len(a['exports']) == len(b['exports']), path
            for x, y in zip(a['exports'], b['exports'], strict=True):
                assert {k:v for k,v in x.items() if k != 'asset'} == {k:v for k,v in y.items() if k != 'asset'}, path
        for suffix in ('.uexp', '.ubulk', '.uptnl'):
            a, b = before.with_suffix(suffix), after.with_suffix(suffix)
            assert a.exists() == b.exists(), str(a)
            assert not a.exists() or digest(a) == digest(b), str(a)
    assert all(digest(legacy / p) == h for p, h in old_hashes.items())
    manifest = copy.deepcopy(original)
    for variant in manifest['catalog']['outfits'][0]['variants']:
        assert len(variant['animations']['idle']) == 1
        assert variant['animations']['idle'][0]['id'] == 'eve'
        variant['animations']['idle'].extend(copy.deepcopy(entries))
    restored = copy.deepcopy(manifest)
    for variant in restored['catalog']['outfits'][0]['variants']:
        variant['animations']['idle'] = variant['animations']['idle'][:1]
    assert restored == original
    metadata = output / 'metadata'
    shutil.copytree(baseline / 'metadata', metadata)
    for suffix in ('.utoc', '.ucas'):
        p = trio / (stem + suffix)
        manifest['containers'][suffix] = dict(file=p.name, bytes=p.stat().st_size, sha256=digest(p))
    manifest_path = next(metadata.rglob('manifest.json'))
    manifest_path.write_text(json.dumps(manifest, separators=(',', ':')) + '\n')
    converter.run(DEFAULT_REPAK, 'pack', metadata, trio / (stem + '.pak'), '--version', 'V8B')
    assert verify(trio) == manifest
    assert all(digest(source / p) == h for p, h in baseline_hashes.items())
    (output / 'verification.json').write_text(json.dumps(dict(
        baseline=baseline_hashes, preserved_assets=sorted(old_paths), added_assets=sorted(new_paths),
        files={p.name: digest(p) for p in trio.iterdir()},
        scope='Review candidate, not a release. Cooked payload equality and metadata verified; gameplay pending.'
    ), indent=2) + '\n')
    print(trio, flush=True)


if __name__ == '__main__':
    main()
