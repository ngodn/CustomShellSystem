"""Relocate cooked Texture2D headers and verify every mip from both containers."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info
from verify_outfit_material_trial import normalize


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('instances', 'dotnet', 'mappings', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    source, output = args.instances.resolve(strict=True), args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / 'work/three-outfit-rig-physics'):
        raise ValueError('Use a new private migration directory')
    proof = read(source / 'verification.json')
    if not proof['passed']:
        raise ValueError('Source extraction and material preservation must pass first')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Pinned source changed: ' + str(path))
        protected[str(path)] = digest

    protect(source / 'verification.json')
    protect(source / 'request.json')
    protect(args.mappings)
    for row in proof['source_containers'].values():
        protect(Path(row['path']), row['sha256'])
    mapping = read(source / 'request.json')['mapping']
    textures = {row['package'] for row in proof['other_assets'] if row['classes'] == ['Texture2D']}
    if not textures or not textures <= mapping.keys():
        raise ValueError('Invalid texture inventory')
    headers = {}
    for path in (source / 'original').rglob('*.uasset'):
        package = asset_info(path.read_bytes())['package']
        if package in textures:
            if package in headers:
                raise ValueError('Duplicate source package')
            headers[package] = path
    if set(headers) != textures:
        raise ValueError('Incomplete texture extraction')
    output.mkdir(parents=True)
    stage = output / 'stage'
    jobs = []
    for package, header in sorted(headers.items()):
        for suffix in ('.uasset', '.uexp', '.ubulk'):
            file = header.with_suffix(suffix)
            if suffix != '.ubulk' or file.exists():
                protect(file)
        target = stage / 'MortalShell2/Content' / (mapping[package].removeprefix('/Game/') + '.uasset')
        jobs.append(dict(input=str(header), output=str(target)))
    request = output / 'request.json'
    request.write_text(json.dumps(dict(mapping=mapping, jobs=jobs, textures=True,
                                      report=str(output / 'relocation.json')), indent=2) + '\n')
    writer = ROOT / 'build/retoc-css-target/release/examples/rewrite_material'
    reader = ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'
    audit = ROOT / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll'
    for path in (writer, reader, audit, ROOT / 'tools/asset-paths/rewrite_material.rs',
                 ROOT / 'tools/AssetReadback/Program.cs', Path(__file__)):
        protect(path)

    def run(name, command):
        (output / (name + '-command.json')).write_text(json.dumps([str(a) for a in command], indent=2) + '\n')
        with (output / (name + '.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        (output / (name + '-exit.json')).write_text(json.dumps(dict(returncode=result.returncode)) + '\n')
        result.check_returncode()

    run('relocation', [writer, request])
    relocation = read(output / 'relocation.json')
    if not relocation['passed'] or len(relocation['textures']) != len(jobs):
        raise ValueError('Incomplete texture relocation')
    for job in jobs:
        for suffix in ('.uexp', '.ubulk'):
            original, candidate = Path(job['input']).with_suffix(suffix), Path(job['output']).with_suffix(suffix)
            if original.exists() != candidate.exists() or original.exists() and sha(original) != sha(candidate):
                raise ValueError('Texture payload changed: ' + str(original))
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    toc = output / 'CSS_TextureNamespaceTrial_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = output / 'containers'
    converter.base_containers(GAME, mount)
    for suffix in ('.pak', '.utoc', '.ucas'):
        file = toc.with_suffix(suffix)
        (mount / file.name).symlink_to(file)
    original_packages, packages = output / 'original-packages.txt', output / 'packages.txt'
    original_packages.write_text('\n'.join(sorted(textures)) + '\n')
    packages.write_text('\n'.join(sorted(mapping[p] for p in textures)) + '\n')
    references = output / 'references.txt'
    references.write_text('\n'.join(mapping[p] + '.' + mapping[p].rsplit('/', 1)[1] for p in sorted(textures)) + '\n')
    run('original-readback', [args.dotnet, reader, source / 'containers', args.mappings,
                              original_packages, output / 'original-decoded.json', '--texture-digests'])
    run('candidate-readback', [args.dotnet, reader, mount, args.mappings,
                               packages, output / 'decoded.json', '--texture-digests'])
    original, candidate = read(output / 'original-decoded.json'), read(output / 'decoded.json')
    normalized = normalize(candidate, {v: k for k, v in mapping.items()})
    differences = [p for p in sorted(textures) if normalized.get(p) != original.get(p)]
    passed = not differences and set(original) == textures and set(normalized) == textures
    mips = sum(len(exports[0]['OfflineTextureDigest']['mips']) for exports in original.values())
    (output / 'comparison.json').write_text(json.dumps(dict(passed=passed, packages=len(textures),
                                                           mip_count=mips, differences=differences), indent=2) + '\n')
    if not passed:
        raise ValueError('Decoded texture metadata or mip payload changed')
    run('imports', [args.dotnet, audit, mount, args.mappings, packages, references, output / 'import-check.json'])
    if not read(output / 'import-check.json')['passed']:
        raise ValueError('Candidate import check failed')
    for path, digest in protected.items():
        if sha(Path(path)) != digest:
            raise ValueError('Source changed during migration: ' + path)
    (output / 'verification.json').write_text(json.dumps(dict(passed=True, textures=len(textures),
        mips=mips, source_files=protected, installed=False,
        payloads={str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()},
        files={toc.with_suffix(s).name: sha(toc.with_suffix(s)) for s in ('.pak', '.utoc', '.ucas')},
        scope='Reversible headers, unchanged export/bulk bytes, identical independently read mip payloads '
              'and texture metadata from original and candidate IoStore containers, and resolved imports. '
              'No rendering or gameplay acceptance.'), indent=2) + '\n')
    print(f'Preserved {len(textures)} textures and {mips} mip payloads')


if __name__ == '__main__':
    main()
