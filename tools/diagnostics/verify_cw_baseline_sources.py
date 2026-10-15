"""Trace retained CW material, texture and rig sources into the namespace baseline."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from verify_cw_repair_lineage import AUTHOR, BASE, GAME, NAME, ROOT, WORK, read, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    out = parser.parse_args().output.resolve()
    if out.exists() or not out.is_relative_to(WORK.resolve()) or out == WORK.resolve():
        raise ValueError('Use a new directory beneath the goal workspace')
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    pins = {}
    relocated = {}
    superseded_tools = {}

    def pin(path, expected=None):
        path = Path(path)
        digest = pins.get(str(path)) or sha(path)
        if expected is not None and digest != expected:
            raise ValueError(f'Changed input: {path}')
        pins[str(path)] = digest
        return digest

    def record(path, expected=None):
        pin(path, expected)
        return read(path)

    def table(values):
        for value, digest in values.items():
            path = Path(value)
            installed_root = GAME / 'Content/Paks/~mods'
            if path.is_relative_to(installed_root):
                backup = GAME / 'Content/CSS-namespace-trial1/backup' / path.relative_to(installed_root)
                pin(backup, digest)
                relocated[value] = str(backup)
            else:
                historical_tools = {
                    str(ROOT / 'build/retoc-css-target/release/examples/rewrite_material'): '3f3481c2ae53b9ef967947977215f5ede8543f87785e599944a472b8e8a0efa9',
                    str(ROOT / 'tools/asset-paths/rewrite_material.rs'): 'fb506d31ade3af195c981d097130c661dc5440e2407510b5512bf51416255665',
                    str(ROOT / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'): 'c2b996455ae4b39b30a3f3caabb5a0432eae5ca07db8f3c161ba23fe111c06a0',
                    str(ROOT / 'tools/AssetReadback/Program.cs'): '8bf3673ee00eb75bad7047677c7992c06cba981c4db6e9c7d084fcdcb2f4297d',
                    str(ROOT / 'tools/asset-paths/migrate_outfit_textures.py'): 'a8cf59b3e27fa6c7d5888f08bac2d97270995cbf8edd58814d516ee4e5ef6ff3',
                }
                if historical_tools.get(str(path)) == digest:
                    superseded_tools[str(path)] = {'historical': digest, 'current': pin(path)}
                else:
                    pin(path, digest)

    try:
        base = record(BASE / 'verification.json')
        rig_dir = BASE.parent / 'commander-rig-pack3'
        rig = record(rig_dir / 'verification.json', base['protected'][str(rig_dir / 'verification.json')])
        material_dir = BASE.parent / 'commander-material-pack2'
        material = record(material_dir / 'verification.json', rig['protected'][str(material_dir / 'verification.json')])
        assert base['passed'] and rig['passed'] and material['passed']
        assert base['payloads'] == rig['payloads']
        table(material['evidence'])
        merged = {}
        material_groups = {}
        for folder, count_key, count in (
                ('commander-material-namespace2', 'material_instances', 80),
                ('commander-parent-namespace1', 'parents', 2),
                ('commander-texture-namespace1', 'textures', 78)):
            directory = WORK / folder
            proof = record(directory / 'verification.json')
            assert proof['passed'] and proof[count_key] == count
            table(proof['source_files'])
            table(proof.get('evidence', {}))
            for row in proof.get('source_containers', {}).values():
                table({row['path']: row['sha256']})
                assert Path(relocated[row['path']]).stat().st_size == row['bytes']
            assert not merged.keys() & proof['payloads'].keys()
            for key, digest in proof['payloads'].items():
                pin(directory / 'stage' / key, digest)
            merged.update(proof['payloads'])
            material_groups[folder] = {'assets': count, 'payloads': len(proof['payloads'])}
        assert merged == material['payloads'] and len(merged) == 391

        editor = WORK / 'commander-rig-editor1'
        cook = BASE.parent / 'commander-rig-cook1'
        receipts = {}
        for name in ('input.json', 'migration.json', 'reload.json', 'reload-exit.json', 'source-validation.json'):
            path = editor / name
            receipts[name] = record(path, rig['protected'][str(path)])
        assert receipts['reload.json']['passed']
        assert receipts['source-validation.json']['all_original_sources_unchanged']
        assert receipts['reload-exit.json'] == {'returncode': 0}
        assert record(cook / 'exit.json', rig['protected'][str(cook / 'exit.json')]) == {'returncode': 0}
        sources = {r['new_package']: r for r in receipts['input.json']['assets']}
        migrated = {r['package']: r for r in receipts['migration.json']['assets']}
        assert len(sources) == len(migrated) == 172
        for package, source in sources.items():
            pin(source['source_file'], source['source_sha256'])
            editable = editor / 'Content' / Path(package.removeprefix('/Game/')).with_suffix('.uasset')
            pin(editable, migrated[package]['sha256'])
        native = receipts['input.json']['native_reference']
        pin(native['source'], native['sha256'])
        pin(native['receipt'], native['receipt_sha256'])
        command = record(cook / 'command.json')
        i = command.index('--ro-bind', command.index('--die-with-parent'))
        assert command[i + 1:i + 3] == [str(editor / 'Content'), str(cook / 'project/Content')]
        pin(cook / 'packages.txt')
        rig_sources = []
        secondary_dir = WORK / 'commander-secondary-namespace1'
        secondary = record(secondary_dir / 'verification.json')
        assert secondary['passed'] and len(secondary['packages']) == 2
        table(secondary['evidence'])
        for package in (cook / 'packages.txt').read_text().splitlines():
            if '/CommanderWhite/' not in package:
                assert package == '/Game/CSS/eins0fx/Shared/Skeletons/SKEL_Base'
                continue
            relative = Path(package.removeprefix('/Game/'))
            payloads = {}
            for suffix in ('.uasset', '.uexp'):
                key = 'MortalShell2/Content/' + str(relative.with_suffix(suffix))
                path = cook / 'cooked/CSSAuthoring/Content' / relative.with_suffix(suffix)
                if key in secondary['payloads']:
                    path = secondary_dir / 'stage' / key
                    assert secondary['payloads'][key] == rig['payloads'][key]
                payloads[key] = pin(path, rig['payloads'][key])
            assert not merged.keys() & payloads.keys()
            merged.update(payloads)
            rig_sources.append({'package': package, 'source': sources[package], 'payloads': payloads})
        assert len(rig_sources) == 11 and merged == rig['payloads']
        for key, digest in base['payloads'].items():
            pin(BASE / 'stage' / key, digest)
        for filename, digest in base['files'].items():
            pin(BASE / filename, digest)
        comparison = record(rig_dir / 'comparison.json')
        assert comparison == {'passed': True, 'compared': 172, 'differences': []}
        mesh = record(rig_dir / 'mesh-evidence.json')
        assert mesh['passed'] and mesh['meshes'] == 2
        for path, digest in mesh['evidence'].items():
            pin(ROOT / path, digest)
        for row in mesh['payloads']:
            pin(ROOT / row['source'], row['sha256'])
        repairs = record(WORK / 'cw-repair-lineage-20261015-pass3/verified.json')
        assert repairs['passed'] and repairs['inputs'][str(BASE / 'verification.json')] == pins[str(BASE / 'verification.json')]
        for path, digest in repairs['installed'].items():
            pin(path, digest)
        # Replay the texture writer because its historical executable was replaced.
        writer = ROOT / 'build/retoc-css-target/release/examples/rewrite_material'
        texture_dir = WORK / 'commander-texture-namespace1'
        request = record(texture_dir / 'request.json')
        for job in request['jobs']:
            relative = Path(job['output']).relative_to(texture_dir / 'stage')
            job['output'] = str(out / 'texture-replay' / relative)
        request['report'] = str(out / 'texture-replay.json')
        (out / 'request.json').write_text(json.dumps(request, indent=2) + '\n')
        with (out / 'replay.log').open('x') as log:
            replay = subprocess.run([str(writer), str(out / 'request.json')], stdout=log, stderr=subprocess.STDOUT)
        (out / 'exit.json').write_text(json.dumps({'returncode': replay.returncode}) + '\n')
        replay.check_returncode()
        texture_payloads = read(texture_dir / 'verification.json')['payloads']
        actual = {str(p.relative_to(out / 'texture-replay')): sha(p)
                  for p in (out / 'texture-replay').rglob('*') if p.is_file()}
        assert actual == texture_payloads
        graph_request = record(secondary_dir / 'request.json')
        for job in graph_request['jobs']:
            source = Path(job['input'])
            pin(source)
            pin(source.with_suffix('.uexp'))
            relative = Path(job['output']).relative_to(secondary_dir / 'stage')
            job['output'] = str(out / 'graph-replay' / relative)
        graph_request['report'] = str(out / 'graph-replay.json')
        (out / 'graph-request.json').write_text(json.dumps(graph_request, indent=2) + '\n')
        with (out / 'graph-replay.log').open('x') as log:
            replay = subprocess.run([str(writer), str(out / 'graph-request.json')], stdout=log, stderr=subprocess.STDOUT)
        (out / 'graph-exit.json').write_text(json.dumps({'returncode': replay.returncode}) + '\n')
        replay.check_returncode()
        graph_actual = {str(p.relative_to(out / 'graph-replay')): sha(p)
                        for p in (out / 'graph-replay').rglob('*') if p.is_file()}
        assert graph_actual == secondary['payloads']
        for path, digest in pins.items():
            if sha(Path(path)) != digest:
                raise ValueError(f'Input changed during audit: {path}')
        result = {'passed': True, 'inputs': pins, 'historical_containers': relocated,
                  'material_groups': material_groups, 'editable_sources': len(sources),
                  'rig_sources': rig_sources, 'baseline_payloads': len(merged),
                  'installed': repairs['installed'],
                  'superseded_tools': superseded_tools, 'replayed_texture_payloads': actual,
                  'replayed_graph_payloads': graph_actual,
                  'scope': 'Retained source and cook identities, exact payload assembly and baseline '
                  'container hashes, linked to the verified installed repair chain. Historical native '
                  'comparison receipts are pinned, not freshly replayed. Texture and compiled graph relocation are freshly '
                  'replayed with the retained writer. No new cook or game test.'}
        (out / 'verified.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({'passed': True, 'protected_files': len(pins),
                          'editable_sources': len(sources), 'rig_packages': len(rig_sources),
                          'baseline_payloads': len(merged)}))
    except BaseException as error:
        (out / 'failure.json').write_text(json.dumps({'type': type(error).__name__, 'error': str(error)}) + '\n')
        raise


if __name__ == '__main__':
    main()
