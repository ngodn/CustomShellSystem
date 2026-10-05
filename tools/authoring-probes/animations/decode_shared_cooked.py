"""Decode each packaged sequence with the independent native ACL reader."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--exporter', type=Path, required=True)
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--mappings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(exist_ok=False)
    environment = {k: v for k, v in os.environ.items() if not k.startswith('CSS_')}
    environment['CSS_ABSOLUTE_TRACKS'] = '1'
    entries = json.loads((work / 'catalog.json').read_text())['animation_library']['entries']
    results = []
    report = output / 'decoded.json'
    report.write_text(json.dumps(dict(complete=False, results=[])) + '\n')
    for index, entry in enumerate(entries):
        destination = output / f'{index:02d}'
        command = [str(args.dotnet.resolve(strict=True)), str(args.exporter.resolve(strict=True)),
                   str(work / 'pack/containers'), str(args.mappings.resolve(strict=True)),
                   entry['assets'][0]['asset'], str(destination)]
        with (output / f'{index:02d}.log').open('w') as log:
            subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT, check=True)
        data = json.loads((destination / 'absolute-tracks.json').read_text())
        if data['schema'] != 2:
            raise ValueError('Decoder must report ACL wrap optimization explicitly')
        results.append(dict(id=entry['id'], **{key: data[key] for key in
                       ('sampleCount', 'logicalSamples', 'wrapOptimized', 'sampleRate', 'trackCount', 'virtualTrackCount')}))
        report.write_text(json.dumps(dict(complete=False, results=results), indent=2) + '\n')
    report.write_text(json.dumps(dict(complete=True, results=results), indent=2) + '\n')
    print(f'Decoded {len(results)} complete sequences')


if __name__ == '__main__':
    main()
