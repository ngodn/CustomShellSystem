"""Reproduce original Eve material migrations with the current pinned writer."""
import copy
import json
from pathlib import Path
import shutil
import subprocess

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'work/three-outfit-rig-physics'
RECOVERY = ROOT/'build/eve-material-source-recovery1'
OUT = RECOVERY/'replay'


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    require(not OUT.exists(), 'Replay exists; preserve any failure before another attempt')
    require(shutil.disk_usage(RECOVERY).free >= 2*1024**3, 'Need 2 GiB for replay')
    recovery = read(RECOVERY/'verified.json')
    require(recovery['passed'], 'Original source recovery failed')
    writer = ROOT/'build/retoc-css-target/release/examples/rewrite_material'
    source = ROOT/'tools/asset-paths/rewrite_material.rs'
    built_source = ROOT/'build/retoc-css/retoc/examples/rewrite_material.rs'
    require(sha(source) == sha(built_source), 'Retained writer source differs from tracked source')
    candidate_path = ROOT/'build/eve-surface-package1/plan.json'
    candidate = read(candidate_path)
    prelist = ROOT/'docs/development/cleanup-2026-10-14-eve-material-replay.md'
    pins = {str(p):sha(p) for p in (writer, source, built_source, candidate_path, prelist,
            RECOVERY/'verified.json', Path(__file__))}
    for relative, digest in recovery['original_hashes'].items():
        path = RECOVERY/'original'/relative
        require(sha(path) == digest, 'Recovered source changed')
        pins[str(path)] = digest
    OUT.mkdir()
    requests, expected = [], {}
    for folder, count in (('eve-material-namespace1', 68), ('eve-texture-namespace2', 204)):
        original = WORK/folder/'request.json'
        pins[str(original)] = sha(original)
        request = copy.deepcopy(read(original))
        require(len(request['jobs']) == count, 'Unexpected migration job count')
        for job in request['jobs']:
            relative = Path(job['output']).relative_to(WORK/folder/'stage')
            job['output'] = str(OUT/'stage'/relative)
            for suffix in ('.uasset', '.uexp', '.ubulk'):
                if not Path(job['input']).with_suffix(suffix).exists():
                    continue
                rel = str(relative.with_suffix(suffix))
                require(rel not in expected and rel in candidate['payloads'], 'Overlapping or unknown output')
                row = candidate['payloads'][rel]
                require(sha(Path(row['path'])) == row['sha256'], 'Accepted source changed')
                pins[row['path']] = row['sha256']
                expected[rel] = row
        request['report'] = str(OUT/(folder+'-report.json'))
        target = OUT/(folder+'-request.json')
        write(target, request)
        requests.append(target)
    require(len(expected) == 745, 'Incomplete replay payload set')
    write(OUT/'plan.json', dict(inputs=pins, expected=expected))
    for request in requests:
        command = [str(writer), str(request)]
        label = request.stem.removesuffix('-request')
        write(OUT/(label+'-command.json'), command)
        with (OUT/(label+'.log')).open('w') as log:
            proc = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
        write(OUT/(label+'-exit.json'), dict(returncode=proc.returncode))
        proc.check_returncode()
        require(read(OUT/(label+'-report.json'))['passed'], 'Writer rejected replay')
    actual = {str(p.relative_to(OUT/'stage')) for p in (OUT/'stage').rglob('*') if p.is_file()}
    require(actual == set(expected), 'Replay payload set differs')
    removal = []
    for relative, row in expected.items():
        path = OUT/'stage'/relative
        require(sha(path) == row['sha256'], 'Replayed payload differs: '+relative)
        removal.append(dict(path=str(path), retained=row['path'], sha256=row['sha256'], bytes=path.stat().st_size))
    require(all(sha(Path(p)) == h for p,h in pins.items()), 'Replay input changed')
    df = ['df', '-h', '/', '/run/media/eins0fx/theark']
    cleanup = dict(files=removal, before=subprocess.check_output(df, text=True), prelist_sha256=sha(prelist))
    write(OUT/'cleanup-plan.json', cleanup)
    shutil.rmtree(OUT/'stage')
    require(all(sha(Path(p)) == h for p,h in pins.items()), 'Retained replay source changed')
    write(OUT/'verified.json', dict(passed=True, payloads={p:r['sha256'] for p,r in expected.items()},
        material_instances=68, textures=204, inputs=pins, writer=str(writer), writer_sha256=sha(writer),
        unchanged_and_inverse_roundtrips=True, duplicate_logical_bytes=sum(r['bytes'] for r in removal),
        before=cleanup['before'], after=subprocess.check_output(df, text=True),
        scope='Current writer reproduces all 745 preserved material/texture payloads from recovered originals. No game changes.'))
    print('Replayed 68 material instances and 204 textures; all 745 outputs match; duplicate stage removed')


if __name__ == '__main__':
    main()
