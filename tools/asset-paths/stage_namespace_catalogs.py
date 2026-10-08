"""Stage private catalogs for verified author-owned outfit and shared containers."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css_convert import asset_info
from css_facial_resources import encode, externalize, resolve

REPAK = ROOT / 'build/repak/release/repak'
RETOC = ROOT / 'build/retoc-css-target/release/retoc'
PACKAGE_ROOT = Path('MortalShell2/Content/CSS/Packages')


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'namespace', 'metadata', 'eve', 'genessa', 'commander', 'shared', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise FileExistsError(output)
    if shutil.disk_usage(output.parent).free < 300 * 1024**2:
        raise ValueError('Need 300 MiB for metadata, packed resources and readback')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError('Changed input: ' + str(path))
        protected[str(path)] = digest

    for path in (args.baseline, args.namespace, REPAK, RETOC):
        protect(path)
    baseline = read(args.baseline)
    mapping = {row['old_package']: row['new_package'] for row in read(args.namespace)['assets']}
    candidates = dict(CSS_EveStellarBlade_eins0fx_P=args.eve,
                      CSS_UnholyGenessa_eins0fx_P=args.genessa,
                      CSS_CommanderWhite_eins0fx_P=args.commander,
                      CSS_SharedAssets_P=args.shared)
    containers, objects, owners, identities = {}, set(), {}, {}
    for container, directory in candidates.items():
        receipt = read(directory / 'verification.json')
        if receipt['passed'] is not True:
            raise ValueError('Failed candidate: ' + container)
        protect(directory / 'verification.json')
        files = {}
        for suffix in ('.utoc', '.ucas'):
            name = container + suffix
            file = directory / name
            if not file.exists():
                file = directory / container / name
            protect(file, receipt['files'][name])
            files[suffix] = dict(file=name, bytes=file.stat().st_size, sha256=sha(file), path=str(file.resolve()))
        info = subprocess.check_output([str(RETOC), 'info', files['.utoc']['path']], text=True)
        ids = re.findall(r'container_id: FIoContainerId\(([0-9a-f]+)\)', info)
        if len(ids) != 1 or ids[0] in identities.values():
            raise ValueError('Missing or duplicate internal container ID: ' + container)
        identities[container] = ids[0]
        containers[container] = files
        for relative, checksum in receipt['payloads'].items():
            if Path(relative).is_absolute() or '..' in Path(relative).parts:
                raise ValueError('Invalid payload path')
            if not relative.endswith('.uasset'):
                continue
            path = directory / 'stage' / relative
            protect(path, checksum)
            asset = asset_info(path.read_bytes())
            package = asset['package']
            if package in owners:
                raise ValueError('Duplicate package provider: ' + package)
            owners[package] = container
            objects.update(package + '.' + entry['name'] for entry in asset['exports'] if entry['outer'] == 0)

    references = set()

    def migrate(value):
        if isinstance(value, list):
            return [migrate(item) for item in value]
        if isinstance(value, dict):
            return {key: migrate(item) for key, item in value.items()}
        if not isinstance(value, str) or not value.startswith('/Game/CSS/'):
            return value
        package, separator, name = value.partition('.')
        target = mapping.get(package, package)
        result = target + separator + name
        if target not in owners or (separator and result not in objects):
            raise ValueError('Catalog reference has no candidate export: ' + value + ' -> ' + result)
        references.add(result)
        return result

    output.mkdir()
    pack = output / 'pack'
    pack.mkdir()
    metadata = output / 'metadata'
    metadata.mkdir()
    (output / 'readback').mkdir()

    def run(label, *command):
        with (output / (label + '.log')).open('x') as log:
            subprocess.run(list(map(str, command)), stdout=log, stderr=subprocess.STDOUT, check=True)

    def descriptors(container):
        return {suffix: {key: value for key, value in row.items() if key != 'path'}
                for suffix, row in containers[container].items()}

    originals, migrated, revisions, fixtures = {}, {}, {}, []
    for package in baseline['packages']:
        if 'historical_manifest' not in package:
            continue
        container = package['container']
        if container not in candidates:
            raise ValueError('Unexpected outfit in baseline')
        source = package['files']['.pak']
        protect(Path(source['path']), source['sha256'])
        stage = metadata / container
        run(container + '-unpack', REPAK, 'unpack', source['path'], '--output', stage)
        manifests = list(stage.rglob('manifest.json'))
        if len(manifests) != 1:
            raise ValueError('Expected one outfit manifest')
        original = read(manifests[0])
        pinned = package['historical_manifest']
        protect(Path(pinned['path']), pinned['sha256'])
        if original != read(Path(pinned['path'])):
            raise ValueError('Installed metadata differs from the audited outfit')
        candidate = migrate(original)
        candidate['containers'] = descriptors(container)
        candidate['dependencies'] = [dict(id='css.locomotion', contract='eins0fx.shared.author_namespace.v1',
                                          name='CSS Shared Assets')]
        originals[original['id']] = original
        migrated[original['id']] = candidate
        revisions[original['id']] = candidate['containers']
        for old, new in zip(original['catalog']['outfits'], candidate['catalog']['outfits'], strict=True):
            for ov, nv in zip(old['variants'], new['variants'], strict=True):
                skeleton = ('/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2'
                            if old['id'] == 'eins0fx.unholygenessa' else '/Game/CSS/Shared/SKEL_Base.SKEL_Base')
                fixtures.append(dict(outfit=old['id'], variant=ov['id'],
                                     old_animations=ov.get('animations', old.get('animations', {})),
                                     animations=nv.get('animations', new.get('animations', {})),
                                     old_skeleton=skeleton, skeleton=migrate(skeleton)))
        manifests[0].write_bytes(encode(candidate))

    shared_path = args.metadata / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    original = read(shared_path)
    protect(shared_path)
    resources = {}
    for resource in original['facial_resources']:
        path = shared_path.parent / resource['file']
        protect(path, resource['sha256'])
        resources[resource['file']] = path.read_bytes()
    inline = resolve(original, resources)
    candidate = copy.deepcopy(inline)
    library = candidate['catalog']['animation_library']
    for entry in library['entries']:
        entry['assets'] = migrate(entry['assets'])
        aliases = entry.get('aliases', [])
        for alias in list(aliases):
            mapped = migrate(alias)
            if mapped not in aliases:
                aliases.append(mapped)
        if entry.get('slot') in ('walk', 'jog', 'sprint'):
            asset = migrate('/Game/CSS/Eve/Anim/BS_S1_' + entry['slot'].capitalize()
                            + '.BS_S1_' + entry['slot'].capitalize())
            entry['assets'].insert(0, dict(asset=asset, skeleton=migrate('/Game/CSS/Shared/SKEL_Base.SKEL_Base')))
    for adapters in [library['adapters'], *[face['adapters'] for face in library['faces']]]:
        for index, adapter in enumerate(adapters):
            updated = migrate(adapter)
            revision = revisions[adapter['source_outfit']]
            updated['source_toc_sha256'] = revision['.utoc']['sha256']
            updated['source_bulk_bytes'] = revision['.ucas']['bytes']
            adapters[index] = updated
    candidate['containers'] = descriptors('CSS_SharedAssets_P')
    candidate['provides'] = ['eins0fx.shared.author_namespace.v1']
    candidate['name'] = library['name'] = 'CSS Shared Assets'
    # Public IDs and facial curves remain unchanged; this is not a release version bump.
    manifest, resources = externalize(candidate)
    stage = metadata / 'CSS_SharedAssets_P'
    folder = stage / PACKAGE_ROOT / candidate['id']
    folder.mkdir(parents=True)
    (folder / 'manifest.json').write_bytes(encode(manifest))
    for name, payload in resources.items():
        (folder / name).write_bytes(payload)
    if [e['id'] for e in library['entries']] != [e['id'] for e in inline['catalog']['animation_library']['entries']]:
        raise ValueError('Animation identities changed')
    for old, new in zip(inline['catalog']['animation_library']['faces'], library['faces'], strict=True):
        for key in set(old) - {'adapters'}:
            if old[key] != new[key]:
                raise ValueError('Facial controls or performance curves changed: ' + key)
    packed_files = {}
    for container in candidates:
        stage = metadata / container
        for suffix, row in containers[container].items():
            (pack / (container + suffix)).symlink_to(row['path'])
        pak = pack / (container + '.pak')
        run(container + '-pack', REPAK, 'pack', stage, pak, '--version', 'V8B')
        unpacked = output / 'readback' / container
        run(container + '-readback', REPAK, 'unpack', pak, '--output', unpacked)
        staged = {str(p.relative_to(stage)): sha(p) for p in stage.rglob('*') if p.is_file()}
        decoded = {str(p.relative_to(unpacked)): sha(p) for p in unpacked.rglob('*') if p.is_file()}
        if staged != decoded:
            raise ValueError('Packed metadata/resource readback differs')
        packed_files[pak.name] = sha(pak)
        shutil.rmtree(unpacked)
    for path, checksum in protected.items():
        if sha(Path(path)) != checksum:
            raise ValueError('Input changed during staging: ' + path)
    (output / 'variants.json').write_bytes(encode(fixtures))
    (output / 'original-outfits.json').write_bytes(encode(originals))
    (output / 'migrated-outfits.json').write_bytes(encode(migrated))
    (output / 'shared-inline.json').write_bytes(encode(candidate))
    (output / 'receipt.json').write_text(json.dumps(dict(
        passed=True, installed=False, inputs=protected, container_ids=identities,
        files=packed_files, asset_owners=owners, references=sorted(references),
        variants=len(fixtures), entries=len(library['entries']), faces=len(library['faces']),
        pending=['Native catalog loader and selection tests', 'Legacy outfit adapter/dependency upgrade path',
                 'Clean/upgrade installation and live gameplay acceptance'],
        scope='Private four-package metadata assembly. All active CSS object references resolve in verified candidate stages; legacy aliases intentionally retain original paths.'
    ), indent=2) + '\n')
    print(f'Staged {len(fixtures)} variants, {len(library["entries"])} animations and {len(library["faces"])} facial rigs')


if __name__ == '__main__':
    main()
