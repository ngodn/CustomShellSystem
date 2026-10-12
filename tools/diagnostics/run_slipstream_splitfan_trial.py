"""Bind the repaired cutout-preserving proxy without copying an authoring project."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

import numpy as np

from run_slipstream_constraint_isolation import BASE, ROOT, sha
from run_slipstream_backstop_trial import ASSET, SLOT

OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-splitfans-native1'
PROXY = ROOT / 'work/three-outfit-rig-physics/slipstream-splitfans2'


def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('--resume-startup-failure', action='store_true')
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--backstop-radius', type=float, default=3.)
    args = parser.parse_args()
    OUT = args.output.absolute()
    assert OUT.parent.resolve() == (ROOT / 'work/three-outfit-rig-physics').resolve()
    assert args.backstop_radius in (3., 16.)
    if not args.resume_startup_failure:
        OUT.mkdir(exist_ok=False)
    content = BASE / 'editor/Content'
    meshes = content / 'CSS/eins0fx/EveSB/Slipstream/Meshes'
    protected = {str(p): sha(p) for p in content.rglob('*.uasset')}
    previous = json.loads((BASE / 'continuous-rebuild.exit.json').read_text())
    for name, digest in previous['module'].items():
        assert sha(BASE / 'editor' / name) == digest
    for name, digest in json.loads((PROXY / 'artifacts.json').read_text()).items():
        assert sha(PROXY / name) == digest
    topology = json.loads((PROXY / 'report.json').read_text())
    assert topology['after']['manifold_vertices'] and topology['unanchored_components'] == 0
    if args.resume_startup_failure:
        inputs = json.loads((OUT / 'inputs.json').read_text())
        failed = json.loads((OUT / 'rebuild.exit.json').read_text())
        assert failed['returncode'] == 139 and not failed['changed_source_assets']
        assert inputs['protected'] == protected and inputs['module'] == previous['module']
        assert sha(OUT / 'meshes/SK_SlipstreamGarments.uasset') == sha(meshes / 'SK_SlipstreamGarments.uasset')
        for name, digest in inputs['files'].items():
            assert sha(OUT / name) == digest
        assert not (OUT / 'rebuild-retry.log').exists()
        shutil.copy2(Path(__file__), OUT / 'retry-recipe.py')
    else:
        (OUT / 'meshes').mkdir()
        shutil.copy2(meshes / 'SK_SlipstreamGarments.uasset', OUT / 'meshes/SK_SlipstreamGarments.uasset')
        for name in ('proxy.json', 'proxy-morphs.json'):
            shutil.copy2(PROXY / name, OUT / name)
        proxy = json.loads((OUT / 'proxy.json').read_text())
        if args.backstop_radius != 3.:
            panel = proxy['slots'][SLOT]
            panel['backstop_radii'] = [args.backstop_radius] * len(panel['positions'])
            (OUT / 'proxy.json').write_text(json.dumps(proxy) + '\n')
        shutil.copy2(BASE / 'config.json', OUT / 'config.json')
        shutil.copy2(Path(__file__), OUT / 'recipe.py')
        (OUT / 'inputs.json').write_text(json.dumps(dict(protected=protected, module=previous['module'],
            engine=previous['engine'], recipe_sha256=sha(Path(__file__)),
            files={name: sha(OUT / name) for name in ('proxy.json', 'proxy-morphs.json', 'config.json')}), indent=2) + '\n')

    def run(name, flags):
        command = previous['command'][:]
        boundary = command.index('--')
        command[boundary:boundary] = ['--bind', str(OUT), str(OUT), '--ro-bind', str(content), str(content),
                                      '--bind', str(OUT / 'meshes'), str(meshes)]
        command = ['nice', '-n', '19'] + command[:command.index('-run=CSSEveCloth') + 1] + flags + ['-corelimit=2', '-NoCrashReports', '-notracethreading']
        with (OUT / (name + '.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        changed = [p for p, h in protected.items() if sha(Path(p)) != h]
        (OUT / (name + '.exit.json')).write_text(json.dumps(dict(command=command,
            returncode=result.returncode, changed_source_assets=changed), indent=2) + '\n')
        if result.returncode or changed:
            raise RuntimeError(f'{name} failed: {result.returncode}, {changed}')

    run('rebuild-retry' if args.resume_startup_failure else 'rebuild', ['-Rebuild', '-Mesh=' + ASSET,
        '-Physics=/Game/CSS/eins0fx/EveSB/Slipstream/Physics/PA_Cloth', '-Prefix=Slipstream', '-Slots=' + SLOT,
        '-Proxy=' + str(OUT / 'proxy.json'), '-Config=' + str(OUT / 'config.json')])
    run('inspect', ['-Inspect', '-Geometry', '-Morphs', '-Mesh=' + ASSET, '-Report=' + str(OUT / 'cloth-inspect.json')])
    before = json.loads((BASE / 'cloth-inspect.json').read_text())
    after = json.loads((OUT / 'cloth-inspect.json').read_text())
    for key in ('skeleton', 'mesh_physics', 'post_process', 'cloth_section_morphs', 'cloth_assets'):
        assert before[key] == after[key], key
    mapping_keys = {'mapping', 'normal_mappings', 'tangent_mappings'}
    for a, b in zip(before['render_geometry']['sections'], after['render_geometry']['sections'], strict=True):
        assert {k: v for k, v in a.items() if k not in mapping_keys} == {k: v for k, v in b.items() if k not in mapping_keys}
    cloth = after['assets'][0]
    proxy = json.loads((OUT / 'proxy.json').read_text())['slots'][SLOT]
    assert cloth['particles'] == len(proxy['positions']) == 667
    for field in ('max_distances', 'backstop_distances', 'backstop_radii', 'physical_normals'):
        assert np.max(np.abs(np.asarray(cloth[field]) - proxy['normals' if field == 'physical_normals' else field])) < 1e-6, field
    for field in ('physics', 'legacy_backstop', 'self_collision', 'self_collision_thickness_cm', 'iterations', 'max_iterations', 'substeps'):
        assert cloth[field] == before['assets'][0][field], field
    (OUT / 'saved-verification.json').write_text(json.dumps(dict(passed=True,
        render_geometry_weights_and_basis_preserved=True, morphs_preserved=True,
        cloth_particles=cloth['particles'], cloth_maps_preserved=True,
        candidate_mesh_sha256=sha(OUT / 'meshes/SK_SlipstreamGarments.uasset'),
        limitation='New render mapping needs independent reconstruction and motion checks.'), indent=2) + '\n')
    common = ('-Mesh=' + ASSET + ' -ClothIndex=0 -Frames=145 -FPS=60 -Secondary '
              '-Animation=/Game/CSS/eins0fx/Shared/Locomotion/EveLegacy/AN_S1_Sprint -Clip=Sprint')
    (OUT / 'batch.json').write_text(json.dumps([common + ' -Report=' + str(OUT / f'sprint-{i}.json') for i in (1, 2)], indent=2) + '\n')
    run('motion', ['-MotionBatch=' + str(OUT / 'batch.json')])
    print('Saved topology repair and two native motion captures:', OUT, flush=True)


if __name__ == '__main__':
    main()
