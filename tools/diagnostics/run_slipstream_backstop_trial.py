"""Test backstop radius or surface normals using a private 9 MB mesh overlay."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

import numpy as np

from run_slipstream_constraint_isolation import BASE, ROOT, EVE, sha

OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-backstop16'
SLOT = 'Slipstream_skirt_Fabric'
ASSET = '/Game/CSS/eins0fx/EveSB/Slipstream/Meshes/SK_SlipstreamGarments'


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-inspection', action='store_true')
    parser.add_argument('--surface-normals', action='store_true')
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args()
    OUT = args.output.resolve()
    if OUT.parent != (ROOT / 'work/three-outfit-rig-physics').resolve():
        raise ValueError('Use a private goal work directory')
    if not args.resume_inspection:
        OUT.mkdir(exist_ok=False)
    content = BASE / 'editor/Content'
    meshes = content / 'CSS/eins0fx/EveSB/Slipstream/Meshes'
    reference = ROOT / 'work/three-outfit-rig-physics/slipstream-backstop16' if args.surface_normals else BASE
    source_meshes = reference / 'meshes' if args.surface_normals else meshes
    protected = {str(p): sha(p) for p in content.rglob('*.uasset')}
    protected.update({str(p): sha(p) for p in source_meshes.glob('*.uasset')})
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
        for path in source_meshes.iterdir():
            if not path.is_file():
                raise ValueError('Unexpected mesh directory entry')
            shutil.copy2(path, OUT / 'meshes' / path.name)
    source_proxy = reference / 'proxy.json' if args.surface_normals else EVE / 'work/slipstream-cloth-continuous2/proxy.json'
    proxy = json.loads(source_proxy.read_text())
    if args.surface_normals:
        panel = proxy['slots'][SLOT]
        vertices = np.asarray(panel['positions'])
        triangles = np.asarray(panel['indices']).reshape(-1, 3)
        face_normals = np.cross(vertices[triangles[:, 1]] - vertices[triangles[:, 0]],
                                vertices[triangles[:, 2]] - vertices[triangles[:, 0]])
        lengths = np.linalg.norm(face_normals, axis=1)
        assert np.all(lengths > 1e-8)
        face_normals /= lengths[:, None]
        normals = np.zeros_like(vertices)
        for column in range(3):
            np.add.at(normals, triangles[:, column], face_normals)
        lengths = np.linalg.norm(normals, axis=1)
        assert np.all(lengths > 1e-8)
        panel['normals'] = (normals / lengths[:, None]).tolist()
    else:
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
                   change=('Only physical backstop normals from averaged unit faces of continuous skirt'
                           if args.surface_normals else 'Only backstop radius 3 to 16 cm'),
                   reference_inspection=str(reference / 'cloth-inspect.json'))
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
    before = json.loads((reference / 'cloth-inspect.json').read_text())
    after = json.loads((OUT / 'cloth-inspect.json').read_text())
    for key in ('render_geometry', 'skeleton', 'mesh_physics', 'post_process', 'cloth_section_morphs'):
        if before[key] != after[key]:
            raise ValueError('Unexpected saved change: ' + key)
    old_cloth, new_cloth = before['assets'][0], after['assets'][0]
    changed = [k for k in old_cloth if old_cloth[k] != new_cloth[k]]
    expected = ['physical_normals'] if args.surface_normals else ['backstop_radii']
    if changed != expected or set(new_cloth['backstop_radii']) != {16.}:
        raise ValueError('Unexpected cloth changes: ' + repr(changed))
    assert np.max(np.abs(np.asarray(new_cloth['physical_normals']) -
                         proxy['slots'][SLOT]['normals'])) < 1e-6
    (OUT / 'saved-verification.json').write_text(json.dumps(dict(changed_cloth_fields=changed,
        original_mesh_sha256=protected[str(meshes / 'SK_SlipstreamGarments.uasset')],
        candidate_mesh_sha256=sha(OUT / 'meshes/SK_SlipstreamGarments.uasset'),
        render_geometry_preserved=True), indent=2) + '\n')
    common = ('-Mesh=' + ASSET + ' -ClothIndex=0 -Frames=145 -FPS=60 -Secondary '
              '-Animation=/Game/CSS/eins0fx/Shared/Locomotion/EveLegacy/AN_S1_Sprint -Clip=Sprint')
    batch = [common + ' -Report=' + str(OUT / f'sprint-{i}.json') for i in (1, 2)]
    (OUT / 'batch.json').write_text(json.dumps(batch, indent=2) + '\n')
    run('motion', ['-MotionBatch=' + str(OUT / 'batch.json')])
    print('Saved backstop change and two native captures completed:', OUT, flush=True)


if __name__ == '__main__':
    main()
