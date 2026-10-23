"""Shared Assets 1.0.4 candidate: released 1.0.3 payloads plus Profane Proxima's astral materials.

Stage = the exact payload set that produced the 1.0.3 utoc (cloth-expanded assembly) plus the 14
cooked ProfaneProxima packages. The five shared dependencies the cook re-emitted stay as released
(their exports are byte-identical). Metadata comes from the 0.2.0 test pak (revision pin + face rig)
with containers and version updated. Run with python3 -I from anywhere.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

CSS = Path('/home/eins0fx/development/mods/msII/CustomShellSystem')
sys.path.insert(0, str(CSS / 'tools'))
from css import GAME  # noqa: E402

W = Path("/run/media/eins0fx/theark/CSS/CustomShellSystem-work/shared-assets-1.0.4-doubles1")
OUT = W / 'build'
BASE_STAGE = Path('/run/media/eins0fx/theark/CSS/CustomShellSystem-work/cloth-expanded-26073152/assembly/CSS_SharedAssets_P/stage')
COOK = Path('/mnt/eins0fxE/CSS-Development/profane-astral1/windows-cook1/project/Saved/Cooked/Windows/CSSShared/Content')
TEST_PAK = Path('/run/media/eins0fx/theark/CSS/CustomShellSystem-work/shared-assets-020-test1/face/trio/CSS_SharedAssets_P.pak')
RETOC = CSS / 'build/retoc-css-target/release/retoc'
REPAK = CSS / 'build/repak/release/repak'
DOTNET = Path('/home/eins0fx/.local/share/mise/installs/dotnet/10.0.401/dotnet')
MAPPINGS = next((CSS / 'docs/development/non-css-modding-guide').glob('*/MortalShell2-5.6.1.usmap'))
NAME = 'CSS_SharedAssets_P'
RELEASED_UTOC = '76fa5b5d60faec2721b4e7d0dc6e8deaa5decc57a622d0c5a38465725baad863'
RETAINED = {'CSS/eins0fx/Shared/Astral/Materials/T_Neutral' + n for n in ('Color', 'Linear', 'Masks', 'Normal')}
RETAINED.add('CSS/eins0fx/Shared/Materials/MF_TrimFlow')
VERSION = '1.0.4'


def sha(path):
    with open(path, 'rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def run(label, args):
    args = [str(a) for a in args]
    (OUT / (label + '-command.json')).write_text(json.dumps(args, indent=1) + '\n')
    env = {**os.environ, 'RAYON_NUM_THREADS': '1', 'DOTNET_ROOT': str(DOTNET.parent)}
    with open(OUT / (label + '.log'), 'w') as log:
        rc = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT, env=env).returncode
    if rc:
        sys.exit(f'{label} failed ({rc}); see {OUT / (label + ".log")}')


def copy_tree(src, dst):
    for p in sorted(src.rglob('*')):
        if p.is_file():
            t = dst / p.relative_to(src)
            t.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, t)


def mount_with(folder, trio):
    folder.mkdir()
    src = GAME / 'Content/Paks'
    for p in sorted(src.iterdir()):
        if p.suffix in ('.pak', '.utoc', '.ucas') and (p.name.startswith('pakchunk') or p.stem == 'global'):
            (folder / p.name).symlink_to(p)
    for p in trio.iterdir():
        (folder / p.name).symlink_to(p.resolve())


def main():
    OUT.mkdir(exist_ok=False)
    inputs = {str(p): sha(p) for p in (TEST_PAK, Path(__file__))}

    # 1. Prove the baseline stage still produces the released utoc.
    base = OUT / 'baseline-check'
    base.mkdir()
    run('baseline-to-zen', [RETOC, 'to-zen', BASE_STAGE, base / (NAME + '.utoc'), '--version', 'UE5_6', '--no-parallel'])
    if sha(base / (NAME + '.utoc')) != RELEASED_UTOC:
        sys.exit('baseline stage no longer reproduces the released 1.0.3 utoc')

    # 2. Stage baseline + new packages.
    stage = OUT / 'stage'
    copy_tree(BASE_STAGE, stage)
    content = stage / 'MortalShell2/Content'
    added = {}
    for p in sorted(COOK.rglob('*')):
        if not p.is_file():
            continue
        local = p.relative_to(COOK)
        package = str(local.with_suffix(''))
        if package in RETAINED:
            released = content / local
            if local.suffix != '.uasset' and sha(released) != sha(p):
                sys.exit(f'cooked dependency differs from release: {local}')
            continue
        if not local.name.startswith(('ProfaneProxima_', 'MI_ProfaneProxima_')) or local.parent != Path('CSS/eins0fx/Shared/Astral/Materials'):
            sys.exit(f'unexpected cooked package: {local}')
        target = content / local
        if target.exists():
            sys.exit(f'would replace a released payload: {local}')
        shutil.copy2(p, target)
        added[str(Path('MortalShell2/Content') / local)] = sha(p)
    packages = sorted({'/Game/' + str(Path(r).relative_to('MortalShell2/Content').with_suffix('')) for r in added})
    if len(packages) != 14:
        sys.exit(f'expected 14 new packages, got {len(packages)}')

    # 3. Containers.
    trio = OUT / 'trio'
    trio.mkdir()
    run('to-zen', [RETOC, 'to-zen', stage, trio / (NAME + '.utoc'), '--version', 'UE5_6', '--no-parallel'])
    run('verify', [RETOC, 'verify', trio / (NAME + '.utoc')])

    # 4. Metadata from the 0.2.0 test pak.
    meta = OUT / 'metadata'
    info = subprocess.run([str(REPAK), 'info', str(TEST_PAK)], capture_output=True, text=True, check=True).stdout
    mount_point = next(l.split(': ', 1)[1] for l in info.splitlines() if l.startswith('mount point: '))
    run('pak-unpack', [REPAK, 'unpack', TEST_PAK, '--output', meta])
    manifest_path, = meta.rglob('manifest.json')
    manifest = json.loads(manifest_path.read_bytes())
    manifest['version'] = VERSION
    manifest['catalog']['animation_library']['version'] = VERSION
    manifest['containers'] = {s: dict(file=NAME + s, bytes=(trio / (NAME + s)).stat().st_size, sha256=sha(trio / (NAME + s)))
                              for s in ('.utoc', '.ucas')}
    manifest_path.write_text(json.dumps(manifest, separators=(',', ':')) + '\n')
    run('pak-pack', [REPAK, 'pack', meta, trio / (NAME + '.pak'), '--version', 'V8B', '--mount-point', mount_point])

    # 5. Roundtrip, decode with shader maps, import closure.
    mount = OUT / 'containers'
    mount_with(mount, trio)
    filters = [v for p in packages for v in ('-f', '/Content/' + p[6:])]
    run('roundtrip', [RETOC, 'to-legacy', mount, OUT / 'roundtrip', '--version', 'UE5_6', '--no-parallel', '--no-shaders', *filters])
    for rel, digest in added.items():
        if not rel.endswith('.uasset') and sha(OUT / 'roundtrip' / rel) != digest:
            sys.exit(f'roundtrip changed {rel}')
    (OUT / 'packages.txt').write_text('\n'.join(packages) + '\n')
    (OUT / 'references.txt').write_text('\n'.join(p + '.' + p.rsplit('/', 1)[1] for p in packages) + '\n')
    run('decode', [DOTNET, CSS / 'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll', mount, MAPPINGS,
                   OUT / 'packages.txt', OUT / 'decoded.json', '--shader-maps'])
    run('imports', [DOTNET, CSS / 'tools/AssetDependencyAudit/bin/Release/net10.0/AssetDependencyAudit.dll', mount, MAPPINGS,
                    OUT / 'packages.txt', OUT / 'references.txt', OUT / 'import-check.json'])
    decoded = json.loads((OUT / 'decoded.json').read_text())
    if set(decoded) != set(packages) or not json.loads((OUT / 'import-check.json').read_text())['passed']:
        sys.exit('exports or imports did not resolve')
    for path, digest in inputs.items():
        if sha(path) != digest:
            sys.exit(f'input changed: {path}')
    receipt = dict(passed=True, installed=False, version=VERSION, added_packages=packages, added_payloads=added,
                   inputs=inputs, files={p.name: dict(bytes=p.stat().st_size, sha256=sha(p)) for p in sorted(trio.iterdir())})
    (OUT / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt['files'], indent=1))


if __name__ == '__main__':
    main()
