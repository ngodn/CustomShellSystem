"""Pin current normal/facial meshes and package ownership without touching the game.

Python 3.14. Uses the existing retoc, repak and game-specific AssetReadback build.
"""
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

from three_outfit_baseline import identity

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME, processes
from css_convert import Converter, DEFAULT_REPAK

NAMES = ('CSS_EveStellarBlade_eins0fx_P', 'CSS_UnholyGenessa_eins0fx_P',
         'CSS_CommanderWhite_eins0fx_P', 'CSS_SharedAssets_P')
OWNERS = {'eins0fx.seduxtress':'CSS_EveStellarBlade_eins0fx_P',
          'eins0fx.unholygenessa':'CSS_UnholyGenessa_eins0fx_P',
          'eins0fx.commanderwhite':'CSS_CommanderWhite_eins0fx_P'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    assert out.is_relative_to((ROOT/'work').resolve()) and not out.exists()
    out.mkdir()
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    providers, manifests, packages, protected = defaultdict(list), {}, {}, {}
    mount = out/'containers'
    converter.base_containers(GAME, mount)
    runtime = GAME/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    selector = json.loads((runtime/'core.json').read_text())
    assert Path(selector['file']).name == selector['file']
    core = identity(runtime/'cores'/selector['file'])
    protected[core['path']] = core['sha256']
    for name in NAMES:
        folder = GAME/'Content/Paks/~mods'/name
        files = {s:identity(folder/(name+s)) for s in ('.pak', '.utoc', '.ucas')}
        for row in files.values():
            path = Path(row['path'])
            protected[str(path)] = row['sha256']
            (mount/path.name).symlink_to(path)
        result = subprocess.run([str(converter.retoc), 'list', '--hash', '--path',
            str(folder/(name+'.utoc'))], capture_output=True, text=True, check=True)
        (out/(name+'-index.txt')).write_text(result.stdout)
        exports = []
        for line in result.stdout.splitlines():
            if 'ExportBundleData' not in line:
                continue
            fields = line.split()
            path = fields[-1]
            assert '/Content/' in path and path.endswith('.uasset'), line
            package = '/Game/'+path.split('/Content/', 1)[1].removesuffix('.uasset')
            exports.append(package)
            providers[package].append(dict(container=name, chunk_hash=fields[2]))
        assert exports and len(set(exports)) == len(exports)
        metadata = out/('metadata-'+name)
        converter.run(DEFAULT_REPAK, 'unpack', folder/(name+'.pak'), '-o', metadata)
        paths = list(metadata.rglob('manifest.json'))
        assert len(paths) == 1
        manifest = json.loads(paths[0].read_text())
        assert all(files[s]['sha256'] == manifest['containers'][s]['sha256'] and
                   files[s]['bytes'] == manifest['containers'][s]['bytes'] for s in ('.utoc', '.ucas'))
        manifests[name] = manifest
        packages[name] = dict(files=files, id=manifest['id'], version=manifest['version'], exports=len(exports))
    assert all(len(rows) == 1 for rows in providers.values()), 'Duplicate asset providers'
    retired = []
    for name in ('CSS_AstralSharedAssets_P', 'CSS_LocomotionSharedAssets_P'):
        paths = list((GAME/'Content/Paks').rglob(name+'.utoc'))
        assert not paths, 'Retired provider remains mounted'
        retired.append(name)
    library = manifests['CSS_SharedAssets_P']['catalog']['animation_library']
    variants, meshes = [], set()
    for owner, container in OWNERS.items():
        for outfit in manifests[container]['catalog']['outfits']:
            assert outfit['id'] == owner
            for variant in outfit['variants']:
                adapters = [(face['id'], adapter) for face in library['faces'] for adapter in face['adapters']
                    if adapter['source_outfit'] == owner and adapter['source_mesh'] == variant['mesh']]
                assert len(adapters) == 1, (owner, variant['id'])
                face, adapter = adapters[0]
                files = packages[container]['files']
                assert adapter['source_toc_sha256'] == files['.utoc']['sha256']
                assert adapter['source_bulk_bytes'] == files['.ucas']['bytes']
                pair = dict(normal=variant['mesh'].split('.')[0], facial=adapter['target_mesh'].split('.')[0])
                for path in pair.values():
                    assert providers[path][0]['container'] == container, path
                    meshes.add(path)
                variants.append(dict(outfit=owner, variant=variant['id'], owner=container, meshes=pair,
                    face_rig=face, adapter_skeleton=adapter['skeleton'].split('.')[0]))
    assert len(variants) == 10 and len(meshes) == 20
    reader = ROOT/'tools/AssetReadback/bin/Release/net10.0/AssetReadback'
    mappings = GAME/'Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap'
    listing = out/'meshes.txt'
    listing.write_text('\n'.join(sorted(meshes))+'\n')
    command = [str(reader), str(mount), str(mappings), str(listing), str(out/'meshes.json'), '--mesh-surface-digests']
    (out/'readback-command.json').write_text(json.dumps(command, indent=2)+'\n')
    with (out/'readback.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
            env={**os.environ, 'DOTNET_ROOT':'/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401'})
    (out/'readback-exit.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')
    result.check_returncode()
    decoded = json.loads((out/'meshes.json').read_text())
    summaries = {}
    for path, exports in decoded.items():
        mesh = next(e for e in exports if e['Type'] == 'SkeletalMesh')
        properties = mesh['Properties']
        references = {key:value['ObjectPath'].rsplit('.', 1)[0] for key,value in properties.items()
                      if key in ('Skeleton', 'PhysicsAsset', 'PostProcessAnimBlueprint') and value}
        assert 'Skeleton' in references and 'PostProcessAnimBlueprint' in references
        assert all(len(providers.get(value, [])) == 1 for value in references.values()), references
        summaries[path] = dict(owner=providers[path][0]['container'], references=references,
                              digest=mesh['OfflineMeshDigest'])
    for variant in variants:
        pair = [summaries[p] for p in variant['meshes'].values()]
        assert all(m['references']['Skeleton'] == variant['adapter_skeleton'] for m in pair), variant
    assert json.loads((runtime/'core.json').read_text()) == selector
    for path, digest in protected.items():
        assert identity(Path(path))['sha256'] == digest, path
    report = dict(captured_utc=datetime.now(timezone.utc).isoformat(), selector=selector, core=core,
        packages=packages, variants=variants, meshes=summaries, providers=dict(providers),
        retired_providers_absent=retired, duplicate_providers=[], protected=protected,
        game_pids_at_end=processes(), reader=identity(reader.with_suffix('.dll')),
        raw_readback=identity(out/'meshes.json'), script=identity(Path(__file__)),
        scope='Installed package, mesh and facial-adapter identities and cooked surface digests. Not authoring provenance, live playback, fit or gameplay acceptance.')
    (out/'inventory.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'Pinned {len(variants)} appearances, {len(meshes)} meshes and {len(providers)} unique package owners.')


if __name__ == '__main__':
    main()
