"""Verify retained Genessa namespace, cloth and hair stages against installed files."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'work/three-outfit-rig-physics'
AUTHOR = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P'
NAME = 'CSS_UnholyGenessa_eins0fx_P'
NAMESPACE = Path('/mnt/eins0fxE/CSSAuthoringWork/three-outfit-rig-physics/genessa-face-namespace-pack2')
GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def relative(value):
    path = Path(value)
    assert not path.is_absolute() and '..' not in path.parts, value
    return path


def graph_changes(original, candidate):
    packages = {'/Game/CSS/eins0fx/UnholyGenessa/ABP_Wings2',
                '/Game/CSS/eins0fx/UnholyGenessa/Animations/Adapters/ABP_Genessa'}
    assert set(candidate) == packages
    for package in sorted(packages):
        expected = copy.deepcopy(original[package])
        cdo = 'Default__' + package.rsplit('/', 1)[1] + '_C'
        before, = [row for row in expected if row.get('Name') == cdo]
        after, = [row for row in candidate[package] if row.get('Name') == cdo]
        field, = [f for row in candidate[package] if row.get('Type') == 'AnimBlueprintGeneratedClass'
                  for f in row.get('ChildProperties', []) if f.get('Name') == 'CSSEnabled']
        assert field['Type'] == 'BoolProperty' and field['bIsNativeBool']
        assert before['Properties']['CSSEnabled'] is True
        assert after['Properties'].get('CSSEnabled', False) is False
        if 'CSSEnabled' in after['Properties']:
            before['Properties']['CSSEnabled'] = False
        else:
            del before['Properties']['CSSEnabled']
        flags = set(before['Flags'].split(' | '))
        assert set(after['Flags'].split(' | ')) in (flags, flags | {'RF_Transactional'})
        before['Flags'] = after['Flags']
        assert expected == candidate[package], package
    return sorted(packages)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    assert not out.exists() and out.is_relative_to(WORK.resolve()) and out != WORK.resolve()
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    protected = {}
    counts = {}

    def protect(path, expected=None):
        path = path.resolve(strict=True)
        digest = sha(path) if str(path) not in protected else protected[str(path)]
        assert expected is None or digest == expected, str(path)
        protected[str(path)] = digest
        return digest

    def record(path):
        protect(path)
        return read(path)

    def hashes(label, table, base=None):
        for path, digest in table.items():
            protect(base / relative(path) if base else Path(path), digest)
        counts[label] = len(table)

    try:
        cloth_dir = WORK / 'genessa-cloth-assembly1'
        cook_dir = WORK / 'genessa-cloth-final-cook1'
        hair_dir = AUTHOR / 'work/hair-solver-pack3'
        namespace = record(NAMESPACE / 'verification.json')
        cloth = record(cloth_dir / 'assembly.json')
        hair = record(hair_dir / 'verification.json')
        assert namespace['passed'] and namespace['differences'] == []
        assert cloth['passed'] and hair['passed']
        for label, table in [('namespace_inputs', namespace['protected']),
                             ('cloth_inputs', cloth['inputs']), ('hair_inputs', hair['inputs'])]:
            hashes(label, table)
        hashes('namespace_payloads', namespace['payloads'], NAMESPACE / 'stage')
        hashes('cloth_payloads', cloth['payloads'][NAME], cloth_dir / 'stage' / NAME)
        hashes('hair_payloads', hair['payloads'], hair_dir / 'stage')
        actual = {str(f.relative_to(hair_dir / 'stage')) for f in (hair_dir / 'stage').rglob('*') if f.is_file()}
        assert actual == set(hair['payloads'])

        assert record(cook_dir / 'exit.json') == {'exit_code': 0, 'project_inputs_unchanged': True}
        hashes('cloth_editor_sources', record(cook_dir / 'sources.json'))
        editor = WORK / 'genessa-cloth-final-editor1'
        finalized = record(editor / 'finalize.json')
        reloaded = record(editor / 'reload.json')
        assert finalized['passed'] and reloaded['passed']
        assert finalized['source_assets_unchanged'] and reloaded['source_assets_unchanged']
        assert finalized['packages'] == reloaded['packages'] and len(finalized['packages']) == 6
        for name in ('finalize.py', 'finalize-plan.json', 'command.json', 'reload-command.json'):
            protect(editor / name)

        changed_meshes = {f'CSS/eins0fx/UnholyGenessa/{folder}SK_{body}W3{suffix}'
                          for body in ('Eve', 'Genessa') for folder, suffix in (('', ''), ('Faces/', '_Face'))}
        assert set(cloth['changed_meshes']) == changed_meshes
        mesh_payloads = set()
        expected_cloth = dict(namespace['payloads'])
        cooked = cook_dir / 'project/Saved/Cooked/Windows/CSSShared/Content'
        for package in sorted(changed_meshes):
            for suffix in ('.uasset', '.uexp', '.ubulk'):
                key = 'MortalShell2/Content/' + package + suffix
                source = cooked / (package + suffix)
                expected_cloth.pop(key, None)
                if suffix == '.ubulk' and not source.exists():
                    continue
                expected_cloth[key] = protect(source)
                mesh_payloads.add(key)
        assert expected_cloth == cloth['payloads'][NAME], 'Unexpected namespace-to-cloth change'

        graphs = graph_changes(record(WORK / 'genessa-rig-pack2/decoded.json'), record(hair_dir / 'graphs.json'))
        expected_hair = dict(cloth['payloads'][NAME])
        graph_payloads = set()
        hair_cook = AUTHOR / 'work/hair-solver-cook1'
        assert record(hair_cook / 'exit.json') == {'returncode': 0, 'sources_unchanged': True}
        hashes('hair_editor_sources', record(hair_cook / 'sources.json'))
        for package in graphs:
            for suffix in ('.uasset', '.uexp'):
                rel = package.removeprefix('/Game/') + suffix
                key = 'MortalShell2/Content/' + rel
                expected_hair[key] = protect(hair_cook / 'cooked/CSSAuthoring/Content' / rel)
                graph_payloads.add(key)
        assert expected_hair == hair['payloads'], 'Unexpected cloth-to-hair change'
        changed = {key for key in hair['payloads'] if hair['payloads'][key] != cloth['payloads'][NAME][key]}
        assert changed == graph_payloads
        installed = {}
        for suffix in ('.pak', '.utoc', '.ucas'):
            filename = NAME + suffix
            digest = hair['files'][filename]
            protect(hair_dir / NAME / filename, digest)
            path = GAME / 'Content/Paks/~mods' / NAME / filename
            installed[str(path)] = protect(path, digest)

        metadata = hair_dir / 'metadata'
        metadata_readback = hair_dir / 'metadata-readback'
        metadata_hashes = {str(f.relative_to(metadata)): protect(f) for f in metadata.rglob('*') if f.is_file()}
        readback_hashes = {str(f.relative_to(metadata_readback)): protect(f)
                           for f in metadata_readback.rglob('*') if f.is_file()}
        assert metadata_hashes == readback_hashes
        manifest_path, = metadata.rglob('manifest.json')
        manifest = read(manifest_path)
        for suffix in ('.utoc', '.ucas'):
            payload = hair_dir / NAME / (NAME + suffix)
            assert manifest['containers'][suffix]['sha256'] == hair['files'][payload.name]
            assert manifest['containers'][suffix]['bytes'] == payload.stat().st_size

        for path, digest in protected.items():
            assert sha(Path(path)) == digest, 'Input changed during audit: ' + path
        report = dict(passed=True, counts=counts, inputs=protected, installed=installed,
                      cloth_mesh_payloads=sorted(mesh_payloads), hair_graph_payloads=sorted(graph_payloads),
                      preserved_after_cloth=len(hair['payloads']) - len(graph_payloads),
                      metadata_readback_files=len(metadata_hashes),
                      scope='Retained source identities, cook receipts, exact stage overlays, archived graph '
                            'and metadata readbacks, and current installed trio identity. No new cook or '
                            'package rebuild, fresh readback, whole-outfit Blender replay, shared-current '
                            'package audit, runtime or performance acceptance.')
        (out / 'verified.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({k: report[k] for k in ('passed', 'counts', 'preserved_after_cloth', 'metadata_readback_files')}))
    except Exception as exc:
        (out / 'failure.json').write_text(json.dumps({'passed': False, 'error': repr(exc)}, indent=2) + '\n')
        raise


if __name__ == '__main__':
    main()
