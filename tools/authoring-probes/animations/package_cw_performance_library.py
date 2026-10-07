"""Replace the CW face and add its five verified performances to a private shared pack."""
import argparse
import copy
import errno
import json
from pathlib import Path
import shutil

import numpy as np

from package_corrected_dance_trial import (
    ROOT, STEM, GAME, PACKAGE_ROOT, DEFAULT_REPAK, Converter, asset_info, read, sha,
)
from css_facial_resources import encode, externalize, resolve


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--face', type=Path, required=True)
    parser.add_argument('--basis', type=Path, required=True)
    parser.add_argument('--curves', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    base, face, basis, curve_index = (p.resolve(strict=True) for p in
                                      (args.base, args.face, args.basis, args.curves))
    output = args.output.resolve()
    roots = (ROOT/'work/cine-animation-port', Path('/mnt/eins0fxE/CSS-authoring-scratch'))
    if output.exists() or not any(output.is_relative_to(root) for root in roots):
        raise ValueError('Use new private animation storage')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        checksum = sha(path)
        if (expected is not None and checksum != expected) or protected.setdefault(str(path), checksum) != checksum:
            raise ValueError('Verified input changed: '+str(path))
        return path

    def checked(path):
        return read(protect(path))

    baseline = checked(base/'verification.json')
    fitted = checked(basis/'report.json')
    decoded = checked(face/'cooked-readback-v1.json')
    if (not baseline.get('passed') or baseline['exports'] != 98 or
            not fitted.get('passed') or fitted['total_shapes'] > 128 or
            not decoded.get('passed') or not decoded.get('source_deltas_checked') or
            len(decoded['results']) != 1):
        raise ValueError('Require the complete base pack and independently decoded combined face')
    protect(face/'readback-shapes.json', decoded['shapes_sha256'])
    protect(face/'vertex-maps.json', decoded['vertex_maps_sha256'])
    readback_shapes = read(face/'readback-shapes.json')
    readback_shapes.pop('expressions')
    if readback_shapes != read(protect(basis/'facial-shapes.json', fitted['sha256'])):
        raise ValueError('Cooked shapes differ from the fitted combined basis')
    for job in ('cook', 'package', 'readback'):
        if checked(face/(job+'-exit.json'))['exit_code'] != 0:
            raise ValueError('Incomplete facial cook or independent readback')
    receipt = checked(face/'probe/receipt.json')
    for group in ('protected', 'cooked', 'containers'):
        for path, checksum in receipt[group].items():
            protect(Path(path), checksum)
    for name, checksum in baseline['files'].items():
        protect(base/STEM/name, checksum)
    relative = Path(PACKAGE_ROOT)/'css.locomotion/manifest.json'
    original = checked(base/'metadata'/relative)
    manifest = copy.deepcopy(original)
    library = manifest['catalog']['animation_library']
    if len(library['entries']) != 63 or len(library['adapters']) != 5:
        raise ValueError('Base animation library changed')
    rig = next(r for r in library['faces'] if r['id'] == 'css.commanderwhite.face')
    old_rig = copy.deepcopy(rig)
    row = decoded['results'][0]
    if len(rig['adapters']) != 1 or row['source'] != rig['adapters'][0]['target_mesh'].split('.')[0]:
        raise ValueError('Combined face was not checked against the current shared face')
    catalog = checked(curve_index)
    if set(catalog) != {'spin', 'dumbdumb', 'cinderella', 'irisout', 'comecloser'}:
        raise ValueError('Require all five new performances')
    names = None
    performances = copy.deepcopy(rig.get('performances', []))
    if len(performances) != 1:
        raise ValueError('Expected the existing Gesture 200 performance')
    for name, folder in catalog.items():
        folder = Path(folder).resolve(strict=True)
        report = checked(folder/'report.json')
        validation = checked(folder/'native-validation.json')
        if not report.get('passed') or not validation.get('passed'):
            raise ValueError('Performance has not passed geometry and native playback checks')
        protect(folder/'native-fixture.json', validation['fixture_sha256'])
        performance = read(protect(folder/'performance.json', report['sha256']))
        if performance['clip'] != report['clip']:
            raise ValueError('Performance identity differs from its verification')
        basis_path = (basis/name/'basis.npz').resolve(strict=True)
        if str(basis_path) not in report['protected']:
            raise ValueError('Performance was not verified against this combined basis')
        with np.load(protect(basis_path, report['protected'][str(basis_path)])) as data:
            current = data['names'].tolist()
        if names is not None and current != names:
            raise ValueError('Performances do not use one combined face')
        names = current
        performances.append(performance)
    if len(set(names)) != len(names) or len(names) != fitted['total_shapes']:
        raise ValueError('Invalid combined control identities')
    used = {name for expression in rig['expressions'] for name in expression['weights']}
    used.update(curve['morph'] for performance in performances for curve in performance['curves'])
    if not used.issubset(names) or len({p['clip'] for p in performances}) != 6:
        raise ValueError('Missing expression or performance controls')
    rig['morphs'] = names
    rig['performances'] = performances
    rig['adapters'][0]['target_mesh'] = row['target']+'.'+row['target'].rsplit('/', 1)[-1]
    restored = copy.deepcopy(manifest)
    restored['catalog']['animation_library']['faces'][library['faces'].index(rig)] = old_rig
    if restored != original:
        raise ValueError('Unrelated animation data changed')
    payloads = {}
    old_package = row['source']
    removed = []
    for header in sorted((base/'stage').rglob('*.uasset')):
        for source in (header, header.with_suffix('.uexp')):
            if source.relative_to(base/'stage').as_posix() == (
                    'MortalShell2/Content/'+old_package.removeprefix('/Game/')+source.suffix):
                removed.append(source)
                continue
            payloads[source.relative_to(base/'stage')] = protect(source)
    if len(removed) != 2 or len(payloads) != 194:
        raise ValueError('Expected exactly 97 unchanged exports and one replaced face')
    for suffix in ('.uasset', '.uexp'):
        asset = row['target'].removeprefix('/Game/')+suffix
        source = face/'cooked/CSSAuthoring/Content'/asset
        key = Path('MortalShell2/Content')/asset
        if key in payloads:
            raise ValueError('Duplicate cooked export')
        payloads[key] = protect(source, receipt['cooked'][str(source)])
    output.mkdir(parents=True)
    for key, source in payloads.items():
        target = output/'stage'/key
        target.parent.mkdir(parents=True, exist_ok=True)
        try:
            target.hardlink_to(source)
        except OSError as error:
            if error.errno != errno.EXDEV:
                raise
            shutil.copy2(source, target)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, output)
    trio = output/STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', output/'stage', trio/(STEM+'.utoc'),
                  '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio/(STEM+'.utoc'))
    mounts = output/'containers'
    converter.base_containers(GAME, mounts)
    for path, checksum in baseline['dependencies'].items():
        source = protect(Path(path), checksum)
        (mounts/source.name).symlink_to(source)
    for source in trio.iterdir():
        (mounts/source.name).symlink_to(source)
    converter.run(converter.retoc, 'to-legacy', mounts, output/'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for key, source in payloads.items():
        result = output/'readback'/key
        if source.suffix == '.uexp':
            if sha(source) != sha(result):
                raise ValueError('Changed export: '+str(key))
        elif asset_info(source.read_bytes())['package'] != asset_info(result.read_bytes())['package']:
            raise ValueError('Changed package identity: '+str(key))
    manifest['containers'] = {suffix: dict(file=STEM+suffix, bytes=(trio/(STEM+suffix)).stat().st_size,
        sha256=sha(trio/(STEM+suffix))) for suffix in ('.utoc', '.ucas')}
    packed, resources = externalize(manifest)
    target = output/'metadata'/relative
    target.parent.mkdir(parents=True)
    target.write_bytes(encode(packed))
    for name, data in resources.items():
        (target.parent/name).write_bytes(data)
    converter.run(DEFAULT_REPAK, 'pack', output/'metadata', trio/(STEM+'.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio/(STEM+'.pak'), '--output', output/'metadata-readback')
    folder = output/'metadata-readback'/relative.parent
    if resolve(read(folder/'manifest.json'), {name: (folder/name).read_bytes() for name in resources}) != manifest:
        raise ValueError('Facial resources changed during packaging')
    for path in list(protected):
        protect(Path(path))
    result = dict(passed=True, entries=63, exports=98, unchanged_existing_exports=97, adapters=5,
                  commander_performances=6, eve_performances=6, commander_controls=len(names),
                  metadata_bytes=target.stat().st_size, protected=protected,
                  dependencies=baseline['dependencies'], files={p.name: sha(p) for p in trio.iterdir()},
                  installed=False, scope='Private cooked combined candidate. Full temporal review and live acceptance remain pending.')
    (output/'verification.json').write_text(json.dumps(result, indent=2)+'\n')
    print('CW_PERFORMANCE_LIBRARY', result['entries'], result['exports'], result['commander_performances'])


if __name__ == '__main__':
    main()
