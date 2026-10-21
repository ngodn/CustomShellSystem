#!/usr/bin/env python3
"""Integrate verified material chunks into a complete CSS package. Python 3.14."""
import argparse
import copy
import json
from pathlib import Path
import re
import shutil
import subprocess

from css_convert import DEFAULT_REPAK, digest
from css_package import verify


def read(path):
    return json.loads(path.read_text())


def inventory(folder):
    return {str(p.relative_to(folder)): digest(p) for p in folder.rglob('*') if p.is_file()}


def listing(text):
    result = {}
    for line in text.splitlines():
        fields = line.split(maxsplit=5)
        if len(fields) != 6 or not re.fullmatch('[0-9a-f]{24}', fields[1]):
            raise ValueError('Unexpected retoc chunk listing')
        if fields[1] in result:
            raise ValueError('Duplicate chunk')
        result[fields[1]] = fields[2:]
    return result


def build(source, patch, version, output, retoc):
    original = verify(source)
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Expected a stable package version')
    old = original['version']
    parts = tuple(map(int, old.split('-')[0].split('.')))
    expected = '.'.join(map(str, parts if '-' in old else (*parts[:2], parts[2] + 1)))
    if version != expected:
        raise ValueError(f'Expected next patch version {expected}, got {version}')
    if output.exists():
        raise FileExistsError(output)
    output.mkdir(parents=True)
    source_files = inventory(source)
    patch_files = inventory(patch)
    stem = next(source.glob('*.pak')).stem
    patch_toc = next(patch.glob('*.utoc'))

    def run(label, *command):
        command = list(map(str, command))
        (output / (label + '-command.json')).write_text(json.dumps(command) + '\n')
        with (output / (label + '.log')).open('x') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        return (output / (label + '.log')).read_text()

    before = listing(run('source-index', retoc, 'list', '--hash', '--size', '--store', source / (stem + '.utoc')))
    repairs = listing(run('repair-index', retoc, 'list', '--hash', '--size', '--store', patch_toc))
    changed = sorted(before.keys() & repairs.keys())
    if not changed:
        raise ValueError('No repaired packages belong to this outfit')
    for key in changed:
        if before[key][1] != 'ExportBundleData' or repairs[key][1] != 'ExportBundleData':
            raise ValueError('Only material export chunks may be replaced')
        # Keeping the original header is valid only when imports, shader dependencies
        # and package-store counts match the independently packed replacement.
        if before[key][3] != repairs[key][3] or before[key][0] == repairs[key][0]:
            raise ValueError('Package-store dependencies changed or repair is a no-op')
    raw, fixed = output / 'raw', output / 'repair-raw'
    run('source-raw', retoc, 'unpack-raw', source / (stem + '.utoc'), raw)
    run('repair-raw', retoc, 'unpack-raw', patch_toc, fixed)
    raw_manifest, patch_manifest = read(raw / 'manifest.json'), read(fixed / 'manifest.json')
    for key in changed:
        if raw_manifest['chunk_paths'][key] != patch_manifest['chunk_paths'][key]:
            raise ValueError('Replacement package path changed')
        shutil.copy2(fixed / 'chunks' / key, raw / 'chunks' / key)
    trio = output / 'trio'
    trio.mkdir()
    run('pack', retoc, 'pack-raw', raw, trio / (stem + '.utoc'))
    run('integrity', retoc, 'verify', trio / (stem + '.utoc'))
    after = listing(run('result-index', retoc, 'list', '--hash', '--size', '--store', trio / (stem + '.utoc')))
    expected_chunks = dict(before)
    expected_chunks.update({key: repairs[key] for key in changed})
    if after != expected_chunks:
        raise ValueError('Full container changed outside the repaired material chunks')
    metadata = output / 'metadata'
    run('metadata-unpack', DEFAULT_REPAK, 'unpack', source / (stem + '.pak'), '--output', metadata)
    manifests = list(metadata.rglob('manifest.json'))
    if len(manifests) != 1:
        raise ValueError('Expected one CSS manifest')
    manifest_path = manifests[0]
    original_metadata = inventory(metadata)
    manifest = read(manifest_path)
    if manifest != original:
        raise ValueError('Source manifest changed during packaging')
    manifest = copy.deepcopy(original)
    manifest['version'] = version
    for outfit in manifest['catalog']['outfits']:
        if 'version' in outfit:
            outfit['version'] = version
    manifest['containers'] = {s: {'file': stem + s, 'bytes': (trio / (stem + s)).stat().st_size,
                                  'sha256': digest(trio / (stem + s))} for s in ('.utoc', '.ucas')}
    manifest_path.write_text(json.dumps(manifest, separators=(',', ':')) + '\n')
    run('metadata-pack', DEFAULT_REPAK, 'pack', metadata, trio / (stem + '.pak'), '--version', 'V8B')
    run('metadata-readback', DEFAULT_REPAK, 'unpack', trio / (stem + '.pak'), '--output', output / 'metadata-readback')
    if inventory(metadata) != inventory(output / 'metadata-readback'):
        raise ValueError('Metadata PAK round-trip failed')
    relative_manifest = str(manifest_path.relative_to(metadata))
    if {p: h for p, h in original_metadata.items() if p != relative_manifest} != {
            p: h for p, h in inventory(metadata).items() if p != relative_manifest}:
        raise ValueError('Unrelated PAK entries changed')
    if verify(trio) != manifest:
        raise ValueError('Final CSS package validation failed')
    if inventory(source) != source_files or inventory(patch) != patch_files:
        raise ValueError('Source package or repair changed during packaging')
    report = dict(passed=True, id=manifest['id'], name=manifest['name'], version=version,
                  previous_version=old, stem=stem, source=str(source), source_files=source_files,
                  repair=str(patch), repair_files=patch_files, replaced_materials=len(changed),
                  unchanged_chunks=len(before) - len(changed), container_header_unchanged=True,
                  store_entries_unchanged=True, metadata_changes=['version', 'catalog version', 'containers'],
                  files=inventory(trio), changed_chunks=changed)
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    # These are reproducible intermediate chunks, never installed or source files.
    for folder in (raw, fixed, output / 'metadata-readback'):
        shutil.rmtree(folder)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('source', 'patch', 'output', 'retoc'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    report = build(args.source.resolve(strict=True), args.patch.resolve(strict=True), args.version,
                   args.output.resolve(), args.retoc.resolve(strict=True))
    print(json.dumps({k: report[k] for k in ('name', 'version', 'replaced_materials', 'unchanged_chunks')}))
