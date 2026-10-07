"""Repackage an accepted inline facial catalog without changing cooked assets or installing it."""
import argparse
import json
from pathlib import Path
import subprocess

from package_corrected_dance_trial import ROOT, STEM, PACKAGE_ROOT, DEFAULT_REPAK, read, sha
from css_facial_resources import encode, externalize, resolve


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    base = args.base.resolve(strict=True)
    output = args.output.resolve()
    if output.exists() or not any(output.is_relative_to(parent) for parent in
            (ROOT/'work/cine-animation-port', Path('/mnt/eins0fxE/CSS-authoring-scratch'))):
        raise ValueError('Use a new private animation workspace')
    report = read(base/'verification.json')
    if report.get('passed') is not True:
        raise ValueError('Base candidate has not passed packaging checks')
    relative = Path(PACKAGE_ROOT)/'css.locomotion/manifest.json'
    original = read(base/'metadata'/relative)
    packed, resources = externalize(original)
    protected = {str(base/'verification.json'): sha(base/'verification.json'),
                 str(base/'metadata'/relative): sha(base/'metadata'/relative)}
    if set(report['files']) != {STEM+suffix for suffix in ('.pak', '.utoc', '.ucas')}:
        raise ValueError('Expected exactly one verified shared-asset trio')
    for name, checksum in report['files'].items():
        path = base/STEM/name
        if sha(path) != checksum:
            raise ValueError('Verified package changed: '+str(path))
        protected[str(path)] = checksum
    output.mkdir(parents=True)
    subprocess.run([str(DEFAULT_REPAK), 'unpack', str(base/STEM/(STEM+'.pak')),
                    '--output', str(output/'base-metadata')], check=True)
    if read(output/'base-metadata'/relative) != original:
        raise ValueError('Base metadata does not match its verified package')
    trio = output/STEM
    trio.mkdir()
    for suffix in ('.utoc', '.ucas'):
        (trio/(STEM+suffix)).hardlink_to(base/STEM/(STEM+suffix))
    target = output/'metadata'/relative
    target.parent.mkdir(parents=True)
    target.write_bytes(encode(packed))
    for name, data in resources.items():
        (target.parent/name).write_bytes(data)
    commands = [[str(DEFAULT_REPAK), 'pack', str(output/'metadata'), str(trio/(STEM+'.pak')), '--version', 'V8B'],
                [str(DEFAULT_REPAK), 'unpack', str(trio/(STEM+'.pak')), '--output', str(output/'metadata-readback')]]
    (output/'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
    for command in commands:
        subprocess.run(command, check=True)
    folder = output/'metadata-readback'/relative.parent
    decoded = read(folder/'manifest.json')
    if decoded != packed or resolve(decoded, {name: (folder/name).read_bytes() for name in resources}) != original:
        raise ValueError('External resource round-trip changed the catalog')
    if any(sha(Path(path)) != checksum for path, checksum in protected.items()):
        raise ValueError('Source candidate changed during verification')
    library = original['catalog']['animation_library']
    result = dict(passed=True, entries=len(library['entries']), exports=report['exports'],
                  unchanged_cooked_exports=report['exports'], base_verification=str(base/'verification.json'),
                  metadata_bytes=target.stat().st_size, facial_resources=packed['facial_resources'],
                  facial_performances={rig['id']: len(rig.get('performances', [])) for rig in library['faces']},
                  protected=protected, dependencies=report['dependencies'],
                  files={path.name: sha(path) for path in trio.iterdir()}, installed=False,
                  scope='Private metadata migration only. No new facial shapes, cooked motion or live acceptance.')
    (output/'verification.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({key: result[key] for key in ('entries', 'exports', 'metadata_bytes', 'facial_performances')}, indent=2))


if __name__ == '__main__':
    main()
