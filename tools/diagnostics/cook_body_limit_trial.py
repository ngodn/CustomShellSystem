"""Windows-cook the verified Eve body rig using the retained UE 5.6.1 toolchain."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


root = Path(__file__).resolve().parents[2]
stage = Path(sys.argv[1]).resolve(strict=True)
proof = json.loads((stage/'verified.json').read_text())
asset = stage/'editor/Content/CSS/eins0fx/EveSB/CR_Body.uasset'
if not proof['passed'] or sha(asset) != proof['candidate_sha256']:
    raise ValueError('Candidate verification is missing or stale')
if not stage.is_relative_to((root/'work').resolve()):
    raise ValueError('Expected a private CSS work directory')
output = stage/'windows-cook1'
if output.exists() or shutil.disk_usage(stage).free < 8*1024**3:
    raise ValueError('Need a new cook directory and 8 GiB free')
ug = root.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P'
engine, wrapper = ug/'work/ue56', ug/'tools/docker-local.sh'
version = json.loads((engine/'Engine/Build/Build.version').read_text())
if tuple(version[k] for k in ('MajorVersion', 'MinorVersion', 'PatchVersion', 'Changelist')) != (5, 6, 1, 44394996):
    raise ValueError('Wrong Windows engine build')
image = subprocess.check_output(['bash', str(wrapper), 'image', 'inspect', 'unholy-wine:11.7',
                                 '--format', '{{.Id}}'], text=True).strip()
project, out = output/'project', output/'out'
project.mkdir(parents=True)
out.mkdir()
baseline = Path('/mnt/eins0fxE/CSS-Development/peony-v2-character2/windows-cook1/project')
shutil.copy2(baseline/'CSSShared.uproject', project/'CSSShared.uproject')
shutil.copytree(baseline/'Config', project/'Config')
target = project/'Content/CSS/eins0fx/EveSB/CR_Body.uasset'
target.parent.mkdir(parents=True)
shutil.copy2(asset, target)
script = root/'tools/shared-assets/cook-shared-inner.sh'
protected = {str(path): sha(path) for path in
             [asset, stage/'verified.json', script, Path(__file__),
              baseline/'CSSShared.uproject', *sorted((baseline/'Config').glob('*.ini'))]}
command = ['bash', str(wrapper), 'run', '--rm', '--init', '--user', '0:0',
           '--name', 'css-eve-body-limits1', '--network', 'none', '--shm-size', '1g',
           '--mount', f'type=bind,src={engine.resolve()},dst=/ue,readonly',
           '--mount', f'type=bind,src={project},dst=/project',
           '--mount', f'type=bind,src={out},dst=/out',
           '--mount', f'type=bind,src={script},dst=/cook.sh,readonly', image, 'bash', '/cook.sh']
(output/'inputs.json').write_text(json.dumps(dict(protected=protected, image=image, engine=version,
                                                command=command), indent=2)+'\n')
with (output/'console.log').open('w') as log:
    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
unchanged = all(sha(Path(path)) == digest for path, digest in protected.items())
(output/'exit.json').write_text(json.dumps(dict(returncode=result.returncode,
                                               sources_unchanged=unchanged))+'\n')
result.check_returncode()
if not unchanged:
    raise ValueError('Protected input changed')
print('Windows cook exited successfully; cooked defaults and package still require readback')
