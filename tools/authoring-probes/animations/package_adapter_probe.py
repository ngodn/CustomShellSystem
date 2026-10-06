"""Package one verified playback adapter and compare its cooked graph to the outfit."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from verify_cooked_performance_layers import verify

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('--authoring-readback', type=Path, required=True)
    parser.add_argument('--dependency', type=Path, required=True)
    parser.add_argument('--source-outfit', required=True)
    parser.add_argument('--readback-tool', type=Path, required=True)
    parser.add_argument('--dotnet', type=Path, required=True)
    parser.add_argument('--mappings', type=Path, required=True)
    args = parser.parse_args()
    work = args.work.resolve(strict=True)
    if not work.is_relative_to(ROOT / 'work/cine-animation-port'):
        raise ValueError('Use the private animation workspace')
    if json.loads((work / 'cook-exit.json').read_text())['exit_code'] != 0:
        raise ValueError('Adapter cook failed')
    authoring = json.loads(args.authoring_readback.read_text())
    if not authoring['complete'] or not authoring['verification'].startswith('PASS:'):
        raise ValueError('Fresh authored graph comparison failed')
    package, source = authoring['output'], authoring['source']
    if (not package.startswith('/Game/CSS/Locomotion/Adapters/ABP_') or '.' in package or
            '..' in package or (work / 'cook.txt').read_text().splitlines() != [package]):
        raise ValueError('Expected one verified shared adapter package')
    toc = args.dependency.resolve(strict=True)
    if toc.suffix != '.utoc':
        raise ValueError('Expected an existing outfit container')
    protected = dict(authoring['protected'])
    original = ROOT.parent / 'CSS-eins0fx-collections/tools/CSSAuthoring/Content' / (
        package.removeprefix('/Game/') + '.uasset')
    protected[str(original)] = authoring['output_sha256']
    for path in (args.authoring_readback.resolve(), toc, toc.with_suffix('.ucas')):
        protected[str(path)] = digest(path)
    if any(digest(Path(p)) != sha for p, sha in protected.items()):
        raise ValueError('Protected authoring or dependency input changed')
    out = work / 'probe'
    out.mkdir(exist_ok=False)
    converter = Converter(ROOT / 'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    cooked = {}
    for suffix in ('.uasset', '.uexp'):
        relative = package.removeprefix('/Game/') + suffix
        path = work / 'cooked/CSSAuthoring/Content' / relative
        target = out / 'stage/MortalShell2/Content' / relative
        if suffix == '.uasset':
            info = asset_info(path.read_bytes())
            if info['package'] != package or not any(e['class'] == 'AnimBlueprintGeneratedClass' for e in info['exports']):
                raise ValueError('Unexpected cooked adapter')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        cooked[str(target.relative_to(out / 'stage'))] = digest(path)
    container = out / 'CSS_PlaybackAdapterProbe_P.utoc'
    converter.run(converter.retoc, 'to-zen', out / 'stage', container, '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', container)
    mounts = out / 'containers'
    converter.base_containers(GAME, mounts)
    for path in (toc, toc.with_suffix('.ucas'), container, container.with_suffix('.ucas')):
        (mounts / path.name).symlink_to(path)
    converter.run(converter.retoc, 'to-legacy', mounts, out / 'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', package.removeprefix('/Game'))
    for relative, checksum in cooked.items():
        path = out / 'readback' / relative
        if path.suffix == '.uexp':
            if digest(path) != checksum:
                raise ValueError('Cooked graph export changed in the container')
        else:
            asset_info(path.read_bytes())
    listing = out / 'readback.txt'
    listing.write_text(source + '\n' + package + '\n')
    command = [str(p.resolve(strict=True)) for p in (args.dotnet, args.readback_tool, mounts,
                                                    args.mappings, listing)] + [str(out / 'graphs.json')]
    (out / 'readback-command.json').write_text(json.dumps(command, indent=2) + '\n')
    with (out / 'readback.log').open('x') as stream:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
    (out / 'readback-exit.json').write_text(json.dumps({'exit_code': result.returncode}) + '\n')
    if result.returncode:
        raise ValueError('Cooked graph decoding failed')
    graphs = json.loads((out / 'graphs.json').read_text())
    skeleton, controls = verify(graphs[source], graphs[package])
    if any(digest(Path(p)) != sha for p, sha in protected.items()):
        raise ValueError('Protected input changed while packaging')
    receipt = out / 'receipt.json'
    receipt.write_text(json.dumps(dict(protected=protected, cooked=cooked, source_toc=str(toc),
        containers={str(p): digest(p) for p in (container, container.with_suffix('.ucas'))},
        installed=False), indent=2) + '\n')
    binding = dict(source_outfit=args.source_outfit,
        source_class=source + '.' + source.rsplit('/', 1)[-1] + '_C',
        target_class=package + '.' + package.rsplit('/', 1)[-1] + '_C', skeleton=skeleton,
        source_toc_sha256=digest(toc), source_bulk_bytes=toc.with_suffix('.ucas').stat().st_size)
    (out / 'verified-binding.json').write_text(json.dumps(dict(passed=True,
        contract='explicit-weight-v1', adapters=[binding], preserved_cooked_css_defaults=controls,
        evidence={str(p.resolve()): digest(p) for p in (out / 'graphs.json', receipt, args.authoring_readback)},
        scope='Cooked graph preservation and structural container check. Full clip coverage and live acceptance remain required.'), indent=2) + '\n')
    print('COOKED_ADAPTER_VERIFIED', package, controls, flush=True)


if __name__ == '__main__':
    main()
