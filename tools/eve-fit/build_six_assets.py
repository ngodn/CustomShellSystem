"""Combine audited Eve containers without changing cooked payloads. Python 3.14."""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import Converter, DEFAULT_REPAK, digest, asset_info
from convert_beaute import DEFAULT_GAME


def main():
    work = ROOT / 'work/eve26'
    audit = json.loads((work / 'six-audit.json').read_text())
    output = work / 'six-assets1'
    output.mkdir(exist_ok=False)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    containers = output / 'inputs'
    converter.base_containers(DEFAULT_GAME, containers)
    paths = {}
    for row in audit['sources']:
        source = Path(row['source'])
        for name, expected in row['files'].items():
            if digest(source / name) != expected:
                raise ValueError(f'Audited input changed: {source / name}')
        container = next(source.glob('*.utoc'))
        listing = converter.run(converter.retoc, 'list', container, '--path')
        for line in listing.splitlines():
            match = re.search(r'\sExportBundleData\s+\.\./\.\./\.\./(.+)$', line)
            if not match:
                continue
            path = match[1]
            if not path.startswith('MortalShell2/Content/CSS/'):
                raise ValueError(f'Unexpected asset outside CSS: {path}')
            if path in paths:
                if {paths[path], row['label']} != {'knit', 'alice'} or not path.endswith('/ABP_KnitFeet1.uasset'):
                    raise ValueError(f'Unreviewed duplicate asset: {path}')
                for suffix in ('.uasset', '.uexp'):
                    a = work / 'a3pack/readback' / Path(path).with_suffix(suffix)
                    b = work / 'k4pack/readback' / Path(path).with_suffix(suffix)
                    if digest(a) != digest(b):
                        raise ValueError('Shared footwear graph differs between candidates')
            paths[path] = row['label']
        for suffix in ('.utoc', '.ucas'):
            (containers / container.with_suffix(suffix).name).symlink_to(container.with_suffix(suffix))
    print(f'Checked {len(paths)} unique asset paths', flush=True)
    legacy = output / 'legacy'
    converter.run(converter.retoc, 'to-legacy', containers, legacy,
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/')
    actual = {str(p.relative_to(legacy)) for p in legacy.rglob('*.uasset')}
    if actual != set(paths):
        raise ValueError(f'Extraction coverage mismatch: missing={set(paths)-actual}, extra={actual-set(paths)}')
    stem = 'CSS_EveStellarBlade_eins0fx_P'
    target = output / (stem + '.utoc')
    converter.run(converter.retoc, 'to-zen', legacy, target, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', target)
    checks = output / 'checks'
    converter.base_containers(DEFAULT_GAME, checks)
    for suffix in ('.utoc', '.ucas'):
        (checks / target.with_suffix(suffix).name).symlink_to(target.with_suffix(suffix))
    decoded = output / 'readback'
    converter.run(converter.retoc, 'to-legacy', checks, decoded,
                  '--version', 'UE5_6', '--no-parallel', '--no-shaders', '-f', '/CSS/')
    for path in paths:
        before, after = legacy / path, decoded / path
        if asset_info(before.read_bytes()) != asset_info(after.read_bytes()):
            raise ValueError(f'Decoded asset header changed: {path}')
        for suffix in ('.uexp', '.ubulk', '.uptnl'):
            a, b = before.with_suffix(suffix), after.with_suffix(suffix)
            if a.exists() != b.exists() or (a.exists() and digest(a) != digest(b)):
                raise ValueError(f'Cooked payload changed: {path}, {suffix}')
    (output / 'verification.json').write_text(json.dumps(dict(
        assets=paths, files={p.name:digest(p) for p in (target,target.with_suffix('.ucas'))},
        scope='Combined container integrity, asset coverage and decoded payload equality. Metadata and gameplay pending.'), indent=2)+'\n')
    print('Combined asset round-trip passed', flush=True)


if __name__ == '__main__':
    main()
