"""Test one saved backstop-radius change using a private 9 MB mesh overlay."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from run_slipstream_constraint_isolation import BASE, ROOT, EVE, sha

OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-backstop16'
SLOT = 'Slipstream_skirt_Fabric'
ASSET = '/Game/CSS/eins0fx/EveSB/Slipstream/Meshes/SK_SlipstreamGarments'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-inspection', action='store_true')
    args = parser.parse_args()
    if not args.resume_inspection:
        OUT.mkdir(exist_ok=False)
    content = BASE / 'editor/Content'
    meshes = content / 'CSS/eins0fx/EveSB/Slipstream/Meshes'
    protected = {str(p): sha(p) for p in content.rglob('*.uasset')}
    previous = json.loads((BASE / 'continuous-rebuild.exit.json').read_text())
    for name, digest in previous['module'].items():
        if sha(BASE / 'editor' / name) != digest:
            raise ValueError('Retained module changed: ' + name)
    if args.resume_inspection:
        saved = json.loads((OUT / 'inputs.json').read_text())
        assert saved['protected'] == protected and saved['module'] == previous['module']
        assert json.loads((OUT / 'rebuild.exit.json').read_text())['returncode'] == 0
        assert not (OUT / 'motion.exit.json').exists()
    else:
        (OUT / 'meshes').mkdir()
        for path in meshes.iterdir():
            if not path.is_file():
                raise ValueError('Unexpected mesh directory entry')
            shutil.copy2(path, OUT / 'meshes' / path.name)
    source_proxy = EVE / 'work/slipstream-cloth-continuous2/proxy.json'
    proxy = json.loads(source_proxy.read_text())
    proxy['slots'][SLOT]['backstop_radii'] = [16.] * len(proxy['slots'][SLOT]['positions'])
    if args.resume_inspection:
        assert json.loads((OUT / 'proxy.json').read_text()) == proxy
        shutil.copy2(OUT / 'recipe.py', OUT / 'recipe-before-inspection-fix.py')
    else:
        (OUT / 'proxy.json').write_text(json.dumps(proxy) + '\n')
    shutil.copy2(Path(__file__), OUT / 'recipe.py')
    receipt = dict(protected=protected, module=previous['module'], engine=previous['engine'],
                   source_proxy=str(source_proxy), source_proxy_sha256=sha(source_proxy),
                   proxy_sha256=sha(OUT / 'proxy.json'), recipe_sha256=sha(Path(__file__)),
                   change='Only backstop radius 3 to 16 cm; original mesh directory overlaid privately.')
    (OUT / ('resume-inputs.json' if args.resume_inspection else 'inputs.json')).write_text(json.dumps(receipt, indent=2) + '\n')

    def run(name, arguments):
        command = previous['command'][:]
        boundary = command.index('--')
        command[boundary:boundary] = ['--bind', str(OUT), str(OUT),
            '--ro-bind', str(content), str(content), '--bind', str(OUT / 'meshes'), str(meshes)]
        command = command[:command.index('-run=CSSEveCloth') + 1] + arguments
        command = ['nice', '-n', '19'] + command + ['-corelimit=2', '-NoCrashReports']
        with (OUT / (name + '.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        changed = [p for p, digest in protected.items() if sha(Path(p)) != digest]
        (OUT / (name + '.exit.json')).write_text(json.dumps(dict(command=command,
            returncode=result.returncode, changed_source_assets=changed), indent=2) + '\n')
        if changed or result.returncode:
            raise RuntimeError(f'{name}: exit {result.returncode}, changed sources {changed}')

    if not args.resume_inspection:
        run('rebuild', ['-Rebuild', '-Mesh=' + ASSET,
            '-Physics=/Game/CSS/eins0fx/EveSB/Slipstream/Physics/PA_Cloth', '-Prefix=Slipstream',
            '-Slots=' + SLOT, '-Proxy=' + str(OUT / 'proxy.json'), '-Config=' + str(BASE / 'config.json')])
    run('inspect-full', ['-Inspect', '-Geometry', '-Morphs', '-Mesh=' + ASSET,
        '-Report=' + str(OUT / 'cloth-inspect.json')])
    before = json.loads((BASE / 'cloth-inspect.json').read_text())
    after = json.loads((OUT / 'cloth-inspect.json').read_text())
    for key in ('render_geometry', 'skeleton', 'mesh_physics', 'post_process', 'cloth_section_morphs'):
        if before[key] != after[key]:
            raise ValueError('Unexpected saved change: ' + key)
    old_cloth, new_cloth = before['assets'][0], after['assets'][0]
    changed = [k for k in old_cloth if old_cloth[k] != new_cloth[k]]
    if changed != ['backstop_radii'] or set(new_cloth['backstop_radii']) != {16.}:
        raise ValueError('Unexpected cloth changes: ' + repr(changed))
    (OUT / 'saved-verification.json').write_text(json.dumps(dict(changed_cloth_fields=changed,
        original_mesh_sha256=protected[str(meshes / 'SK_SlipstreamGarments.uasset')],
        candidate_mesh_sha256=sha(OUT / 'meshes/SK_SlipstreamGarments.uasset'),
        render_geometry_preserved=True), indent=2) + '\n')
    common = ('-Mesh=' + ASSET + ' -ClothIndex=0 -Frames=145 -FPS=60 -Secondary '
              '-Animation=/Game/CSS/eins0fx/Shared/Locomotion/EveLegacy/AN_S1_Sprint -Clip=Sprint')
    batch = [common + ' -Report=' + str(OUT / f'sprint-{i}.json') for i in (1, 2)]
    (OUT / 'batch.json').write_text(json.dumps(batch, indent=2) + '\n')
    run('motion', ['-MotionBatch=' + str(OUT / 'batch.json')])
    print('Saved radius-only change and two native captures completed:', OUT, flush=True)


if __name__ == '__main__':
    main()
