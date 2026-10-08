"""Package migrated material dependencies and compare them with cooked originals."""
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


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text())


def normalize(value, mapping):
    if isinstance(value, dict):
        return {normalize(k, mapping): normalize(v, mapping) for k, v in value.items()}
    if isinstance(value, list):
        return [normalize(v, mapping) for v in value]
    if isinstance(value, str):
        for new, old in sorted(mapping.items(), key=lambda item: len(item[0]), reverse=True):
            value = value.replace(new, old)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--instances', type=Path, required=True)
    parents = parser.add_mutually_exclusive_group(required=True)
    parents.add_argument('--cook', type=Path)
    parents.add_argument('--parents', type=Path, help='Verified header relocation with preserved compiled material payloads')
    parser.add_argument('--textures', type=Path, help='Verified cooked texture relocation to preserve instead of recooking')
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--mappings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--work-root', type=Path, default=ROOT / 'work/three-outfit-rig-physics')
    args = parser.parse_args()
    output = args.output.resolve()
    work_root = args.work_root.resolve()
    if output.exists() or output == work_root or not output.is_relative_to(work_root):
        raise ValueError('Use a new private migration directory')
    instances = args.instances.resolve(strict=True)
    cook = args.cook.resolve(strict=True) if args.cook else None
    instance_check = read(instances / 'verification.json')
    parent_evidence = []
    if not instance_check['passed']:
        raise ValueError('Instance migration must pass first')
    if cook:
        if read(cook / 'exit.json') != dict(returncode=0, sources_unchanged=True):
            raise ValueError('Parent cook must pass first')
        parent_evidence = [cook / 'exit.json', cook / 'sources.json']
    for row in instance_check['source_containers'].values():
        if sha(Path(row['path'])) != row['sha256']:
            raise ValueError('Pinned original container changed')
    if read(instances / 'original-readback-exit.json')['returncode'] != 0:
        raise ValueError('Original independent readback failed')
    mapping = read(instances / 'request.json')['mapping']
    originals = read(instances / 'original-decoded.json')
    expected = {mapping[p] for p in originals}
    if len(expected) != len(originals):
        raise ValueError('Material destinations overlap')
    output.mkdir(parents=True)
    stage = output / 'stage'
    for relative, digest in instance_check['payloads'].items():
        source = instances / 'stage' / relative
        if sha(source) != digest:
            raise ValueError('Migrated instance changed: ' + relative)
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
    preserved_textures = set()
    texture_evidence = []
    if args.textures:
        textures = args.textures.resolve(strict=True)
        texture_check = read(textures / 'verification.json')
        if not texture_check['passed'] or texture_check['textures'] < 1 or texture_check['mips'] < 1:
            raise ValueError('Texture preservation must pass first')
        texture_evidence.append(textures / 'verification.json')
        for relative, digest in texture_check['payloads'].items():
            source, target = textures / 'stage' / relative, stage / relative
            if sha(source) != digest or target.exists():
                raise ValueError('Changed or overlapping preserved texture: ' + relative)
            if source.suffix == '.uasset':
                info = asset_info(source.read_bytes())
                if info['package'] not in expected or [e['class'] for e in info['exports']] != ['Texture2D']:
                    raise ValueError('Unexpected preserved texture package')
                preserved_textures.add(info['package'])
            target.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
        if len(preserved_textures) != texture_check['textures']:
            raise ValueError('Incomplete preserved texture inventory')
    if args.parents:
        parent_stage = args.parents.resolve(strict=True)
        receipt = parent_stage / 'verification.json'
        proof = read(receipt)
        if not proof['passed']:
            raise ValueError('Parent preservation failed')
        parent_evidence = [receipt]
        for source, digest in proof['source_files'].items():
            if sha(Path(source)) != digest:
                raise ValueError('Parent source changed: ' + source)
        for relative, digest in proof['payloads'].items():
            if Path(relative).is_absolute() or '..' in Path(relative).parts:
                raise ValueError('Invalid preserved parent payload path')
            source, target = parent_stage / 'stage' / relative, stage / relative
            if sha(source) != digest or target.exists():
                raise ValueError('Changed or overlapping preserved parent')
            if source.suffix == '.uasset':
                info = asset_info(source.read_bytes())
                if info['package'] not in expected or [e['class'] for e in info['exports']] != ['Material']:
                    raise ValueError('Unexpected preserved parent class or package')
            target.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
    cooked = cook / 'project/Saved/Cooked/Windows/CSSShared/Content' if cook else None
    for header in cooked.rglob('*.uasset') if cooked else ():
        package = '/Game/' + str(header.relative_to(cooked).with_suffix(''))
        if package not in expected:
            continue
        if package in preserved_textures:
            # A parent cook may emit its texture dependencies. Keep the already
            # verified original mip payloads as the sole provider for those paths.
            if [e['class'] for e in asset_info(header.read_bytes())['exports']] != ['Texture2D']:
                raise ValueError('Cook conflicts with a preserved texture')
            continue
        for source in header.parent.glob(header.stem + '.*'):
            if source.suffix not in {'.uasset', '.uexp', '.ubulk'}:
                raise ValueError('Unsupported cooked payload: ' + str(source))
            target = stage / 'MortalShell2/Content' / source.relative_to(cooked)
            if target.exists():
                raise ValueError('Recook would overwrite preserved instance: ' + str(target))
            target.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
    headers = {asset_info(p.read_bytes())['package'] for p in stage.rglob('*.uasset')}
    if headers != expected:
        raise ValueError('Material dependency inventory differs: ' + str(sorted(headers ^ expected)))
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    toc = output / 'CSS_OutfitMaterialNamespaceTrial_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = output / 'containers'
    converter.base_containers(GAME, mount)
    for suffix in ('.pak', '.utoc', '.ucas'):
        path = toc.with_suffix(suffix)
        (mount / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mount, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/')
    payloads = {str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()}
    for relative, digest in payloads.items():
        if Path(relative).suffix != '.uasset' and sha(output / 'readback' / relative) != digest:
            raise ValueError('Payload changed through IoStore: ' + relative)
    packages = output / 'packages.txt'
    packages.write_text('\n'.join(sorted(expected)) + '\n')
    references = output / 'references.txt'
    references.write_text('\n'.join(p + '.' + p.rsplit('/', 1)[1] for p in sorted(expected)) + '\n')
    commands = {
        'decode': [str(args.dotnet), str(ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'),
                   str(mount), str(args.mappings), str(packages), str(output / 'decoded.json'), '--shader-maps'],
        'imports': [str(args.dotnet), str(ROOT / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll'),
                    str(mount), str(args.mappings), str(packages), str(references), str(output / 'import-check.json')],
    }
    for name, command in commands.items():
        (output / (name + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        with (output / (name + '.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        (output / (name + '-exit.json')).write_text(json.dumps(dict(returncode=result.returncode)) + '\n')
        result.check_returncode()
    decoded = read(output / 'decoded.json')
    reverse = {v: k for k, v in mapping.items()}
    differences = [old for old, value in originals.items() if normalize(decoded[mapping[old]], reverse) != value]
    texture_payloads = []
    for old, exports in originals.items():
        if len(exports) != 1 or exports[0]['Type'] != 'Texture2D':
            continue
        old_base = instances / 'original/MortalShell2/Content' / old.removeprefix('/Game/')
        new_base = stage / 'MortalShell2/Content' / mapping[old].removeprefix('/Game/')
        for suffix in ('.uexp', '.ubulk'):
            before, after = old_base.with_suffix(suffix), new_base.with_suffix(suffix)
            if not before.exists() and not after.exists():
                continue
            same = before.is_file() and after.is_file() and sha(before) == sha(after)
            texture_payloads.append(dict(package=old, suffix=suffix, identical=same))
    (output / 'texture-payloads.json').write_text(json.dumps(texture_payloads, indent=2) + '\n')
    (output / 'comparison.json').write_text(json.dumps(dict(passed=not differences,
        packages=len(expected), identical=len(expected) - len(differences), differences=differences), indent=2) + '\n')
    if differences:
        raise ValueError('Cooked material fields differ; retain decoded comparisons')
    if any(not row['identical'] for row in texture_payloads):
        raise ValueError('Texture payload differs; inspect encoded mip data before acceptance')
    if not read(output / 'import-check.json')['passed']:
        raise ValueError('Migrated material imports do not resolve')
    (output / 'verification.json').write_text(json.dumps(dict(passed=True, packages=len(expected),
        payloads=payloads, files={toc.with_suffix(s).name: sha(toc.with_suffix(s)) for s in ('.pak', '.utoc', '.ucas')},
        evidence={str(p): sha(p) for p in [instances / 'verification.json', instances / 'original-decoded.json',
                  instances / 'request.json', *parent_evidence, *texture_evidence]},
        preserved_texture_packages=sorted(preserved_textures),
        installed=False, scope='Material subset only. Decoded fields and shader maps match after path normalization; '
                               'imports resolve without the original outfit mounted. Game acceptance remains.'), indent=2) + '\n')
    print(f'Verified {len(expected)} migrated material and texture packages')


if __name__ == '__main__':
    main()
