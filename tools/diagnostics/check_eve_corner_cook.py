"""Roundtrip a private Eve corner-order cook and compare it with the accepted import."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if not work.is_relative_to((ROOT / 'work').resolve()):
        raise ValueError('Use a private CSS work directory')
    result = json.loads((work / 'cook/exit.json').read_text())
    if result != dict(returncode=0, changed_inputs=[]):
        raise ValueError('Cook did not finish cleanly')
    proof = json.loads((work / 'verified.json').read_text())
    receipt = json.loads((work / 'candidate.mesh.receipt.json').read_text())
    source = Path(receipt['source'])
    if sha(source) != receipt['source_sha256']:
        raise ValueError('Accepted import changed')
    out = work / 'pack'
    out.mkdir(exist_ok=False)
    content = work / 'cook/project/Saved/Cooked/Windows/CSSShared/Content'
    stage, payloads = out / 'stage', {}
    packages = [receipt['mesh_package'], receipt['skeleton_package']]
    for package in packages:
        relative = Path(package.removeprefix('/Game/') + '.uasset')
        header = content / relative
        if asset_info(header.read_bytes())['package'] != package or not header.with_suffix('.uexp').exists():
            raise ValueError('Unexpected cooked package')
        for file in header.parent.glob(header.stem+'.*'):
            if file.suffix not in ('.uasset', '.uexp', '.ubulk', '.uptnl'):
                raise ValueError('Unexpected cooked payload')
            relative = Path('MortalShell2/Content') / file.relative_to(content)
            destination = stage / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, destination)
            payloads[str(relative)] = sha(file)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    container = out / 'container'
    container.mkdir()
    toc = container / 'CSS_EveSurfaceTrial_P.utoc'
    converter.run(converter.retoc, 'to-zen', stage, toc, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', toc)
    mount = out / 'containers'
    converter.base_containers(GAME, mount)
    for path in container.iterdir():
        (mount/path.name).symlink_to(path)
    filters = [value for package in packages for value in ('-f', package.removeprefix('/Game'))]
    converter.run(converter.retoc, 'to-legacy', mount, out/'roundtrip', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', *filters)
    for relative, digest in payloads.items():
        path = out / 'roundtrip' / relative
        if path.suffix == '.uasset':
            if asset_info(path.read_bytes())['package'] != asset_info((stage/relative).read_bytes())['package']:
                raise ValueError('Roundtrip changed package identity')
        elif sha(path) != digest:
            raise ValueError('Roundtrip changed export or bulk payload')
    baseline = ROOT/'work/three-outfit-rig-physics/eve-surface-20261014/black_pearl/command.json'
    old_command = json.loads(baseline.read_text())['command']
    dotnet, reader, mappings = old_command[0], old_command[1], old_command[3]
    pinned = {str(source): sha(source), str(Path(__file__)): sha(Path(__file__)),
              reader: sha(Path(reader)), mappings: sha(Path(mappings)),
              str(converter.retoc): sha(converter.retoc)}
    for path in container.iterdir():
        pinned[str(path)] = sha(path)
    (out/'inputs.json').write_text(json.dumps(pinned, indent=2)+'\n')
    (out/'roundtrip.json').write_text(json.dumps(dict(passed=True, packages=packages, cooked_payloads=payloads), indent=2)+'\n')
    mesh = receipt['mesh_package']
    object_path = mesh+'.'+mesh.rsplit('/', 1)[1]
    commands = []
    for mode in ('skin', 'morph'):
        env = {'CSS_SKIN_AUDIT':'1', 'CSS_SKIN_SURFACE':'1'} if mode == 'skin' else {'CSS_MORPH_AUDIT':'1'}
        commands.append((mode, [dotnet, reader, str(mount), mappings, object_path, str(out/mode)], env))
    commands.extend([
        ('geometry', [sys.executable, '-B', str(ROOT/'tools/diagnostics/check_import_cooked_mesh.py'),
            '--source', str(source), '--skin', str(out/'skin'), '--morph', str(out/'morph'), '--output', str(out/'geometry.json')], {}),
        ('surface', [sys.executable, '-B', str(ROOT/'tools/diagnostics/check_import_cooked_surface.py'),
            '--source', str(source), '--readback', str(out/'skin'), '--output', str(out/'surface.json')], {}),
        ('tangents', [sys.executable, '-B', str(ROOT/'tools/diagnostics/measure_cooked_tangent_basis.py'),
            '--readback', str(out/'skin'), '--output', str(out/'tangents.json')], {})])
    results = []
    for mode, command, overrides in commands:
        (out/(mode+'-command.json')).write_text(json.dumps(dict(command=command, environment=overrides), indent=2)+'\n')
        with (out/(mode+'.log')).open('w') as log:
            proc = subprocess.run(command, env={**os.environ, **overrides}, stdout=log, stderr=subprocess.STDOUT)
        row = dict(mode=mode, returncode=proc.returncode)
        results.append(row)
        (out/(mode+'-exit.json')).write_text(json.dumps(row)+'\n')
        print(row, flush=True)
    changed = [p for p, digest in pinned.items() if sha(Path(p)) != digest]
    passed = all(r['returncode'] == 0 for r in results) and not changed
    (out/'verified.json').write_text(json.dumps(dict(passed=passed, results=results, changed_inputs=changed,
        expected_editor_vertices=proof['lod']['vertices'],
        scope='Standalone Windows cook/IoStore/readback and accepted-import geometry/surface correspondence. Tangent observations are not full basis or visual acceptance; no assembly bindings or game installation.'), indent=2)+'\n')
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
