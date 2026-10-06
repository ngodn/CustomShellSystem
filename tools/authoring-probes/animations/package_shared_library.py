"""Package audited animations, adapters and optional facial meshes without installing them."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT, asset_info
from css_paths import package_path, windows_destination
from adapter_additions import load_addition


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--dependency', type=Path, required=True, action='append',
                        help='Verified outfit trio supplying declared dependencies; repeat for additional outfits')
    parser.add_argument('--output-name', default='pack')
    parser.add_argument('--adapter-cook', help='Successful adapter cook label from run_performance_layers.py')
    parser.add_argument('--adapter-evaluation', help='Successful nine-mesh evaluated-pose label')
    parser.add_argument('--adapter-bindings', type=Path, help='Independent cooked/source comparison receipt')
    parser.add_argument('--adapter-addition', type=Path, action='append', default=[],
                        help='Additional adapter cook, graph and complete moving-clip evidence')
    parser.add_argument('--faces', type=Path, help='Verified facial workspace from prepare_facial_catalog.py')
    parser.add_argument('--face-catalog', type=Path, help='Verified metadata revision using the same facial cook')
    args = parser.parse_args()
    if args.face_catalog and not args.faces:
        raise ValueError('A facial catalog requires its verified cook workspace')
    work = args.work.resolve(strict=True)
    if json.loads((work / 'cook-exit.json').read_text())['exit_code'] != 0:
        raise ValueError('Cook did not exit successfully')
    verification = json.loads((work / 'verify-result.json').read_text())
    if verification['complete'] is not True:
        raise ValueError('Fresh editor readback is incomplete')
    catalog = json.loads((work / 'catalog.json').read_text())
    library = catalog['animation_library']
    packages = (work / 'cook.txt').read_text().splitlines()
    expected = {entry['assets'][0]['asset'].split('.')[0] for entry in library['entries']}
    verified = {entry['asset'].split('.')[0] for entry in verification['results']}
    if len(packages) != len(set(packages)) or set(packages) != expected or expected != verified:
        raise ValueError('Cook, catalog and fresh readback disagree')
    for value in (args.output_name, args.adapter_cook, args.adapter_evaluation):
        if value is not None and not re.fullmatch(r'[a-z0-9-]{1,48}', value):
            raise ValueError('Expected a short work label')
    if len({bool(args.adapter_cook), bool(args.adapter_evaluation), bool(args.adapter_bindings)}) != 1:
        raise ValueError('Adapter cook, evaluation and binding readback are required together')
    adapter_packages = set()
    additional_payloads = {}
    additional_evidence = {}
    if args.adapter_cook:
        if json.loads((work / (args.adapter_cook + '-exit.json')).read_text())['exit_code'] != 0:
            raise ValueError('Adapter cook did not succeed')
        evaluated = json.loads((work / (args.adapter_evaluation + '-result.json')).read_text())
        if evaluated.get('complete') is not True or evaluated.get('body_passed') is not True or len(evaluated['results']) != 9:
            raise ValueError('Nine-mesh adapter evaluation did not pass')
        if any(not any(case['case'] == 'full_clip' and case['body_passed'] for case in row['cases'])
               for row in evaluated['results']):
            raise ValueError('Full moving-clip comparison is required on every mesh')
        adapter_packages = {'/Game/CSS/Locomotion/Adapters/ABP_' + name
                            for name in ('Eve', 'EveBikini', 'EveKnit', 'Genessa', 'CommanderWhite')}
        if set((work / (args.adapter_cook + '-cook.txt')).read_text().splitlines()) != adapter_packages:
            raise ValueError('Unexpected cooked adapter set')
        if {row['layer'].split('.')[0] for row in evaluated['results']} != adapter_packages:
            raise ValueError('Evaluated adapter set differs from cook')
        for package in adapter_packages:
            relative = '/Content/' + package.removeprefix('/Game/') + '.uasset'
            inputs = [(path, checksum) for path, checksum in evaluated['protected'].items() if path.endswith(relative)]
            if len(inputs) != 1 or digest(Path(inputs[0][0])) != inputs[0][1]:
                raise ValueError('Evaluated editor graph has changed: ' + package)
        packages += sorted(adapter_packages)
    dependency_files = []
    for path in args.dependency:
        dependency = path.resolve(strict=True)
        if dependency.suffix != '.utoc':
            raise ValueError('Dependency must be a .utoc with .ucas companion')
        dependency_files += [dependency, dependency.with_suffix('.ucas')]
    if len({path.name for path in dependency_files}) != len(dependency_files):
        raise ValueError('Dependencies have duplicate container filenames')
    dependency_hashes = {str(path): digest(path) for path in dependency_files}
    if args.adapter_bindings:
        bindings = json.loads(args.adapter_bindings.read_text())
        if bindings.get('passed') is not True or bindings.get('contract') != 'explicit-weight-v1':
            raise ValueError('Cooked adapter comparison did not pass')
        for path, checksum in bindings['evidence'].items():
            if digest(Path(path)) != checksum:
                raise ValueError('Adapter comparison evidence changed: ' + path)
        adapters = bindings['adapters']
        if len(adapters) != 5 or {entry['target_class'].split('.')[0] for entry in adapters} != adapter_packages:
            raise ValueError('Adapter metadata differs from the cooked set')
        library['adapters'] = adapters
    for specification in args.adapter_addition:
        additions, payloads, evidence = load_addition(specification,
            {entry['id'] for entry in library['entries'] if entry['playback'] == 'once'})
        if set(payloads).intersection(packages):
            raise ValueError('Additional adapter duplicates a packaged asset')
        existing = library.setdefault('adapters', [])
        identities = {(row['source_outfit'], row['source_toc_sha256'], row['source_class']) for row in existing}
        for row in additions:
            identity = (row['source_outfit'], row['source_toc_sha256'], row['source_class'])
            if identity in identities:
                raise ValueError('Conflicting additional adapter binding')
            identities.add(identity)
        existing += additions
        if len(existing) > 32:
            raise ValueError('Adapter bindings exceed runtime catalog limit')
        packages += sorted(payloads)
        adapter_packages.update(payloads)
        additional_payloads.update(payloads)
        additional_evidence.update(evidence)
    if library.get('adapters'):
        for entry in library['adapters']:
            candidates = [path for path in dependency_files if path.suffix == '.utoc' and
                          dependency_hashes[str(path)] == entry['source_toc_sha256'] and
                          path.with_suffix('.ucas').stat().st_size == entry['source_bulk_bytes']]
            if len(candidates) != 1:
                raise ValueError('Adapter source revision is absent or ambiguous')
    face_packages = set()
    face_evidence = {}
    face_work = None
    if args.faces:
        face_work = args.faces.resolve(strict=True)
        catalog_work = args.face_catalog.resolve(strict=True) if args.face_catalog else face_work
        evidence_path = catalog_work / 'facial-catalog-evidence.json'
        rig_path = catalog_work / 'facial-rig.json'
        evidence = json.loads(evidence_path.read_text())
        if evidence.get('passed') is not True or digest(rig_path) != evidence['rig_sha256']:
            raise ValueError('Facial catalog differs from its verified evidence')
        face_evidence = dict(evidence['protected'])
        face_evidence[str(evidence_path)] = digest(evidence_path)
        face_evidence[str(rig_path)] = digest(rig_path)
        if any(digest(Path(path)) != checksum for path, checksum in face_evidence.items()):
            raise ValueError('Facial verification inputs changed')
        if json.loads((face_work / 'cook-exit.json').read_text())['exit_code'] != 0:
            raise ValueError('Facial cook failed')
        rig = json.loads(rig_path.read_text())
        readback = json.loads((face_work / 'cooked-readback-v1.json').read_text())
        if readback.get('passed') is not True or len(rig['adapters']) != 6:
            raise ValueError('Expected six verified Eve facial bindings')
        expected_pairs = {(entry['source_mesh'].split('.')[0], entry['target_mesh'].split('.')[0])
                          for entry in rig['adapters']}
        if expected_pairs != {(row['source'], row['target']) for row in readback['results'] if row['passed']}:
            raise ValueError('Facial catalog and cooked readback disagree')
        for entry in rig['adapters']:
            candidates = [path for path in dependency_files if path.suffix == '.utoc' and
                          dependency_hashes[str(path)] == entry['source_toc_sha256'] and
                          path.with_suffix('.ucas').stat().st_size == entry['source_bulk_bytes']]
            if len(candidates) != 1:
                raise ValueError('Facial source revision is absent or ambiguous')
            package = entry['target_mesh'].split('.')[0]
            if not package.startswith('/Game/CSS/Locomotion/Faces/'):
                raise ValueError('Facial mesh is outside the shared namespace')
            face_packages.add(package)
        if len(face_packages) != 6 or face_packages.intersection(packages):
            raise ValueError('Duplicate facial mesh package')
        library['faces'] = [rig]
        packages += sorted(face_packages)
    out = work / args.output_name
    out.mkdir(exist_ok=False)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    stage = out / 'stage'
    payloads = {}
    for package in packages:
        package_path(package)
        if not package.startswith('/Game/CSS/Locomotion/'):
            raise ValueError(f'Unexpected package: {package}')
        relative = package.removeprefix('/Game/')
        for suffix in ('.uasset', '.uexp'):
            cook_root = (work / (args.adapter_cook + '-cooked')) if args.adapter_cook and package in adapter_packages else work / 'cooked'
            if package in face_packages:
                cook_root = face_work / 'cooked'
            source = cook_root / 'CSSAuthoring/Content' / (relative + suffix)
            if package in additional_payloads:
                source = additional_payloads[package][suffix]
            if package in face_packages and face_evidence.get(str(source)) != digest(source):
                raise ValueError('Facial payload has not passed independent readback: ' + package)
            destination = stage / 'MortalShell2/Content' / (relative + suffix)
            windows_destination('C:/CSS', str(destination.relative_to(stage)))
            if suffix == '.uasset':
                info = asset_info(source.read_bytes())
                expected_class = ('SkeletalMesh' if package in face_packages else
                                  'AnimBlueprintGeneratedClass' if package in adapter_packages else 'AnimSequence')
                if info['package'] != package or not any(e['class'] == expected_class for e in info['exports']):
                    raise ValueError(f'Not the expected cooked animation asset: {package}')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            payloads[str(destination.relative_to(stage))] = digest(source)
    stem = 'CSS_LocomotionSharedAssets_P'
    trio = out / stem
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', stage, trio / (stem + '.utoc'),
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (stem + '.utoc'))
    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for path in dependency_files + list(trio.iterdir()):
        (mount / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mount, out / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, checksum in payloads.items():
        decoded = out / 'readback' / relative
        if not decoded.is_file():
            raise ValueError(f'Missing round-trip file: {relative}')
        if decoded.suffix == '.uexp' and digest(decoded) != checksum:
            raise ValueError(f'Round-trip changed export bytes: {relative}')
        if decoded.suffix == '.uasset':
            asset_info(decoded.read_bytes())  # Reject unresolved imports, not just missing files.
    manifest = dict(format='CSS.Package', format_version=1, game='MortalShell2', engine='5.6',
                    kind='animation_library', catalog=catalog,
                    **{key: library[key] for key in ('id', 'name', 'author', 'version')})
    manifest['containers'] = {suffix: dict(file=stem + suffix, bytes=(trio / (stem + suffix)).stat().st_size,
                                         sha256=digest(trio / (stem + suffix))) for suffix in ('.utoc', '.ucas')}
    metadata = out / 'metadata'
    path = metadata / PACKAGE_ROOT / library['id'] / 'manifest.json'
    path.parent.mkdir(parents=True)
    path.write_text(json.dumps(manifest, indent=2) + '\n')
    converter.run(DEFAULT_REPAK, 'pack', metadata, trio / (stem + '.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio / (stem + '.pak'), '--output', out / 'metadata-readback')
    if json.loads((out / 'metadata-readback' / PACKAGE_ROOT / library['id'] / 'manifest.json').read_text()) != manifest:
        raise ValueError('Manifest did not round-trip exactly')
    if any(digest(Path(path)) != checksum for path, checksum in dependency_hashes.items()):
        raise ValueError('Dependency changed during verification')
    if any(digest(Path(path)) != checksum for path, checksum in face_evidence.items()):
        raise ValueError('Facial evidence changed during packaging')
    if any(digest(Path(path)) != checksum for path, checksum in additional_evidence.items()):
        raise ValueError('Additional adapter evidence changed during packaging')
    report = dict(passed=True, packages=len(packages), exact_export_payloads=len(packages),
                  adapters=sorted(adapter_packages), adapter_evaluation=args.adapter_evaluation,
                  additional_adapter_evidence=additional_evidence,
                  facial_meshes=sorted(face_packages), facial_evidence=face_evidence,
                  dependencies=dependency_hashes,
                  required_skeleton='/Game/CSS/Shared/SKEL_Base.SKEL_Base',
                  files={path.name: digest(path) for path in trio.iterdir()},
                  scope='Private structural package check. Decoded motion and live playback remain pending.')
    (out / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
