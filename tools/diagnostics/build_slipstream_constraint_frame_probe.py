"""Extend the retained C++20 probe to inspect a chosen captured cloth frame."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

from run_slipstream_constraint_isolation import ROOT, EVE, sha

OUT = ROOT / 'work/three-outfit-rig-physics/slipstream-constraint-frame1'
SOURCE = EVE / 'work/slipstream-cloth-native13'
ENGINE = ROOT.parent / 'CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume-build', action='store_true')
    args = parser.parse_args()
    if not args.resume_build:
        OUT.mkdir(exist_ok=False)
    editor = OUT / 'editor'
    if args.resume_build:
        old = json.loads((OUT / 'build.exit.json').read_text())
        assert old['returncode'] != 0 and not old['changed_source_files']
        for name in ('recipe.py', 'build.log', 'build.exit.json'):
            shutil.copy2(OUT / name, OUT / ('first-' + name))
    else:
        editor.mkdir()
        for name in ('Source', 'Config'):
            shutil.copytree(SOURCE / 'editor' / name, editor / name)
        shutil.copy2(SOURCE / 'editor/CSSAuthoring.uproject', editor / 'CSSAuthoring.uproject')
    protected = {str(p): sha(p) for p in (SOURCE / 'editor/Source').rglob('*') if p.is_file()}
    source = editor / 'Source/CSSAuthoring/CSSEveEmbeddedMotion.inl'
    original = (SOURCE / 'editor/Source/CSSAuthoring/CSSEveEmbeddedMotion.inl').read_text()
    needle = '    if (ContactTrace && (ContactFrame<0 || ContactFrame>=FrameCount))'
    addition = '''
    int32 ConstraintFrame=38;
    if (FParse::Value(*Params,TEXT("ConstraintFrame="),ConstraintFrame) &&
        (!CollisionTrace || ConstraintFrame<0 || ConstraintFrame>=FrameCount))
        return Fail(TEXT("ConstraintFrame requires CollisionTrace and a captured frame"));
'''
    old_condition = 'if (CollisionTrace && (Frame==0 || Frame==38))'
    assert original.count(needle) == original.count(old_condition) == 1
    edited = original.replace(needle, addition + needle).replace(old_condition,
        'if (CollisionTrace && (Frame==0 || Frame==ConstraintFrame))')
    source.write_text(edited)
    for name in ('ubt-config', 'ubt-cache', 'dotnet', 'ubt-runtime-scratch', 'tmp'):
        (OUT / name).mkdir(exist_ok=args.resume_build)
    env = dict(os.environ, TMPDIR=str(OUT / 'tmp'), DOTNET_CLI_HOME=str(OUT / 'dotnet'),
               XDG_CONFIG_HOME=str(OUT / 'ubt-config'), XDG_CACHE_HOME=str(OUT / 'ubt-cache'))
    command = ['nice', '-n', '19', 'bwrap', '--ro-bind', '/', '/', '--bind', str(OUT), str(OUT),
        '--bind', str(OUT / 'ubt-runtime-scratch'), '/tmp', '--dev-bind', '/dev', '/dev', '--die-with-parent', '--',
        str(ENGINE / 'Binaries/ThirdParty/DotNet/8.0.300/linux-x64/dotnet'),
        str(ENGINE / 'Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'),
        'CSSAuthoringEditor', 'Linux', 'Development', '-Project=' + str(editor / 'CSSAuthoring.uproject'),
        '-NoHotReloadFromIDE', '-NoUBA', '-MaxParallelActions=2', '-Log=' + str(OUT / 'build.engine.log')]
    shutil.copy2(Path(__file__), OUT / 'recipe.py')
    with (OUT / 'build.log').open('w') as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT)
    changed = [p for p, h in protected.items() if sha(Path(p)) != h]
    (OUT / 'build.exit.json').write_text(json.dumps(dict(command=command, returncode=result.returncode,
        source_before_sha256=sha(SOURCE / 'editor/Source/CSSAuthoring/CSSEveEmbeddedMotion.inl'),
        source_after_sha256=sha(source), protected=protected, changed_source_files=changed,
        recipe_sha256=sha(Path(__file__))), indent=2) + '\n')
    if result.returncode or changed:
        raise RuntimeError('Probe build failed or source changed')
    names = ('CSSAuthoring.uproject', 'Binaries/Linux/UnrealEditor.modules', 'Binaries/Linux/libUnrealEditor-CSSAuthoring.so')
    (OUT / 'module.json').write_text(json.dumps({name: sha(editor / name) for name in names}, indent=2) + '\n')
    print('Built frame-selectable private probe', OUT, flush=True)


if __name__ == '__main__':
    main()
