"""Re-export Genessa's accepted head patch from its retained Blender source."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from verify_genessa_face_inputs import AUTHOR, ROOT, WORK, read, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    out = args.output.resolve()
    if out.exists() or not out.is_relative_to(WORK.resolve()) or out == WORK.resolve():
        raise ValueError('Use a new directory beneath the goal workspace')
    old = AUTHOR / 'work/face-patch1'
    original = read(old / 'report.json')
    blender = ROOT.parent / 'CSS-eins0fx-collections/reference-tools/blender-5.2.2-linux-x64/blender'
    version = subprocess.check_output([str(blender), '--version'], text=True)
    if version.splitlines()[0] != 'Blender 5.2.2 LTS':
        raise ValueError('Expected the retained Blender 5.2.2 toolchain')
    protected = dict(original['inputs'])
    for path, digest in protected.items():
        if sha(Path(path)) != digest:
            raise ValueError(f'Protected input changed: {path}')
    assert sha(old / 'head-patch.json') == original['sha256']
    for path in (old / 'recipe.py', old / 'report.json', old / 'head-patch.json', blender):
        protected[str(path)] = sha(path)
    out.mkdir()
    shutil.copyfile(__file__, out / 'recipe.py')
    (out / 'blender-version.txt').write_text(version)
    (out / 'inputs.json').write_text(json.dumps(protected, indent=2) + '\n')
    command = ['bwrap', '--unshare-net', '--ro-bind', '/', '/', '--bind', str(out), str(out),
               '--dev-bind', '/dev', '/dev', '--tmpfs', '/tmp', '--die-with-parent', '--',
               str(blender), '--background', '--factory-startup', '--disable-autoexec',
               '--threads', '2', '--python-exit-code', '1', '--python', str(old / 'recipe.py'), '--',
               '--source', str(AUTHOR / 'authoring/GownEye1.blend'),
               '--candidate', str(AUTHOR / 'work/face-articulation5'),
               '--backing', str(AUTHOR / 'work/face-backing2/backing.json'),
               '--topology', str(ROOT / 'work/cine-animation-port/author-head-source-v1/eve-correspondence-v2.npz'),
               '--output', str(out / 'export')]
    (out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (out / 'blender.log').open('x') as log:
        result = subprocess.run(command, cwd=AUTHOR, stdout=log, stderr=subprocess.STDOUT)
    (out / 'exit.json').write_text(json.dumps({'exit_code': result.returncode}) + '\n')
    result.check_returncode()
    exported = read(out / 'export/report.json')
    assert exported == original
    digest = sha(out / 'export/head-patch.json')
    assert digest == original['sha256']
    for path, expected in protected.items():
        assert sha(Path(path)) == expected, path
    report = dict(passed=True, input_count=len(protected), inputs=protected, head_patch_sha256=digest,
                  controls=exported['controls'], face_vertices=exported['original_face_vertices'],
                  lip_duplicates=exported['added_lip_vertices'], oral_vertices=exported['oral_vertices'],
                  scope='Exact head-patch export from GownEye1.blend and retained articulation/backing data. '
                        'Not a replay of the upstream articulation/backing/donor generation or the whole outfit.')
    (out / 'verified.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'inputs'}))


if __name__ == '__main__':
    main()
