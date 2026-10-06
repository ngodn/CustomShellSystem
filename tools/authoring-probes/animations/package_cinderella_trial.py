"""Add the verified Cinderella cook and source facial keys to the private shared trial."""
import argparse
import copy
import errno
import json
from pathlib import Path
import shutil

from package_corrected_dance_trial import (
    ROOT, STEM, GAME, PACKAGE_ROOT, DEFAULT_REPAK, Converter, asset_info, read, sha,
)
from prepare_performance_curves import native_performance

CLIP = 'css.eve.dance.cinderella'
ASSET = '/Game/CSS/Locomotion/Studies/C10/AN_Cinderella.AN_Cinderella'
MESHES = {
    '/Game/CSS/EveTest/SK_' + name
    for name in ('ACloth3', 'BFit1', 'KCloth4', 'PFit14', 'SFit16')
} | {'/Game/CSS/SeduXtress/SK_BlackPearl2', '/Game/CSS/UnholyGenessa/SK_EveW3',
     '/Game/CSS/UnholyGenessa/SK_GenessaW3', '/Game/CSS/CommanderWhite/Dev/SK_CommanderC4'}
MESHES = {path + '.' + path.rsplit('/', 1)[1] for path in MESHES}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    output = parser.parse_args().output.resolve()
    work = ROOT / 'work/cine-animation-port'
    if output.exists() or not any(output.is_relative_to(parent) for parent in
            (work, Path('/mnt/eins0fxE/CSS-authoring-scratch'))):
        raise ValueError('Use a new private animation workspace')
    if shutil.disk_usage(output.parent).free < 3 * 1024**3:
        raise ValueError('Need 3 GiB for packaging and readback')
    base = work / 'eve-face-jali3-trial-v2'
    extra = work / 'cinderella-c10-cook/pack'
    protected, dependencies, payloads = {}, {}, {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        actual = sha(path)
        if expected is not None and actual != expected:
            raise ValueError('Verified input changed: ' + str(path))
        if protected.setdefault(str(path), actual) != actual:
            raise ValueError('Input changed while reading: ' + str(path))

    for folder, count_key, count in ((base, 'exports', 96), (extra, 'packages', 2)):
        report = read(folder / 'verification.json')
        if report.get('passed') is not True or report[count_key] != count:
            raise ValueError('Unexpected source package verification')
        protect(folder / 'verification.json')
        for name, checksum in report['files'].items():
            protect(folder / STEM / name, checksum)
        for path, checksum in report['dependencies'].items():
            protect(Path(path), checksum)
            if dependencies.setdefault(path, checksum) != checksum:
                raise ValueError('Conflicting dependency revisions')
        headers = sorted((folder / 'stage').rglob('*.uasset'))
        if len(headers) != count:
            raise ValueError('Source export count changed')
        for header in headers:
            if not asset_info(header.read_bytes())['package'].startswith('/Game/CSS/Locomotion/'):
                raise ValueError('Asset outside shared namespace')
            for source in (header, header.with_suffix('.uexp')):
                relative = source.relative_to(folder / 'stage')
                if relative in payloads:
                    raise ValueError('Duplicate input export')
                roundtrip = folder / 'readback' / relative
                if source.suffix == '.uexp' and sha(source) != sha(roundtrip):
                    raise ValueError('Source export differs from verified readback')
                protect(source)
                protect(roundtrip)
                payloads[relative] = source

    motion_path = work / 'cinderella-c10-cook/full-motion-verification.json'
    motion = read(motion_path)
    if (motion.get('complete') is not True or motion.get('passed') is not True or
            motion['asset'] != ASSET or motion['frames'] != 8381 or motion['tracks'] != 388):
        raise ValueError('Require full cooked motion verification')
    protect(motion_path)
    for path, checksum in motion['inputs'].items():
        protect(Path(path), checksum)
    evaluation_path = work / 'cinderella-c10-adapters/cinderella-current-result.json'
    evaluation = read(evaluation_path)
    if not all(evaluation.get(key) is True for key in ('complete', 'passed', 'body_passed', 'fingers_passed')):
        raise ValueError('Adapter evaluation failed')
    protect(evaluation_path)
    for path, checksum in evaluation['protected'].items():
        # Editor source mounts are empty outside its sandbox. Container revisions
        # above remain the authority for those installed outfit dependencies.
        if Path(path).is_file():
            protect(Path(path), checksum)
    rows = evaluation['results']
    required_cases = {'inactive', 'active_start', 'active_seek', 'held_end', 'stopped',
                      'null_clip_guard', 'restored', 'zero_weight', 'full_clip'}
    if len(rows) != 9 or {row['mesh'] for row in rows} != MESHES:
        raise ValueError('Require exactly the nine requested meshes')
    for row in rows:
        if (row['performance'] != ASSET or row['finger_bones_checked'] != 38 or
                len(row['cases']) != 9 or {case['case'] for case in row['cases']} != required_cases or
                not all(case['body_passed'] and case['fingers_passed'] for case in row['cases'])):
            raise ValueError('Incomplete playback/restoration coverage')

    manifest_path = base / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    protect(manifest_path)
    original = read(manifest_path)
    manifest = copy.deepcopy(original)
    library = manifest['catalog']['animation_library']
    if len(library['entries']) != 62 or len(library['adapters']) != 5 or len(library['faces']) != 2:
        raise ValueError('Unexpected baseline catalog')
    face = next(rig for rig in library['faces'] if rig['id'] == 'css.eve.face')
    if len(face['performances']) != 5 or len(face['morphs']) != 115:
        raise ValueError('Expected the verified Eve face')
    curves_path = work / 'facial-cinderella-v1/curves.json'
    evidence_path = curves_path.with_name('verification.json')
    evidence = read(evidence_path)
    protect(evidence_path)
    protect(curves_path, evidence['prepared_sha256'])
    if (evidence.get('passed') is not True or evidence['curves'] != 12 or evidence['keys'] != 3897):
        raise ValueError('Missing source curve verification')
    curves = read(curves_path)
    for path, checksum in evidence['inputs'].items():
        protect(Path(path), checksum)
    source = work / 'author-head-source-v1/cinderella-audit/source-mesh.json'
    if str(source) not in evidence['inputs']:
        raise ValueError('Facial metadata is not bound to the source')
    performance = native_performance(read(source), face['morphs'], CLIP)
    if performance != curves['performance']:
        raise ValueError('Prepared performance differs from original keys')
    library['entries'].append(dict(id=CLIP, name='Cinderella', category='dance', playback='once',
        hide_weapons=True, assets=[dict(asset=ASSET, skeleton='/Game/CSS/Shared/SKEL_Base.SKEL_Base')]))
    face['performances'].append(performance)
    if len({entry['id'] for entry in library['entries']}) != 63:
        raise ValueError('Animation identity collision')
    restored = copy.deepcopy(manifest)
    restored['catalog']['animation_library']['entries'].pop()
    next(rig for rig in restored['catalog']['animation_library']['faces']
         if rig['id'] == 'css.eve.face')['performances'].pop()
    if restored != original:
        raise ValueError('Unrelated catalog data changed')

    output.mkdir()
    for relative, source in payloads.items():
        target = output / 'stage' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            target.hardlink_to(source)
        except OSError as error:
            if error.errno != errno.EXDEV:
                raise
            shutil.copy2(source, target)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output / STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', output / 'stage', trio / (STEM + '.utoc'), '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio / (STEM + '.utoc'))
    mounts = output / 'containers'
    converter.base_containers(GAME, mounts)
    for path in [Path(p) for p in dependencies] + list(trio.iterdir()):
        (mounts / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mounts, output / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, source in payloads.items():
        decoded = output / 'readback' / relative
        if source.suffix == '.uexp':
            if sha(source) != sha(decoded):
                raise ValueError('Merged export changed: ' + str(relative))
        elif asset_info(source.read_bytes())['package'] != asset_info(decoded.read_bytes())['package']:
            raise ValueError('Package identity changed')
    manifest['containers'] = {suffix: dict(file=STEM + suffix, bytes=(trio / (STEM + suffix)).stat().st_size,
        sha256=sha(trio / (STEM + suffix))) for suffix in ('.utoc', '.ucas')}
    metadata = json.dumps(manifest, separators=(',', ':'), allow_nan=False) + '\n'
    if len(metadata.encode('utf-8')) > 256 * 1024:
        raise ValueError('Catalog exceeds the existing runtime limit')
    path = output / 'metadata' / PACKAGE_ROOT / 'css.locomotion/manifest.json'
    path.parent.mkdir(parents=True)
    path.write_text(metadata)
    converter.run(DEFAULT_REPAK, 'pack', output / 'metadata', trio / (STEM + '.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio / (STEM + '.pak'), '--output', output / 'metadata-readback')
    if read(output / 'metadata-readback' / PACKAGE_ROOT / 'css.locomotion/manifest.json') != manifest:
        raise ValueError('Catalog round-trip differs')
    if any(sha(Path(path)) != checksum for path, checksum in protected.items()):
        raise ValueError('A protected input changed during packaging')
    report = dict(passed=True, entries=63, exports=98, unchanged_existing_entries=62,
        unchanged_existing_exports=96, adapters=5, native_candidates=4, facial_rigs=2,
        eve_performances=6, metadata_bytes=len(metadata.encode('utf-8')),
        protected=protected, dependencies=dependencies, files={p.name: sha(p) for p in trio.iterdir()},
        installed=False, scope='Private candidate; visual, secondary physics and live acceptance remain.')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print('CINDERELLA_TRIAL: 63 identities, 98 exports, six Eve facial performances', flush=True)


if __name__ == '__main__':
    main()
