"""Verify retained Genessa W3 authoring inputs and their migration/face consumers."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from verify_genessa_face_inputs import AUTHOR, ROOT, WORK, read, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    out = parser.parse_args().output.resolve()
    if out.exists() or out == WORK.resolve() or not out.is_relative_to(WORK.resolve()):
        raise ValueError('Use a new directory beneath the goal workspace')
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    protected = {}

    def pin(path, expected=None):
        path = Path(path)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError(f'Changed input: {path}')
        protected[str(path)] = digest
        return digest

    def record(path):
        pin(path)
        return read(path)

    try:
        wing = AUTHOR / 'work/winggait2'
        candidate = record(AUTHOR / 'work/wingcandidate1/created.json')
        pose = record(wing / 'pose-check.json')
        hinge = record(AUTHOR / 'authoring/wing-hinge-check.json')
        proxy = record(AUTHOR / 'authoring/skirt-proxy-saved-check.json')
        original_sources = record(wing / 'sources.json')
        assert candidate['sources_unchanged'] and pose['passed'] and hinge['passed'] and proxy['passed']
        assert hinge['protected'] == original_sources
        for table in (original_sources, pose['protected'], candidate['protected']):
            for path, digest in table.items():
                pin(path, digest)
        for path, digest in proxy['inputs'].items():
            pin(AUTHOR / path, digest)
        graph = record(wing / 'graph-created.json')
        graph_path = AUTHOR / 'work/ue/Content/CSS/UnholyGenessa/ABP_Wings2.uasset'
        assert pin(graph_path, graph['sha256']) == candidate['protected'][str(graph_path)]
        for row in hinge['variants']:
            name = row['skin'].lower() + '.json'
            pin(wing / name, row['readback_sha256'])
            pin(AUTHOR / 'work/skirtproxy1' / name, row['baseline_readback_sha256'])

        # Re-run the original full-data comparison with only its report writable.
        checker = AUTHOR / 'tools/check_wing_hinges.py'
        pin(checker)
        original_report = AUTHOR / 'authoring/wing-hinge-check.json'
        output_report = out / 'wing-hinge-check.json'
        output_report.touch(exist_ok=False)
        sandbox = Path(shutil.which('bwrap') or '')
        pin(sandbox)
        pin(Path(sys.executable).resolve())
        command = [str(sandbox), '--die-with-parent', '--ro-bind', '/', '/',
                   '--bind', str(output_report), str(original_report), '--unshare-net',
                   sys.executable, '-B', str(checker)]
        (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
        with (out / 'comparison.log').open('x') as log:
            run = subprocess.run(command, cwd=AUTHOR, stdout=log, stderr=subprocess.STDOUT)
        (out / 'exit.json').write_text(json.dumps({'returncode': run.returncode}) + '\n')
        run.check_returncode()
        assert sha(output_report) == protected[str(original_report)]

        migration_path = WORK / 'genessa-rig-editor2/input.json'
        migration = record(migration_path)['assets']
        facial = record(AUTHOR / 'work/face-integration6/input.json')['meshes']
        rows = []
        assert len(candidate['created']) == 2
        for package, digest in candidate['created'].items():
            source = AUTHOR / 'work/ue/Content' / (package.removeprefix('/Game/') + '.uasset')
            pin(source, digest)
            migrated = next(r for r in migration if r['old_package'] == package)
            face = next(r for r in facial if r['package'] == package)
            assert migrated['source_sha256'] == face['sha256'] == digest
            assert migrated['source_file'] == face['source_file'] == str(source)
            rows.append(dict(package=package, source=str(source), sha256=digest,
                             namespace_target=migrated['new_package'], facial_target=face))
        assert {r['package'].rsplit('/', 1)[-1] for r in rows} == {'SK_EveW3', 'SK_GenessaW3'}
        for name in ('stage_wing_hinges.py', 'stage_wing_candidate.py', 'create_wing_layer.py'):
            pin(AUTHOR / 'tools' / name)
        for name in ('UGWingLibrary.cpp', 'UGWingLibrary.h', 'UGWingAnimLibrary.cpp',
                     'UGWingAnimLibrary.h', 'CSSAuthoring.Build.cs'):
            pin(AUTHOR / 'work/ue/Source/CSSAuthoring' / name)

        # Join to the completed downstream audits, including the current installed trio.
        downstream = {}
        for name in ('genessa-material-rig-sources-20261015',
                     'genessa-package-lineage-20261015-pass2'):
            receipt = record(WORK / name / 'verified.json')
            assert receipt['passed']
            for path, digest in receipt['inputs'].items():
                pin(path, digest)
            downstream[name] = {'receipt': str(WORK / name / 'verified.json'),
                                'sha256': protected[str(WORK / name / 'verified.json')]}
        for path, digest in protected.items():
            if sha(Path(path)) != digest:
                raise ValueError(f'Input changed during verification: {path}')
        result = dict(passed=True, inputs=protected, variants=rows, downstream=downstream,
                      hinge_comparison_sha256=sha(output_report), python=sys.version,
                      scope='Retained editable W3 sources, original wing comparison replay and '
                            'exact consumer/source identities. No new UE cook, Blender whole-outfit '
                            'export, asset installation or game-state changes.')
        (out / 'verified.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({'passed': True, 'variants': len(rows), 'protected_files': len(protected)}))
    except BaseException as error:
        (out / 'failure.json').write_text(json.dumps({'type': type(error).__name__, 'error': str(error)}) + '\n')
        raise


if __name__ == '__main__':
    main()
