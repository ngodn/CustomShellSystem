"""Pin the cloth follow-up to the accepted namespace trial. Python 3.14."""
import argparse
from pathlib import Path

from install_namespace_trial import (
    ROOT, GAME, EXPECTED_UE4SS, NAMES, SUFFIXES, atomic, check, check_closed,
    directory_hashes, mounts, package_matches, read, sha, validate_package_sets,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    check_closed()
    work = ROOT / 'work/three-outfit-rig-physics'
    baseline = work / 'catalog-namespace3'
    candidate = work / 'genessa-cloth-assembly1'
    core = work / 'cloth-runtime-binding1'
    evidence_paths = [baseline / 'receipt.json', baseline / 'validation.json',
                      candidate / 'assembly.json', candidate / 'mesh-upgrade.json',
                      candidate / 'graph-verification.json', candidate / 'imports.json',
                      candidate / 'catalog.log', core / 'result.json', core / 'source-hashes.json',
                      work / 'live-face-visibility1/validation.json']
    for file in evidence_paths[:6]:
        if read(file).get('passed') is not True:
            raise ValueError('Validation did not pass: ' + str(file))
    if (candidate / 'catalog.log').read_text().strip() != '1380 animation library checks passed':
        raise ValueError('Unexpected catalog result')
    core_result = read(core / 'result.json')
    if core_result['host_tests_passed'] != 24 or core_result['registered_host_tests'] != 24:
        raise ValueError('Runtime tests incomplete')
    check({str(ROOT / f): h for f, h in read(core / 'source-hashes.json').items()})
    check({str(core / 'css_core.dll'): core_result['dll_sha256']})
    receipt = read(baseline / 'receipt.json')
    assembled = read(candidate / 'assembly.json')
    check({str(candidate / 'pack' / f): h for f, h in assembled['files'].items()})
    for report_name in ('mesh-upgrade.json', 'graph-verification.json'):
        report = read(candidate / report_name)
        check({str(ROOT / f): h for f, h in report['inputs'].items()})
    runtime = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    accepted = read(work / 'live-face-visibility1/validation.json')
    selector = read(runtime / 'core.json')
    if selector != accepted['selector']:
        raise ValueError('Installed selector differs from accepted baseline')
    previous = runtime / 'cores' / selector['file']
    ue4ss = runtime.parents[1] / 'UE4SS.dll'
    check({str(previous): accepted['candidate_sha256'], str(ue4ss): EXPECTED_UE4SS})
    old, new, retained = {}, {}, {}
    for name in NAMES:
        hashes = {}
        for suffix in SUFFIXES:
            source = (baseline / 'pack' / (name + suffix)).resolve(strict=True)
            digest = receipt['files'][source.name] if suffix == '.pak' else receipt['inputs'][str(source)]
            check({str(source): digest})
            hashes[source.name] = digest
        package_matches(GAME / 'Content/Paks/~mods' / name, hashes)
        if name in ('CSS_UnholyGenessa_eins0fx_P', 'CSS_SharedAssets_P'):
            old[name] = hashes
            new[name] = {name + s: dict(source=str((candidate / 'pack' / (name + s)).resolve()),
                        sha256=assembled['files'][name + s], bytes=(candidate / 'pack' / (name + s)).stat().st_size)
                         for s in SUFFIXES}
        else:
            # Repacking identical metadata can change archive ordering; keep the accepted archive.
            for suffix in ('.utoc', '.ucas'):
                if assembled['files'][name + suffix] != hashes[name + suffix]:
                    raise ValueError('Retained outfit payload changed: ' + name)
            before, after = [directory_hashes(p / 'metadata' / name) for p in (baseline, candidate)]
            normalized = [{str(Path(k).relative_to(p / 'metadata' / name)): v for k, v in d.items()}
                          for p, d in zip((baseline, candidate), (before, after))]
            if normalized[0] != normalized[1]:
                raise ValueError('Retained outfit metadata changed: ' + name)
            retained[name] = hashes
    mounts(GAME, NAMES)
    protected = {str(p): sha(p) for p in (ue4ss, previous, runtime / 'dlls/main.dll')}
    trees = {str(p): directory_hashes(p) for p in (
        runtime / 'state', runtime.parent / 'CSSX/state', runtime.parent / 'CSSX/extensions/eins0fx.cine')}
    new_name = 'css_core-cloth-morph-' + core_result['dll_sha256'][:16] + '.dll'
    if (runtime / 'cores' / new_name).exists():
        raise FileExistsError(new_name)
    plan = dict(schema=2, game=str(GAME), old=old, new=new, retained=retained,
                previous_selector=selector, new_selector=dict(abi=1, file=new_name),
                core_source=str(core / 'css_core.dll'), core_sha256=core_result['dll_sha256'],
                protected=protected, protected_trees=trees,
                evidence={str(p): sha(p) for p in evidence_paths},
                stage_bytes=sum(r['bytes'] for rows in new.values() for r in rows.values())
                            + (core / 'css_core.dll').stat().st_size)
    validate_package_sets(plan)
    check_closed()
    atomic(args.output, plan)
    print(args.output)


if __name__ == '__main__':
    main()
