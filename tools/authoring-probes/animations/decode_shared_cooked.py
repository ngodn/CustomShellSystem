"""Decode each packaged sequence with the independent native ACL reader."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--mappings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--pack-name', default='pack', help='Verified package output label within the workspace')
    parser.add_argument('--all-frames', action='store_true', help='Also write every ACL sample as compact binary transforms')
    parser.add_argument('--runtime-sampling', action='store_true', help='Use linear ACL interpolation, including stripped keys (requires the native study helper)')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z0-9-]{1,48}', args.pack_name):
        parser.error('--pack-name must be a short workspace label')
    if args.runtime_sampling and not args.all_frames:
        parser.error('--runtime-sampling requires --all-frames')
    work = args.work.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(exist_ok=False)
    environment = {k: v for k, v in os.environ.items() if not k.startswith('CSS_')}
    environment['CSS_ABSOLUTE_TRACKS'] = '1'
    if args.all_frames:
        environment['CSS_ABSOLUTE_FULL'] = '1'
    if args.runtime_sampling:
        environment['CSS_ABSOLUTE_RUNTIME'] = '1'
    entries = json.loads((work / 'catalog.json').read_text())['animation_library']['entries']
    results = []
    report = output / 'decoded.json'
    report.write_text(json.dumps(dict(complete=False, results=[])) + '\n')
    for index, entry in enumerate(entries):
        destination = output / f'{index:02d}'
        command = [str(args.dotnet.resolve(strict=True)), str(args.exporter.resolve(strict=True)),
                   str(work / args.pack_name / 'containers'), str(args.mappings.resolve(strict=True)),
                   entry['assets'][0]['asset'], str(destination)]
        with (output / f'{index:02d}.log').open('w') as log:
            subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
        data = json.loads((destination / 'absolute-tracks.json').read_text())
        if data['schema'] != 2:
            raise ValueError('Decoder must report ACL wrap optimization explicitly')
        if args.all_frames and not (destination / 'absolute-tracks-full.json').is_file():
            raise ValueError('Decoder did not produce the requested full sample export')
        results.append(dict(id=entry['id'], **{key: data[key] for key in
                       ('sampleCount', 'logicalSamples', 'wrapOptimized', 'sampleRate', 'trackCount', 'virtualTrackCount')}))
        report.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
    report.write_text(json.dumps(dict(complete=True, results=results), indent=2) + '\n')
    print(f'Decoded {len(results)} complete sequences')


if __name__ == '__main__':
    main()
