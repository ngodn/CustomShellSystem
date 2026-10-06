"""Replace the six verified Eve faces while preserving the complete shared trial."""
import argparse
import copy
import json
from pathlib import Path
import shutil

from package_corrected_dance_trial import (
    ROOT, STEM, GAME, PACKAGE_ROOT, DEFAULT_REPAK, Converter, asset_info, read, sha,
)


def refresh_catalog(original, rig):
    result = copy.deepcopy(original)
    library = result['catalog']['animation_library']
    matches = [i for i, face in enumerate(library['faces']) if face['id'] == 'css.eve.face']
    if len(matches) != 1 or rig['id'] != 'css.eve.face':
        raise ValueError('Expected one existing Eve facial rig')
    index = matches[0]
    old = library['faces'][index]
    if (set(rig) != set(old) or not set(old['morphs']).issubset(rig['morphs']) or
            rig['expressions'] != old['expressions']):
        raise ValueError('Existing facial schema, shapes or expressions changed')
    previous = {entry['clip']: entry for entry in old['performances']}
    updated = {entry['clip']: entry for entry in rig['performances']}
    if len(updated) != len(rig['performances']) or any(updated.get(k) != v for k,v in previous.items()):
        raise ValueError('Existing facial performances changed')
    if set(updated)-set(previous) != {'css.eve.dance.dumbdumb', 'css.eve.dance.irisout',
                                      'css.eve.dance.jangwonyoungspin'}:
        raise ValueError('Unexpected added facial performance')
    body_ids = {entry['id'] for entry in library['entries']}
    if not set(updated).issubset(body_ids):
        raise ValueError('Facial performance has no body clip')
    by_source = lambda adapters: {a['source_mesh']: {k:v for k,v in a.items() if k != 'target_mesh'}
                                  for a in adapters}
    if (len(rig['adapters']) != 6 or len(by_source(rig['adapters'])) != 6 or
            by_source(rig['adapters']) != by_source(old['adapters'])):
        raise ValueError('Source outfit revision or skeleton binding changed')
    if len({a['target_mesh'] for a in rig['adapters']}) != 6:
        raise ValueError('Repeated replacement mesh')
    library['faces'][index] = copy.deepcopy(rig)
    check = copy.deepcopy(result)
    check['catalog']['animation_library']['faces'][index] = old
    if check != original:
        raise ValueError('Unrelated catalog data changed')
    return result, old


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    output = parser.parse_args().output.resolve()
    work = ROOT/'work/cine-animation-port'
    if output.exists() or not any(output.is_relative_to(parent) for parent in
            (work, Path('/mnt/eins0fxE/CSS-authoring-scratch'))):
        raise ValueError('Use new private animation storage')
    if shutil.disk_usage(output.parent).free < 3*1024**3:
        raise ValueError('Need 3 GiB for packaging and readback')
    base, face = work/'cw-face-trial-v1', work/'shared-faces-jali3-v1'
    protected = {}

    def protect(path, expected=None):
        actual = sha(path)
        if expected is not None and actual != expected:
            raise ValueError('Verified input changed: '+str(path))
        prior = protected.setdefault(str(path), actual)
        if prior != actual:
            raise ValueError('Input changed while reading: '+str(path))

    baseline = read(base/'verification.json')
    evidence = read(face/'facial-catalog-evidence.json')
    decoded = read(face/'cooked-readback-v1.json')
    rig = read(face/'facial-rig.json')
    if (not baseline.get('passed') or not evidence.get('passed') or
            not decoded.get('passed') or not decoded.get('source_deltas_checked') or
            len(decoded['results']) != 6 or len(rig['morphs']) != 115 or
            len(rig['expressions']) != 21 or len(rig['performances']) != 5):
        raise ValueError('Require the complete trial and verified 115-shape Eve cook')
    for filename in ('cook-exit.json','readback-exit.json'):
        if read(face/filename)['exit_code'] != 0:
            raise ValueError('Cook or readback did not complete successfully')
        protect(face/filename)
    for path in (base/'verification.json',face/'facial-catalog-evidence.json',
                 face/'cooked-readback-v1.json'):
        protect(path)
    protect(face/'facial-rig.json',evidence['rig_sha256'])
    for path, checksum in evidence['protected'].items():
        protect(Path(path),checksum)
    for name, checksum in baseline['files'].items():
        protect(base/STEM/name,checksum)
    manifest_path = base/'metadata'/PACKAGE_ROOT/'css.locomotion/manifest.json'
    protect(manifest_path)
    manifest, old = refresh_catalog(read(manifest_path),rig)
    library = manifest['catalog']['animation_library']
    if len(library['entries']) != 62 or len(library['adapters']) != 5 or len(library['faces']) != 2:
        raise ValueError('Base catalog dimensions changed')
    removed = {a['target_mesh'].split('.')[0] for a in old['adapters']}
    if len(removed) != 6 or any(p in json.dumps(manifest) for p in removed):
        raise ValueError('An old face remains referenced')
    adapters = {a['target_mesh'].split('.')[0]:a for a in rig['adapters']}
    dependencies = baseline['dependencies']
    payloads = {}; skipped = set()
    headers = sorted((base/'stage').rglob('*.uasset'))
    if len(headers) != 96:
        raise ValueError('Expected 96 existing exports')
    for header in headers:
        package = asset_info(header.read_bytes())['package']
        if package in removed:
            skipped.add(package)
            continue
        for source in (header,header.with_suffix('.uexp')):
            relative = source.relative_to(base/'stage')
            protect(source)
            roundtrip = base/'readback'/relative
            protect(roundtrip)
            if source.suffix == '.uexp' and sha(source) != sha(roundtrip):
                raise ValueError('Prior export differs from its verified readback')
            payloads[relative] = source
    if skipped != removed or len(payloads) != 180:
        raise ValueError('Replacement would omit an unrelated export')
    for row in decoded['results']:
        package = row['target']; adapter = adapters.get(package)
        if not row['passed'] or adapter is None or adapter['source_mesh'].split('.')[0] != row['source']:
            raise ValueError('Replacement does not match its cooked binding')
        matching = [Path(p) for p,h in dependencies.items() if p.endswith('.utoc') and
                    h == adapter['source_toc_sha256'] and
                    Path(p).with_suffix('.ucas').stat().st_size == adapter['source_bulk_bytes']]
        if len(matching) != 1:
            raise ValueError('Missing or ambiguous Eve dependency revision')
        for suffix in ('.uasset','.uexp'):
            relative = Path('MortalShell2/Content')/(package.removeprefix('/Game/')+suffix)
            source = face/'cooked/CSSAuthoring/Content'/(package.removeprefix('/Game/')+suffix)
            protect(source,evidence['protected'][str(source)])
            if relative in payloads:
                raise ValueError('Duplicate replacement package')
            payloads[relative] = source
    if len(payloads) != 192:
        raise ValueError('Incomplete replacement payloads')
    output.mkdir()
    for relative,source in payloads.items():
        target = output/'stage'/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc',DEFAULT_REPAK,output)
    trio = output/STEM; trio.mkdir()
    converter.run(converter.retoc,'to-zen',output/'stage',trio/(STEM+'.utoc'),'--version','UE5_6','--no-parallel')
    converter.run(converter.retoc,'verify',trio/(STEM+'.utoc'))
    mounts = output/'containers'; converter.base_containers(GAME,mounts)
    for source,checksum in dependencies.items():
        source = Path(source); protect(source,checksum)
        (mounts/source.name).symlink_to(source)
    for source in trio.iterdir():
        (mounts/source.name).symlink_to(source)
    converter.run(converter.retoc,'to-legacy',mounts,output/'readback','--version','UE5_6',
                  '--no-parallel','--no-shaders','-f','/CSS/Locomotion/')
    for relative,source in payloads.items():
        target = output/'readback'/relative
        if source.suffix == '.uexp':
            if sha(source) != sha(target):
                raise ValueError('Container round-trip changed an export')
        elif asset_info(source.read_bytes())['package'] != asset_info(target.read_bytes())['package']:
            raise ValueError('Package identity changed')
    manifest['containers'] = {suffix:dict(file=STEM+suffix,bytes=(trio/(STEM+suffix)).stat().st_size,
        sha256=sha(trio/(STEM+suffix))) for suffix in ('.utoc','.ucas')}
    path = output/'metadata'/PACKAGE_ROOT/'css.locomotion/manifest.json'
    metadata = json.dumps(manifest,separators=(',',':'))+'\n'
    if len(metadata.encode('utf-8')) > 256*1024:
        raise ValueError('Facial metadata exceeds the existing CSS manifest limit')
    path.parent.mkdir(parents=True); path.write_text(metadata)
    converter.run(DEFAULT_REPAK,'pack',output/'metadata',trio/(STEM+'.pak'),'--version','V8B')
    converter.run(DEFAULT_REPAK,'unpack',trio/(STEM+'.pak'),'--output',output/'metadata-readback')
    if read(output/'metadata-readback'/PACKAGE_ROOT/'css.locomotion/manifest.json') != manifest:
        raise ValueError('Catalog round-trip changed')
    if any(sha(Path(path)) != checksum for path,checksum in protected.items()):
        raise ValueError('A protected input changed during packaging')
    report = dict(passed=True,entries=62,exports=96,native_candidates=4,adapters=5,facial_rigs=2,
        unchanged_existing_exports=90,replaced_facial_meshes=6,eve_performances=5,
        protected=protected,dependencies=dependencies,files={p.name:sha(p) for p in trio.iterdir()},
        installed=False,scope='Combined private candidate. Full facial motion and live acceptance remain.')
    (output/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('EVE_FACIAL_UPDATE: 62 identities, 96 exports, five Eve facial performances')


if __name__ == '__main__':
    main()
