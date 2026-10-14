"""Pin retained sources for the combined Eve repair without recooking native proxies.

Python 3.14. Mesh cook provenance and the older material preservation chain are
separate from runtime acceptance and from the unchanged Peony source recipes.
"""
import argparse
import json
from pathlib import Path

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'work/three-outfit-rig-physics'
SURFACE = WORK/'eve-surface-20261014'
SETS = ('blackpearl-final', 'eve-tangent-final', 'planet-cloth-final',
        'knit-cloth-final', 'alice-cloth-final')


def read(path):
    return json.loads(path.read_text())


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'Output already exists')
    out = args.candidate.resolve(strict=True)
    inputs = {}

    def pin(path, expected=None):
        path = Path(path).resolve(strict=True)
        key = str(path)
        if key not in inputs:
            inputs[key] = sha(path)
        require(expected is None or inputs[key] == expected, 'Source hash differs: '+key)
        return dict(path=key, sha256=inputs[key])

    def load(path, expected=None):
        pin(path, expected)
        return read(path)

    plan = load(out/'plan.json')
    proof = load(out/'verified.json')
    require(proof['passed'] and proof['plan_sha256'] == sha(out/'plan.json'), 'Candidate is not verified')
    pin(Path(__file__))
    pin(Path(__file__).with_name('check_import_cooked_mesh.py'))
    for relative, row in plan['payloads'].items():
        pin(row['path'], row['sha256'])
    meshes = []
    dependencies = {}
    for name in SETS:
        base = SURFACE/name
        final_plan = load(base/'plan.json')
        reloaded = load(base/'reload.json')
        cooked = load(base/'cook/inputs.json')
        exit_record = load(base/'cook/exit.json')
        packaged = load(base/'pack/verified.json', plan['protected'][str(base/'pack/verified.json')])
        allowlist = load(base/'cook/shipping-allowlist.json')
        command = load(base/'cook/command.json')
        require(reloaded['passed'] and packaged['passed'], 'Final asset was not verified')
        require(exit_record == dict(returncode=0, changed_inputs=[]), 'Final cook failed')
        require(set(allowlist) == set(reloaded['outputs']) == set(final_plan['mapping'].values()),
                'Cook allowlist differs from final meshes')
        # Only these editable meshes ship from this cook. Material/rig dependency
        # proxies in its project are deliberately excluded from the allowlist.
        recipes = []
        for path, digest in cooked.items():
            if path.endswith(('.py', '.sh', '.ini', '.uproject', 'Build.version')):
                recipes.append(pin(path, digest))
        for package, row in final_plan['assets'].items():
            entry = pin(row['path'], row['sha256'])
            dependencies.setdefault(package, {})[entry['path']] = entry['sha256']
        for package, digest in reloaded['outputs'].items():
            source = base/'editor/Content'/(package.removeprefix('/Game/')+'.uasset')
            source_row = pin(source, digest)
            matches = [p for p, h in cooked.items() if h == digest and Path(p).resolve() == source.resolve()]
            require(matches, 'Saved editor mesh is absent from cook inputs: '+package)
            prefix = 'MortalShell2/Content/'+package.removeprefix('/Game/')
            payloads = {prefix+suffix: packaged['payloads'][prefix+suffix] for suffix in ('.uasset', '.uexp')}
            for relative, wanted in payloads.items():
                require(plan['payloads'][relative]['sha256'] == wanted, 'Combined payload differs from final cook')
            meshes.append(dict(package=package, editor_source=source_row,
                verification=str((base/'pack/verified.json').resolve()), cook_command=command,
                cook_recipes=recipes, shipping_payloads=payloads,
                direct_dependencies=reloaded['dependencies'][package]))
    require(len(meshes) == 12 and {r['package'] for r in meshes} == set(plan['expected_meshes']),
            'Incomplete repaired mesh provenance')

    material_proof_path = WORK/'eve-material-pack1/verification.json'
    rig_proof = load(WORK/'eve-rig-pack2/verification.json')
    materials = load(material_proof_path, rig_proof['protected'][str(material_proof_path)])
    require(materials['passed'] and materials['packages'] == 273, 'Unexpected original material set')
    for p, h in materials['evidence'].items():
        pin(p, h)
    for relative, digest in materials['payloads'].items():
        require(plan['payloads'][relative]['sha256'] == digest, 'Native material/texture payload changed: '+relative)
    hair = load(WORK/'eve-hair-cook1/sources.json')
    require(load(WORK/'eve-hair-cook1/exit.json') == dict(returncode=0, sources_unchanged=True), 'Hair material cook failed')
    for p, h in hair['files'].items():
        pin(p, h)
    for name, digest in hair['editor_receipts'].items():
        pin(WORK/'eve-hair-editor1'/name, digest)
    hair_command = load(WORK/'eve-hair-cook1/command.json')
    instances = load(WORK/'eve-material-namespace1/verification.json')
    textures = load(WORK/'eve-texture-namespace2/verification.json')
    require(instances['passed'] and textures['passed'], 'Original material preservation did not pass')
    original_files = {}
    for p, h in instances['source_files'].items():
        original_files[p] = pin(p, h)
    original_root = WORK/'eve-material-namespace1/original'
    for p, h in textures['source_files'].items():
        # Other recorded inputs include the historical installation, which has
        # since been upgraded. Pin the retained original export files here.
        if Path(p).is_relative_to(original_root):
            original_files[p] = pin(p, h)
    requests = []
    for folder in ('eve-material-namespace1', 'eve-texture-namespace2'):
        request = WORK/folder/'request.json'
        requests.append(pin(request))
        for key in ('mapping', 'jobs'):
            require(key in read(request), 'Incomplete material rewrite recipe')
    replay_path = ROOT/'build/eve-material-source-recovery1/replay/verified.json'
    replay = load(replay_path)
    require(replay['passed'] and len(replay['payloads']) == 745, 'Current material recipe replay failed')
    for relative, digest in replay['payloads'].items():
        require(materials['payloads'][relative] == plan['payloads'][relative]['sha256'] == digest,
                'Replayed material output differs from candidate')
    for p, h in replay['inputs'].items():
        pin(p, h)
    historical_writer = None
    for p, h in instances['evidence'].items():
        if Path(p).resolve() == Path(replay['writer']).resolve():
            pin(p, replay['writer_sha256'])
            historical_writer = dict(path=p, historical_sha256=h, current_sha256=replay['writer_sha256'],
                                     compatibility_replay=str(replay_path))
        else:
            pin(p, h)
    require(all(sha(Path(p)) == h for p, h in inputs.items()), 'Source changed during audit')
    args.output.write_text(json.dumps(dict(passed=True, meshes=meshes,
        editor_dependency_versions=dependencies,
        material_preservation=dict(packages=materials['packages'], payloads=len(materials['payloads']),
            material_instances=instances['material_instances'], textures=textures['textures'],
            mip_payloads=textures['mips'], original_files=list(original_files.values()), requests=requests,
            writer_replay=historical_writer,
            authored_hair_parent=dict(package='/Game/CSS/eins0fx/EveSB/Hair/M_Hair1',
                source_files=hair['files'], renderer=hair['renderer'], cook_command=hair_command),
            rule='Preserve verified cooked instances/textures. Do not replace them with recooked editor binding proxies.'),
        retained_payloads=len(plan['payloads']), inputs=inputs,
        scope='Twelve final editor meshes linked to successful cook and combined payloads; '
              '272 original material/texture packages replayed from retained exports plus one authored hair parent with retained cook inputs. '
              'No fresh cook, live rendering, shader execution or full-release source-completeness claim.'), indent=2)+'\n')
    print('12 editor-to-cook mesh chains and 273 preserved material/texture packages verified')


if __name__ == '__main__':
    main()
