"""Add verified CW jaw curves while keeping every cooked shared asset unchanged."""
import argparse
import copy
import json
from pathlib import Path
import subprocess

from package_corrected_dance_trial import ROOT, STEM, PACKAGE_ROOT, DEFAULT_REPAK, read, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    work = ROOT/'work/cine-animation-port'
    if output.exists() or not any(output.is_relative_to(parent) for parent in
            (work,Path('/mnt/eins0fxE/CSS-authoring-scratch'))):
        raise ValueError('Use a new private animation workspace')
    base = work/'cinderella-combined-v2'
    prepared = work/'cw-gesture200-curves-v2'
    report = read(base/'verification.json')
    check = read(prepared/'verification.json')
    if not report.get('passed') or report['exports']!=98 or not check.get('passed'):
        raise ValueError('Require the verified C11 pack and CW curve mapping')
    protected = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        checksum = sha(path)
        if expected is not None and checksum != expected:
            raise ValueError('Verified input changed: '+str(path))
        if protected.setdefault(str(path),checksum) != checksum:
            raise ValueError('Input changed during packaging')

    for path in (base/'verification.json',prepared/'verification.json',prepared/'performance.json',
                 prepared/'candidate-manifest.json',prepared/'native-fixture.json'):
        protect(path)
    for path,checksum in check['inputs'].items():
        protect(Path(path),checksum)
    relative = Path(PACKAGE_ROOT)/'css.locomotion/manifest.json'
    original = read(base/'metadata'/relative)
    manifest = read(prepared/'candidate-manifest.json')
    restored = copy.deepcopy(manifest)
    rig = next(r for r in restored['catalog']['animation_library']['faces'] if r['id']=='css.commanderwhite.face')
    if rig.pop('performances') != [read(prepared/'performance.json')]:
        raise ValueError('Prepared CW performance differs from the candidate')
    # The existing static rig may omit the optional field or carry an empty list.
    baseline_rig = next(r for r in original['catalog']['animation_library']['faces'] if r['id']=='css.commanderwhite.face')
    if 'performances' in baseline_rig:
        rig['performances'] = []
    if restored != original:
        raise ValueError('Unrelated catalog data changed')
    serialized = json.dumps(manifest,separators=(',',':'),allow_nan=False)+'\n'
    if len(serialized.encode())>256*1024:
        raise ValueError('Catalog exceeds the unchanged 256 KiB limit')
    for name,checksum in report['files'].items():
        protect(base/STEM/name,checksum)
    output.mkdir(parents=True)
    trio = output/STEM
    trio.mkdir()
    for suffix in ('.utoc','.ucas'):
        (trio/(STEM+suffix)).hardlink_to((base/STEM/(STEM+suffix)).resolve())
    target = output/'metadata'/relative
    target.parent.mkdir(parents=True)
    target.write_text(serialized)
    commands = [[str(DEFAULT_REPAK),'pack',str(output/'metadata'),str(trio/(STEM+'.pak')),'--version','V8B'],
                [str(DEFAULT_REPAK),'unpack',str(trio/(STEM+'.pak')),'--output',str(output/'metadata-readback')]]
    (output/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    for command in commands:
        subprocess.run(command,check=True)
    if read(output/'metadata-readback'/relative)!=manifest:
        raise ValueError('Packaged facial catalog round-trip differs')
    for path in list(protected):
        protect(Path(path))
    for name in ('stage','readback'):
        (output/name).symlink_to((base/name).resolve())
    result = dict(passed=True,entries=63,exports=98,unchanged_cooked_exports=98,
        base_verification=str((base/'verification.json').resolve()),metadata_bytes=len(serialized.encode()),
        commander_performances=1,eve_performances=6,protected=protected,dependencies=report['dependencies'],
        files={p.name:sha(p) for p in trio.iterdir()},installed=False,
        scope='Private metadata-only candidate. Cooked payloads are identical to the verified C11 pack; native catalog and live acceptance are separate.')
    (output/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print('CW_JAW_TRIAL',result['metadata_bytes'],'metadata bytes; all 98 cooked exports unchanged')


if __name__ == '__main__':
    main()
