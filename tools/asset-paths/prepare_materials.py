"""Prepare and relocate preserved SeduXtress materials. Python 3.14.

Textures are mapped here and duplicated/cooked separately by the editor.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

CSS = Path(__file__).resolve().parents[2]
ROOT = CSS.parent
sys.path.insert(0, str(CSS / 'tools'))
from css_convert import digest
from css_paths import package_path, output_path, windows_destination

FOLDERS = ['Face','Head','Body','Legs','Arms','Genitals','Fingernails','Toenails',
    'Eyes','EyesB','EyeMoisture','Eyelids','Eyebrows','Eyes2','Mouth','PubicHair',
    'Outfit','Emissive','Trim','OutfitBB','Shoes','Hair','Tail','CoveredFeet',
    'CoveredNails','FootSkin','FootNails','FlatFoot','HeelFoot','Blade']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--windows-root', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(CSS / 'work') or out.exists():
        raise ValueError('Use a fresh short directory under CustomShellSystem/work')
    output_path(out)
    plan = json.loads(args.plan.read_text())
    mapping, jobs, protected, textures = {}, [], {}, {}
    for index, (material, folder) in enumerate(zip(plan['materials'], FOLDERS, strict=True)):
        if material['slot_index'] != index:
            raise ValueError('Unexpected material slot order')
        for old in [material['package']] + [t['package'] for t in material['textures'].values()]:
            new = '/Game/CSS/SeduXtress/' + folder + '/' + old.rsplit('/', 1)[1]
            package_path(new)
            if old in mapping and mapping[old] != new:
                raise ValueError('A shared reference needs an explicit shared destination')
            mapping[old] = new
            if old != material['package']:
                textures[old] = new
        target = out / 'legacy/MortalShell2/Content' / (mapping[material['package']].removeprefix('/Game/') + '.uasset')
        output_path(target)
        windows_destination(args.windows_root, str(target.relative_to(out / 'legacy')))
        for suffix, record in material['files'].items():
            file = Path(record['file']).resolve()
            if not file.is_relative_to(ROOT) or digest(file) != record['sha256']:
                raise ValueError(f'Material source changed: {file}')
            protected[str(file)] = record['sha256']
        jobs.append(dict(input=material['files']['.uasset']['file'], output=str(target)))
    if len(set(v.casefold() for v in mapping.values())) != len(mapping):
        raise ValueError('Destination collision')
    out.mkdir(parents=True)
    request = dict(mapping=mapping, jobs=jobs, report=str(out / 'report.json'))
    for name, value in [('request', request), ('map', mapping), ('textures', textures), ('protected', protected)]:
        (out / (name + '.json')).write_text(json.dumps(value, indent=2) + '\n')
    tool = CSS / 'build/retoc-css-target/release/examples/rewrite_material'
    command = [str(tool), str(out / 'request.json')]
    (out / 'run.json').write_text(json.dumps(command, indent=2) + '\n')
    with (out / 'run.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    (out / 'exit.json').write_text(json.dumps(dict(exit_code=result.returncode, tool_sha256=digest(tool))) + '\n')
    if result.returncode:
        raise RuntimeError('Material relocation failed; inspect run.log')
    if any(digest(Path(p)) != h for p, h in protected.items()):
        raise RuntimeError('Material source changed during relocation')


if __name__ == '__main__':
    main()
