"""Trace Genessa material/rig cooks and replay the preserved material instances."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

from verify_genessa_package_lineage import AUTHOR, GAME, NAME, ROOT, WORK, read, sha

OUT = WORK / 'genessa-material-rig-sources-20261015'


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def main():
    assert not OUT.exists(), 'Preserve prior attempts before running again'
    OUT.mkdir()
    shutil.copyfile(__file__, OUT / 'recipe.py')
    pins = {}

    def pin(path, expected=None):
        path = Path(path).resolve(strict=True)
        digest = pins.get(str(path)) or sha(path)
        assert expected is None or digest == expected, str(path)
        pins[str(path)] = digest
        return digest

    def record(path, expected=None):
        pin(path, expected)
        return read(path)

    def table(values):
        for path, digest in values.items():
            pin(path, digest)

    try:
        final_dir = AUTHOR / 'work/hair-solver-pack3'
        final = record(final_dir / 'verification.json')
        rig_dir = WORK / 'genessa-rig-pack2'
        rig = record(rig_dir / 'verification.json')
        materials_dir = WORK / 'genessa-material-pack2'
        materials = record(materials_dir / 'verification.json', rig['protected'][str(materials_dir / 'verification.json')])
        instance_dir = WORK / 'genessa-material-namespace2'
        instances = record(instance_dir / 'verification.json', materials['evidence'][str(instance_dir / 'verification.json')])
        assert final['passed'] and rig['passed'] and materials['passed'] and instances['passed']
        table(materials['evidence'])
        table(instances['source_files'])
        archive = AUTHOR / 'dist/v0.2.0/CSS_UnholyGenessa_eins0fx_P-v0.2.0.zip'
        checksum = archive.with_suffix('.zip.sha256')
        pin(checksum)
        pin(archive, checksum.read_text().split()[0])
        with zipfile.ZipFile(archive) as release:
            for suffix, row in instances['source_containers'].items():
                name = NAME + '/' + NAME + suffix
                assert release.getinfo(name).file_size == row['bytes']
                with release.open(name) as stream:
                    assert hashlib.file_digest(stream, 'sha256').hexdigest() == row['sha256']
        for suffix in ('.pak', '.utoc', '.ucas'):
            filename = NAME + suffix
            pin(GAME / 'Content/Paks/~mods' / NAME / filename, final['files'][filename])

        cook = WORK / 'genessa-material-cook2'
        editor = WORK / 'genessa-material-editor1'
        sources = record(cook / 'sources.json')
        table(sources['files'])
        for name, digest in sources['editor_receipts'].items():
            pin(editor / name, digest)
        assert record(cook / 'exit.json') == {'returncode': 0, 'sources_unchanged': True}
        assert record(editor / 'reload.json')['passed']
        original = record(editor / 'input.json')['assets']
        migrated = {r['package']: r for r in record(editor / 'migration.json')['assets']}
        assert len(original) == len(migrated) == 55
        authored = []
        expected_materials = dict(instances['payloads'])
        for source in original:
            package = source['new_package']
            rel = Path(package.removeprefix('/Game/'))
            pin(source['source_file'], source['source_sha256'])
            editable = editor / 'Content' / rel.with_suffix('.uasset')
            assert pin(editable, migrated[package]['sha256']) == sources['files'][str(editable)]
            outputs = {}
            for suffix in ('.uasset', '.uexp', '.ubulk'):
                path = cook / 'project/Saved/Cooked/Windows/CSSShared/Content' / rel.with_suffix(suffix)
                if suffix == '.ubulk' and not path.exists():
                    continue
                key = 'MortalShell2/Content/' + str(rel.with_suffix(suffix))
                assert key not in expected_materials
                outputs[key] = pin(path, materials['payloads'][key])
                expected_materials[key] = outputs[key]
            authored.append(dict(package=package, source=source, editor=str(editable), payloads=outputs))
        assert expected_materials == materials['payloads']
        for key, digest in materials['payloads'].items():
            assert rig['payloads'][key] == final['payloads'][key] == digest, key
            pin(final_dir / 'stage' / key, digest)

        rig_editor = WORK / 'genessa-rig-editor2'
        rig_cook = WORK / 'genessa-rig-cook1'
        for name in ('input.json', 'migration.json', 'reload.json', 'reload-exit.json', 'source-validation.json'):
            pin(rig_editor / name, rig['protected'][str(rig_editor / name)])
        assert record(rig_editor / 'reload.json')['passed']
        assert record(rig_editor / 'source-validation.json')['all_original_sources_unchanged']
        assert record(rig_cook / 'exit.json', rig['protected'][str(rig_cook / 'exit.json')]) == {'returncode': 0}
        command = record(rig_cook / 'command.json')
        i = command.index('--ro-bind', command.index('--die-with-parent'))
        assert command[i + 1:i + 3] == [str(rig_editor / 'Content'), str(rig_cook / 'project/Content')]
        packages = (rig_cook / 'packages.txt').read_text().splitlines()
        pin(rig_cook / 'packages.txt')
        rig_originals = {r['new_package']: r for r in record(rig_editor / 'input.json')['assets']}
        rig_sources = []
        for package in packages:
            if '/UnholyGenessa/' not in package:
                continue
            source = rig_originals[package]
            pin(source['source_file'], source['source_sha256'])
            rel = Path(package.removeprefix('/Game/'))
            editable = rig_editor / 'Content' / rel.with_suffix('.uasset')
            pin(editable, rig['protected'][str(editable)])
            outputs = {}
            for suffix in ('.uasset', '.uexp'):
                path = rig_cook / 'cooked/CSSAuthoring/Content' / rel.with_suffix(suffix)
                key = 'MortalShell2/Content/' + str(rel.with_suffix(suffix))
                outputs[key] = pin(path, rig['payloads'][key])
            rig_sources.append(dict(package=package, source=source, editor=str(editable), payloads=outputs,
                                    unchanged_in_final=all(final['payloads'][k] == h for k, h in outputs.items())))
        assert len(rig_sources) == 13
        expected_changed = {'SK_EveW3', 'SK_GenessaW3', 'ABP_Wings2', 'ABP_Genessa'}
        assert {r['package'].rsplit('/', 1)[1] for r in rig_sources if not r['unchanged_in_final']} == expected_changed

        writer = ROOT / 'build/retoc-css-target/release/examples/rewrite_material'
        source = ROOT / 'tools/asset-paths/rewrite_material.rs'
        assert pin(source) == pin(ROOT / 'build/retoc-css/retoc/examples/rewrite_material.rs')
        pin(writer)
        prelist = ROOT / 'docs/development/cleanup-2026-10-15-genessa-material-replay.md'
        pin(prelist)
        request = copy.deepcopy(record(instance_dir / 'request.json', materials['evidence'][str(instance_dir / 'request.json')]))
        assert len(request['jobs']) == 33
        for job in request['jobs']:
            rel = Path(job['output']).relative_to(instance_dir / 'stage')
            job['output'] = str(OUT / 'stage' / rel)
        request['report'] = str(OUT / 'relocation.json')
        write(OUT / 'request.json', request)
        write(OUT / 'inputs.json', pins)
        command = [str(writer), str(OUT / 'request.json')]
        write(OUT / 'command.json', command)
        with (OUT / 'replay.log').open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(OUT / 'exit.json', {'returncode': result.returncode})
        result.check_returncode()
        replay = record(OUT / 'relocation.json')
        assert replay['passed'] and len(replay['materials']) == 33
        assert all(r['unchanged_roundtrip'] and r['inverse_roundtrip'] for r in replay['materials'])
        actual = {str(p.relative_to(OUT / 'stage')): sha(p) for p in (OUT / 'stage').rglob('*') if p.is_file()}
        assert actual == instances['payloads']
        assert all(sha(Path(p)) == h for p, h in pins.items())
        df = ['df', '-h', '/', '/run/media/eins0fx/theark']
        cleanup = dict(before=subprocess.check_output(df, text=True), files=[dict(path=str(OUT / 'stage' / p),
                       sha256=h, bytes=(OUT / 'stage' / p).stat().st_size) for p, h in actual.items()])
        write(OUT / 'cleanup-plan.json', cleanup)
        for row in cleanup['files']:
            path = Path(row['path'])
            assert sha(path) == row['sha256']
            path.unlink()
        shutil.rmtree(OUT / 'stage')
        write(OUT / 'cleanup.json', dict(files=len(actual), logical_bytes=sum(r['bytes'] for r in cleanup['files']),
                                       before=cleanup['before'], after=subprocess.check_output(df, text=True)))
        write(OUT / 'verified.json', dict(passed=True, inputs=pins, authored_material_sources=authored,
              rig_sources=rig_sources, replayed_instances=33, replayed_payloads=actual,
              renderer=sources['renderer'], engine=sources['engine'],
              scope='Retained material/rig sources and cooks, exact 33-instance writer replay, unchanged '
                    'material payloads in installed lineage. No new UE cook or gameplay test. Later four '
                    'mesh/graph replacements are accounted for by the separate package-lineage audit.'))
        print(json.dumps(dict(passed=True, authored_materials=len(authored), rig_sources=len(rig_sources),
                              replayed_instances=33, preserved_material_payloads=len(materials['payloads']))))
    except Exception as exc:
        write(OUT / 'failure.json', dict(passed=False, error=repr(exc)))
        raise


if __name__ == '__main__':
    main()
