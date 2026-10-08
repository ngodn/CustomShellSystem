"""Verify the migrated Genessa outfit with its two authored facial meshes."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info
from verify_outfit_material_trial import normalize, read, sha

FACE_ROOT = '/Game/CSS/eins0fx/UnholyGenessa/Faces/'
FACES = [FACE_ROOT + name for name in ('SK_EveW3_Face', 'SK_GenessaW3_Face')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base', 'editor', 'cook', 'face-baseline', 'shared', 'dotnet', 'mappings', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise FileExistsError(output)
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Changed input: ' + str(path))
        protected[str(path)] = digest

    for receipt in (args.base / 'verification.json', args.editor / 'reload.json',
                    args.editor / 'source-validation.json', args.shared / 'verification.json',
                    args.face_baseline / 'oral-verification.json'):
        if not read(receipt)['passed']:
            raise ValueError('Failed prerequisite: ' + str(receipt))
        protect(receipt)
    if read(args.cook / 'exit.json') != dict(returncode=0, sources_unchanged=True):
        raise ValueError('Facial cook did not pass')
    if (args.cook / 'packages.txt').read_text().splitlines() != FACES:
        raise ValueError('Unexpected facial cook selection')
    for path, digest in read(args.cook / 'sources.json').items():
        protect(Path(path), digest)
    for path in (args.cook / 'exit.json', args.cook / 'sources.json', args.editor / 'input.json'):
        protect(path)
    for path in (args.base / 'decoded.json', args.base / 'references.txt',
                 args.face_baseline / 'pack/CSS_GenessaFaceTrial_P.utoc',
                 args.face_baseline / 'pack/CSS_GenessaFaceTrial_P.ucas'):
        protect(path)
    plan = read(args.editor / 'input.json')
    reverse = {r['new_package']: r['old_package'] for r in plan['assets']}
    sources = {}

    def add(source, relative, expected=None):
        protect(source, expected)
        if relative in sources:
            raise ValueError('Duplicate payload provider: ' + str(relative))
        sources[relative] = source

    for relative, digest in read(args.base / 'verification.json')['payloads'].items():
        if Path(relative).is_absolute() or '..' in Path(relative).parts:
            raise ValueError('Invalid base payload path')
        add(args.base / 'stage' / relative, Path(relative), digest)
    for package in FACES:
        relative = Path(package.removeprefix('/Game/')).with_suffix('.uasset')
        for suffix in ('.uasset', '.uexp', '.ubulk'):
            source = args.cook / 'cooked/CSSAuthoring/Content' / relative.with_suffix(suffix)
            if suffix == '.ubulk' and not source.exists():
                continue
            add(source, Path('MortalShell2/Content') / relative.with_suffix(suffix))
    oral = read(args.face_baseline / 'oral-verification.json')
    if len(oral['mapped_assets']) != 4:
        raise ValueError('Expected four preserved oral packages')
    for row in oral['exact_payloads']:
        protect(args.face_baseline.parent.parent / row['target'], row['sha256'])
    for package in oral['mapped_assets'].values():
        if not package.startswith(FACE_ROOT + 'Mouth/'):
            raise ValueError('Unexpected oral package')
        relative = Path('MortalShell2/Content') / Path(package.removeprefix('/Game/'))
        for suffix in ('.uasset', '.uexp', '.ubulk'):
            source = args.face_baseline / 'pack/legacy' / relative.with_suffix(suffix)
            if suffix == '.ubulk' and not source.exists():
                continue
            add(source, relative.with_suffix(suffix))
    required = 3 * sum(p.stat().st_size for p in sources.values()) + 256 * 1024**2
    if shutil.disk_usage(output.parent).free < required:
        raise ValueError(f'Need {required} free bytes for staging, container and readback')
    output.mkdir()
    stage = output / 'stage'
    for relative, source in sources.items():
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['cp', '--reflink=auto', str(source), str(target)], check=True)
    expected = set(read(args.base / 'decoded.json')) | set(FACES) | set(oral['mapped_assets'].values())
    headers = {asset_info(p.read_bytes())['package'] for p in stage.rglob('*.uasset')}
    if len(headers) != 107 or headers != expected - {'/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base'}:
        raise ValueError('Incomplete outfit plus facial package inventory')
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    toc = output / 'CSS_UnholyGenessa_eins0fx_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = output / 'containers'
    converter.base_containers(GAME, mount)
    for suffix in ('.pak', '.utoc', '.ucas'):
        path = toc.with_suffix(suffix)
        (mount / path.name).symlink_to(path)
    for name, digest in read(args.shared / 'verification.json')['files'].items():
        path = args.shared / 'CSS_SharedAssets_P' / name
        protect(path, digest)
        (mount / name).symlink_to(path.resolve())
    converter.run(converter.retoc, 'to-legacy', mount, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/eins0fx/UnholyGenessa/')
    payloads = {str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()}
    for relative, digest in payloads.items():
        if Path(relative).suffix != '.uasset' and sha(output / 'readback' / relative) != digest:
            raise ValueError('Export/bulk payload changed: ' + relative)
    packages = output / 'packages.txt'
    packages.write_text('\n'.join(sorted(expected)) + '\n')
    face_paths = output / 'faces.txt'
    face_paths.write_text('\n'.join(FACES) + '\n')
    oral_paths = output / 'oral.txt'
    oral_paths.write_text('\n'.join(oral['mapped_assets'].values()) + '\n')
    references = output / 'references.txt'
    references.write_text((args.base / 'references.txt').read_text() + '\n'.join(
        p + '.' + p.rsplit('/', 1)[1] for p in [*FACES, *oral['mapped_assets'].values()]) + '\n')
    reader = ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'
    commands = {
        'candidate': [reader, mount, args.mappings, packages, output / 'decoded.json', '--shader-maps'],
        'original-face': [reader, args.face_baseline / 'pack/containers', args.mappings,
                          face_paths, output / 'original-faces.json', '--mesh-digests'],
        'original-oral': [reader, args.face_baseline / 'pack/containers', args.mappings,
                          oral_paths, output / 'original-oral.json', '--shader-maps'],
        'candidate-face': [reader, mount, args.mappings, face_paths, output / 'faces.json', '--mesh-digests'],
        'imports': [ROOT / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll',
                    mount, args.mappings, packages, references, output / 'import-check.json'],
    }
    for name, values in commands.items():
        command = [str(args.dotnet), *map(str, values)]
        (output / (name + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
        with (output / (name + '.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        (output / (name + '-exit.json')).write_text(json.dumps(dict(returncode=result.returncode)) + '\n')
        result.check_returncode()
    original = read(args.base / 'decoded.json')
    decoded = read(output / 'decoded.json')
    differences = [p for p, value in original.items() if decoded[p] != value]
    oral_original = read(output / 'original-oral.json')
    differences += [p for p, value in oral_original.items() if decoded[p] != value]
    face_original, face_candidate = read(output / 'original-faces.json'), read(output / 'faces.json')
    differences += [p for p in FACES if normalize(face_candidate[p], reverse) != face_original[p]]
    result = dict(passed=not differences, packages=107, compared=len(expected),
                  facial_meshes=2, differences=differences, installed=False)
    (output / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    if differences:
        raise ValueError('Export or facial buffer comparison failed: ' + ', '.join(differences))
    for path, digest in protected.items():
        if sha(Path(path)) != digest:
            raise ValueError('Protected source changed: ' + path)
    result.update(payloads=payloads, protected=protected,
                  files={toc.with_suffix(s).name: sha(toc.with_suffix(s)) for s in ('.pak', '.utoc', '.ucas')},
                  scope='Migrated outfit and new facial meshes with original oral materials. '
                        'Full cooked comparisons and imports; not live acceptance or release metadata.')
    (output / 'verification.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Verified 107 outfit packages, two facial meshes and shared skeleton references')


if __name__ == '__main__':
    main()
