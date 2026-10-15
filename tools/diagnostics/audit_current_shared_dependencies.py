"""Exercise current packages with missing, duplicate and retired shared providers."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

from audit_coordinated_upgrade import ROOT, WORK, EVE, SHARED, UG, CW, read, write, require

sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME, sha
from css_convert import DEFAULT_REPAK


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    out = parser.parse_args().output.resolve()
    require(out.is_relative_to((ROOT / 'build').resolve()) and not out.exists(),
            'Use a new directory beneath build')
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    protected = {}

    def pin(path, expected=None):
        digest = sha(path)
        require(expected is None or digest == expected, 'Changed input: ' + str(path))
        protected[str(path)] = digest

    def run(label, command):
        command = list(map(str, command))
        write(out / (label + '-command.json'), command)
        with (out / (label + '.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(out / (label + '-exit.json'), {'returncode': result.returncode})
        return result.returncode

    try:
        current = GAME / 'Content/Paks/~mods'
        backup = GAME / 'Content/CSS-namespace-trial1/backup'
        locomotion, astral = 'CSS_LocomotionSharedAssets_P', 'CSS_AstralSharedAssets_P'
        sources = {name: current / name for name in (EVE, UG, CW, SHARED)}
        sources.update({name: backup / name for name in (locomotion, astral)})
        candidate = ROOT / 'build/eve-surface-package1'
        expected = read(candidate / 'verified.json')
        installed = read(candidate / 'installation-postcheck.json')
        require(expected['passed'] and installed['passed'], 'Missing current-package proof')
        for name in ('verified.json', 'installation-postcheck.json'):
            pin(candidate / name)
        retoc = ROOT / 'build/retoc-css-target/release/retoc'
        test = ROOT / 'build/host/css_package_dependency_tests'
        for path in (retoc, DEFAULT_REPAK, test, ROOT / 'tests/package_dependency_tests.cpp', Path(__file__)):
            pin(path)
        inventories = {}
        for name, folder in sources.items():
            for suffix in ('.pak', '.utoc', '.ucas'):
                filename = name + suffix
                digest = (expected['files'][name + '/' + filename] if name in (EVE, SHARED)
                          else installed['retained'][name][filename] if name in (UG, CW) else None)
                pin(folder / filename, digest)
            require(run(name + '-inventory', [retoc, 'list', '--hash', '--path', folder / (name + '.utoc')]) == 0,
                    'Inventory failed')
            exports = {}
            for line in (out / (name + '-inventory.log')).read_text().splitlines():
                if 'ExportBundleData' not in line:
                    continue
                fields = line.split()
                path = fields[-1]
                require('/Content/' in path and path.endswith('.uasset'), 'Unexpected package path')
                package = '/Game/' + path.split('/Content/', 1)[1][:-7]
                require(package not in exports, 'Duplicate package within a container')
                exports[package] = fields[2]
            require(bool(exports), 'Empty container')
            inventories[name] = exports
        write(out / 'inventories.json', inventories)
        write(out / 'inputs.json', protected)
        outfits = ['eins0fx.seduxtress', 'eins0fx.unholygenessa', 'eins0fx.commanderwhite']
        roots = [EVE, UG, CW]
        definitions = [
            ('current', roots + [SHARED], outfits, ['css.locomotion'], 4, 0, 10, []),
            ('missing', roots, [], [], 0, 3, 0, ['Install CSS Shared Assets']),
            ('old-locomotion', roots + [locomotion], [], ['css.locomotion'], 1, 3, 0,
             ['Update CSS Shared Assets']),
            ('both-locomotion', roots + [SHARED, locomotion], [], [], 0, 5, 0,
             ['Remove duplicate copies of CSS Shared Assets']),
            ('duplicate-current', roots + [SHARED, SHARED], [], [], 0, 5, 0,
             ['Remove duplicate copies of CSS Shared Assets']),
            ('astral-without-shared', roots + [astral], [], [], 0, 3, 0,
             ['Install CSS Shared Assets']),
            ('current-with-astral', roots + [SHARED, astral], outfits, ['css.locomotion'], 4, 0, 10, []),
            ('current-with-both-retired', roots + [SHARED, locomotion, astral], [], [], 0, 5, 0,
             ['Remove duplicate copies of CSS Shared Assets']),
        ]
        expectations, overlaps = [], {}
        for case, names, available, libraries, loaded, rejected, faces, reasons in definitions:
            owners = {}
            for index, name in enumerate(names):
                folder = out / 'cases' / case / str(index)
                folder.mkdir(parents=True)
                for suffix in ('.pak', '.utoc', '.ucas'):
                    filename = name + suffix
                    (folder / filename).symlink_to(sources[name] / filename)
                for package, digest in inventories[name].items():
                    owners.setdefault(package, []).append({'container': name, 'copy': index, 'hash': digest})
            overlap = {k: v for k, v in owners.items() if len(v) > 1}
            overlaps[case] = {'packages': overlap,
                              'divergent': [k for k, v in overlap.items() if len({r['hash'] for r in v}) > 1]}
            expectations.append(dict(name=case, outfits=available, libraries=libraries,
                                     loaded=loaded, rejected=rejected, face_matches=faces, reasons=reasons))
        write(out / 'expectations.json', expectations)
        write(out / 'overlaps.json', overlaps)
        code = run('catalog', [test, out / 'cases', out / 'expectations.json'])
        unchanged = all(sha(Path(path)) == digest for path, digest in protected.items())
        result = dict(passed=code == 0 and unchanged, catalog_returncode=code,
                      inputs_unchanged=unchanged, cases=len(expectations),
                      overlap_counts={k: {'overlaps': len(v['packages']), 'divergent': len(v['divergent'])}
                                      for k, v in overlaps.items()}, game_files_written=False,
                      scope='Production host catalog with actual current trios and retained retired packs; '
                            'export-chunk overlap inventory. Does not mount conflicting packages in the '
                            'game or prove clean-install, save migration or runtime rendering.')
        write(out / 'report.json', result)
        print(json.dumps(result), flush=True)
        require(result['passed'], 'Dependency checks failed; retain evidence')
    except BaseException as error:
        write(out / 'failure.json', {'type': type(error).__name__, 'error': str(error)})
        raise


if __name__ == '__main__':
    main()
