"""Check current Eve/shared containers against current and published legacy outfits.

Python 3.14. Does not install, version or publish packages.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
from css import GAME, sha
from css_convert import Converter, DEFAULT_REPAK

OUT = ROOT/'build/coordinated-upgrade-20261014'
CANDIDATE = ROOT/'build/eve-surface-package1'
WORK = ROOT/'work/three-outfit-rig-physics'
EVE = 'CSS_EveStellarBlade_eins0fx_P'
SHARED = 'CSS_SharedAssets_P'
UG = 'CSS_UnholyGenessa_eins0fx_P'
CW = 'CSS_CommanderWhite_eins0fx_P'


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    args = parser.parse_args()
    OUT = args.output.resolve()
    require(not OUT.exists(), 'Use a new output directory')
    OUT.mkdir()
    pins = {}

    def pin(path, expected=None):
        path = Path(path)
        digest = sha(path)
        require(expected is None or digest == expected, 'Input differs: '+str(path))
        pins[str(path)] = digest

    def run(label, command):
        command = list(map(str, command))
        write(OUT/(label+'-command.json'), command)
        with (OUT/(label+'.log')).open('x') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(OUT/(label+'-exit.json'), dict(returncode=result.returncode))
        return result.returncode

    proof = read(CANDIDATE/'verified.json')
    installed = read(CANDIDATE/'installation-postcheck.json')
    require(proof['passed'] and installed['passed'], 'Current candidate proof is missing')
    for name in ('verified.json', 'installation-postcheck.json', 'imports-command.json'):
        pin(CANDIDATE/name)
    old_inputs_path = WORK/'peony-public-upgrade2/inputs.json'
    pin(old_inputs_path)
    old_inputs = read(old_inputs_path)
    retoc = ROOT/'build/retoc-css-target/release/retoc'
    catalog = ROOT/'build/host/css_package_dependency_tests'
    original_command = read(CANDIDATE/'imports-command.json')
    upgrade_command_path = WORK/'peony-public-upgrade2/both-legacy/command.json'
    pin(upgrade_command_path)
    upgrade_command = read(upgrade_command_path)
    dotnet, auditor, mappings = map(Path, (original_command[0], upgrade_command[1], original_command[3]))
    pin(auditor, old_inputs[str(auditor)])
    for p in (retoc, DEFAULT_REPAK, dotnet, auditor, auditor.parent/'CUE4Parse.dll', mappings,
              catalog, Path(__file__), ROOT/'tests/package_dependency_tests.cpp'):
        pin(p)
    current = GAME/'Content/Paks/~mods'
    legacy = GAME/'Content/CSS-namespace-trial1/backup'
    sources = {
        'eve': (EVE, CANDIDATE/'pack'/EVE),
        'shared': (SHARED, CANDIDATE/'pack'/SHARED),
        'ug-current': (UG, current/UG), 'cw-current': (CW, current/CW),
        'ug-legacy': (UG, legacy/UG), 'cw-legacy': (CW, legacy/CW),
    }
    inventories, manifests = {}, {}
    for key, (container, folder) in sources.items():
        for suffix in ('.pak', '.utoc', '.ucas'):
            path = folder/(container+suffix)
            if key.endswith('-legacy'):
                expected = old_inputs[str(path)]
            elif key.endswith('-current'):
                expected = installed['retained'][container][path.name]
            else:
                expected = proof['files'][container+'/'+path.name]
            pin(path, expected)
        metadata = OUT/'metadata'/key
        metadata.mkdir(parents=True)
        require(run(key+'-metadata', [DEFAULT_REPAK, 'unpack', folder/(container+'.pak'),
                    '--output', metadata]) == 0, 'Metadata unpack failed')
        manifest, = metadata.rglob('manifest.json')
        manifests[key] = read(manifest)
        require(run(key+'-inventory', [retoc, 'list', '--hash', '--path', folder/(container+'.utoc')]) == 0,
                'Container listing failed')
        exports = {}
        for line in (OUT/(key+'-inventory.log')).read_text().splitlines():
            if 'ExportBundleData' not in line:
                continue
            fields = line.split()
            path = fields[-1]
            require('/Content/' in path and path.endswith('.uasset'), 'Unexpected export path')
            package = '/Game/'+path.split('/Content/', 1)[1][:-7]
            require(package not in exports, 'Duplicate export in container')
            exports[package] = fields[2]
        require(exports, 'Empty container')
        inventories[key] = exports
    write(OUT/'inputs.json', pins)
    write(OUT/'inventories.json', inventories)
    scenarios = {
        'all-current': (['eve', 'shared', 'ug-current', 'cw-current'], 10),
        'legacy-genessa': (['eve', 'shared', 'ug-legacy', 'cw-current'], 8),
        'legacy-commander': (['eve', 'shared', 'ug-current', 'cw-legacy'], 9),
        'both-legacy': (['eve', 'shared', 'ug-legacy', 'cw-legacy'], 7),
    }
    expectations, results = [], []
    for case, (keys, faces) in scenarios.items():
        stage = OUT/case
        stage.mkdir()
        mount = stage/'containers'
        Converter(retoc, DEFAULT_REPAK, stage).base_containers(GAME, mount)
        owners = {}
        for key in keys:
            container, folder = sources[key]
            discover = OUT/'cases'/case/container
            discover.mkdir(parents=True)
            for suffix in ('.pak', '.utoc', '.ucas'):
                path = folder/(container+suffix)
                (mount/path.name).symlink_to(path.resolve())
                (discover/path.name).symlink_to(path.resolve())
            for package, digest in inventories[key].items():
                owners.setdefault(package, []).append(dict(source=key, chunk_hash=digest))
        overlaps = {p:v for p,v in owners.items() if len(v)>1}
        divergent = [p for p,v in overlaps.items() if len({r['chunk_hash'] for r in v})>1]
        write(stage/'overlaps.json', overlaps)
        packages, references = stage/'packages.txt', stage/'references.txt'
        packages.write_text('\n'.join(sorted(owners))+'\n')
        references.write_text('')
        command = [dotnet, auditor, mount, mappings, packages, references, stage/'imports.json',
                   '--package-name-paths']
        returncode = run(case+'-imports', command)
        require((stage/'imports.json').exists(), 'Auditor produced no report')
        audit = read(stage/'imports.json')
        row = dict(case=case, returncode=returncode, packages=len(audit['packages']),
                   failures=len(audit['failures']), overlapping_packages=len(overlaps),
                   divergent_packages=divergent, expected_face_matches=faces)
        results.append(row)
        expectations.append(dict(name=case, outfits=sorted(manifests[k]['id'] for k in keys if k!='shared'),
            libraries=['css.locomotion'], loaded=4, rejected=0, face_matches=faces))
        print(json.dumps(row), flush=True)
    write(OUT/'expectations.json', expectations)
    catalog_code = run('catalog', [catalog, OUT/'cases', OUT/'expectations.json'])
    unchanged = all(sha(Path(p)) == digest for p,digest in pins.items())
    passed = unchanged and catalog_code == 0 and all(r['returncode']==0 and r['failures']==0
                                                    and not r['divergent_packages'] for r in results)
    write(OUT/'report.json', dict(passed=passed, cases=results, catalog_returncode=catalog_code,
        inputs_unchanged=unchanged, game_files_written=False,
        versions={k:m['version'] for k,m in manifests.items()},
        scope='Current candidate plus mixed published legacy/current outfits: package imports, duplicate-provider content and catalog/facial revision matching. Not live rendering, save migration, CINE execution or stable release artifacts.'))
    require(passed, 'Upgrade audit failed; inspect retained reports')


if __name__ == '__main__':
    main()
