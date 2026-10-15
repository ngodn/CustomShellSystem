"""Check retained CW repair sources, package overlays and installed final walk."""
import argparse
import copy
import json
from pathlib import Path
import shutil

from verify_genessa_face_inputs import ROOT, WORK, read, sha

AUTHOR = ROOT.parent / 'CSS-Mod-Authoring/eins0fx-collections/CSS_CommanderWhite_eins0fx_P'
CW = AUTHOR / 'work'
NAME = 'CSS_CommanderWhite_eins0fx_P'
GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
BASE = Path('/mnt/eins0fxE/CSSAuthoringWork/three-outfit-rig-physics/commander-final-container1')


def relocate(value, mapping):
    if isinstance(value, dict):
        return {k: relocate(v, mapping) for k, v in value.items()}
    if isinstance(value, list):
        return [relocate(v, mapping) for v in value]
    if isinstance(value, str):
        for old, new in mapping.items():
            if value == old or value.startswith(old + '.'):
                return new + value[len(old):]
            for kind in ('AnimSequence', 'BlendSpace'):
                if value == kind + "'" + old.rsplit('/', 1)[1] + "'":
                    return kind + "'" + new.rsplit('/', 1)[1] + "'"
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    out = parser.parse_args().output.resolve()
    if out.exists() or out == WORK.resolve() or not out.is_relative_to(WORK.resolve()):
        raise ValueError('Use a new directory beneath the goal workspace')
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    inputs = {}

    def pin(path, expected=None):
        path = Path(path)
        digest = sha(path)
        if expected is not None and digest != expected:
            raise ValueError(f'Changed input: {path}')
        inputs[str(path)] = digest
        return digest

    def record(path, expected=None):
        pin(path, expected)
        return read(path)

    def table(values):
        for path, digest in values.items():
            pin(path, digest)

    try:
        baseline = record(BASE / 'verification.json')
        assert baseline['passed']
        previous = baseline
        stages = []
        names = ['hair-strand-assembly1', 'ankle-pack1', 'belt-assembly40',
                 'foot-walk-orientation3/assembly', 'walk-final-paths1/assembly']
        expected_deltas = [(16, 0, 0), (0, 0, 18), (16, 0, 1), (18, 0, 8), (18, 18, 0)]
        for name, counts in zip(names, expected_deltas, strict=True):
            folder = CW / name
            current = record(folder / 'verification.json')
            assert current['passed']
            old, new = previous['payloads'], current['payloads']
            added, removed = sorted(new.keys() - old.keys()), sorted(old.keys() - new.keys())
            changed = sorted(k for k in new.keys() & old.keys() if new[k] != old[k])
            assert (len(added), len(removed), len(changed)) == counts, name
            changed_exports = [p for p in changed if not p.endswith('.uasset')]
            ankle_export = 'MortalShell2/Content/CSS/eins0fx/CommanderWhite/Skeletons/SKEL_Strands1.uexp'
            assert changed_exports == ([ankle_export] if name == 'ankle-pack1' else [])
            for suffix in ('.pak', '.utoc', '.ucas'):
                filename = NAME + suffix
                pin(folder / NAME / filename, current['files'][filename])
                source = str(GAME / 'Content/Paks/~mods' / NAME / filename)
                expected = previous['files'][filename]
                if name == 'hair-strand-assembly1' and suffix == '.pak':
                    # Metadata had advanced after the binary namespace baseline.
                    receipt = record(folder / 'installed.json')
                    archived = Path(receipt['transaction']) / 'backup' / NAME / filename
                    expected = pin(archived, current['inputs'][source])
                if name == 'foot-walk-orientation3/assembly' and suffix == '.pak':
                    label = record(CW / 'display-name41/verification.json')
                    assert label['passed'] and label['previous'][filename] == expected
                    expected = label['candidate_sha256']
                    pin(CW / 'display-name41' / NAME / filename, expected)
                    assert label['changed_fields'] == ['name', 'catalog.outfits[0].name',
                                                       'catalog.outfits[0].description']
                assert current['inputs'][source] == expected, (name, suffix)
            stages.append(dict(name=name, payloads=len(new), added=added, removed=removed,
                               changed_headers=[p for p in changed if p.endswith('.uasset')],
                               changed_exports=changed_exports))
            previous = current
        final = previous
        installed = record(CW / 'walk-final-paths1/assembly/installed.json')
        assert installed['installed'] and installed['saved_state_preserved']
        installed_files = {}
        for suffix in ('.pak', '.utoc', '.ucas'):
            filename = NAME + suffix
            path = GAME / 'Content/Paks/~mods' / NAME / filename
            assert installed['files'][filename] == final['files'][filename]
            installed_files[str(path)] = pin(path, final['files'][filename])

        sources = {}
        for folder, source_name, exit_name in (
                ('hair-strand-final-cook1', 'sources.json', 'exit.json'),
                ('belt-cook33', 'sources-release.json', 'exit4.json'),
                ('walk-final-paths1/cook', 'sources.json', 'exit.json')):
            source = record(CW / folder / source_name)
            table(source['files'])
            assert record(CW / folder / exit_name) == {'returncode': 0, 'sources_unchanged': True}
            sources[folder] = len(source['files'])
        waist = record(CW / 'belt-final38/sources.json')
        table(waist['files'])
        sources['belt-final38'] = len(waist['files'])

        repair_payloads = {}
        for cook, source_name, pack, assembly in (
                ('hair-strand-final-cook1', 'sources.json', 'hair-strand-final-pack1', 'hair-strand-assembly1'),
                ('belt-cook33', 'sources-release.json', 'belt-pack39', 'belt-assembly40')):
            proof = record(CW / pack / 'roundtrip.json')
            semantic = record(CW / pack / 'semantic-check.json')
            assert semantic['passed'] and len(proof['payloads']) == 16
            source = read(CW / cook / source_name)
            assert set(source['roots']) == set(proof['packages'])
            for name, digest in semantic['evidence'].items():
                pin(CW / pack / name, digest)
            assembled = read(CW / assembly / 'verification.json')['payloads']
            for relative, digest in proof['payloads'].items():
                cooked = CW / cook / 'project/Saved/Cooked/Windows/CSSShared' / relative.removeprefix('MortalShell2/')
                pin(cooked, digest)
                assert assembled[relative] == digest
                if not relative.endswith('.uasset'):
                    # Only the later ankle edit replaces the strand skeleton export.
                    if relative.endswith('/Skeletons/SKEL_Strands1.uexp'):
                        assert final['payloads'][relative] != digest
                    else:
                        assert final['payloads'][relative] == digest
            repair_payloads[pack] = proof['payloads']

        # Recompute the final walk comparison from pinned decoded data.
        walk = CW / 'walk-final-paths1'
        proof = record(walk / 'pack/verified.json', final['inputs'][str(walk / 'pack/verified.json')])
        decoded = record(walk / 'pack/decoded.json', proof['decoded_sha256'])
        assert proof['passed'] and len(proof['mapping']) == len(proof['rows']) == 9
        animations = 0
        for old, new in proof['mapping'].items():
            assert len(decoded[old]) == len(decoded[new]) == 1
            expected = relocate(copy.deepcopy(decoded[old][0]), proof['mapping'])
            actual = decoded[new][0]
            expected['Name'] = actual['Name']
            assert actual['Name'] == new.rsplit('/', 1)[1] and expected == actual
            if actual['Type'] == 'AnimSequence':
                assert len(actual['Properties']['RetargetSourceAssetReferencePose']) == 395
                assert actual['OfflineAnimationDigest']
                animations += 1
        assert animations == 8
        reloaded = record(walk / 'reloaded.json')
        assert reloaded['passed'] and len(reloaded['captures']) == 14
        table(reloaded['protected'])
        table(reloaded['assets'])
        assert record(walk / 'reload-exit.json') == {'returncode': 0}
        for item in reloaded['captures']:
            assert item['exact_after_path_mapping']
            pin(CW / 'foot-walk-orientation3/blendbound' / item['file'], item['sha256'])
        for relative, digest in proof['payloads'].items():
            pin(walk / 'pack/stage' / relative, digest)
            cooked = walk / 'cook/project/Saved/Cooked/Windows/CSSShared' / relative.removeprefix('MortalShell2/')
            pin(cooked, digest)
            assert final['payloads'][relative] == digest
        old = read(CW / 'foot-walk-orientation3/assembly/verification.json')['payloads']
        removed = {'MortalShell2/' + p for p in final['removed_payloads']}
        expected = {k: v for k, v in old.items() if k not in removed}
        assert len(old) - len(expected) == 18
        assert not expected.keys() & proof['payloads'].keys()
        expected.update(proof['payloads'])
        assert expected == final['payloads']
        for path, digest in inputs.items():
            if sha(Path(path)) != digest:
                raise ValueError(f'Input changed during audit: {path}')
        (out / 'verified.json').write_text(json.dumps(dict(
            passed=True, inputs=inputs, stages=stages, source_counts=sources,
            repair_payloads=repair_payloads,
            installed=installed_files, walking_sequences=animations, walking_blends=1,
            preserved_final_payloads=445, scope='Retained CW repair source/cook identities, '
            'assembly tables, actual container hashes and archived final-walk decoded comparison. '
            'Header reconstruction differences are listed, not claimed byte-identical. '
            'No fresh cook, new game test or original whole-outfit Blender rebuild.'), indent=2) + '\n')
        print(json.dumps({'passed': True, 'protected_files': len(inputs), 'stages': len(stages),
                          'final_payloads': len(final['payloads']), 'preserved_final_payloads': 445}))
    except BaseException as error:
        (out / 'failure.json').write_text(json.dumps({'type': type(error).__name__, 'error': str(error)}) + '\n')
        raise


if __name__ == '__main__':
    main()
