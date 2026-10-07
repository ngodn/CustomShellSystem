"""Replace only CW's shared facial mesh and add its verified optical materials."""
import argparse
import json
from pathlib import Path
import shutil

from package_corrected_dance_trial import ROOT, STEM, GAME, PACKAGE_ROOT, DEFAULT_REPAK, Converter, sha, read


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=Path)
    parser.add_argument('mesh_cook', type=Path)
    parser.add_argument('material_cook', type=Path)
    parser.add_argument('audit', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    base, mesh_cook, material_cook, audit_path = [p.resolve(strict=True) for p in
        (args.base, args.mesh_cook, args.material_cook, args.audit)]
    out = args.output.resolve()
    if out.exists() or not out.is_relative_to(Path('/mnt/eins0fxE/CSS-authoring-scratch')):
        raise ValueError('Use a new private secondary-drive output')
    audit = read(audit_path)
    baseline = read(base/'verification.json')
    if not audit.get('passed') or not baseline.get('passed') or baseline['entries'] != 63:
        raise ValueError('Unverified inputs')
    protected = {str(audit_path): sha(audit_path), str(base/'verification.json'): sha(base/'verification.json')}
    for group in (audit['protected'], audit['assets'], baseline['dependencies']):
        protected.update(group)
    for name, digest in baseline['files'].items():
        protected[str(base/STEM/name)] = digest
    if any(sha(Path(p)) != digest for p, digest in protected.items()):
        raise ValueError('An audited input changed')
    old = audit['source'].removeprefix('/Game/')
    new = audit['target'].removeprefix('/Game/')
    payloads = {p.relative_to(base/'stage'): p for p in (base/'stage').rglob('*') if p.is_file()}
    if len(payloads) != baseline['exports']*2:
        raise ValueError('Unexpected baseline export files')
    for suffix in ('.uasset', '.uexp'):
        del payloads[Path('MortalShell2/Content')/(old+suffix)]
        payloads[Path('MortalShell2/Content')/(new+suffix)] = mesh_cook/(new+suffix)
    optical = material_cook/'CSS/Locomotion/Faces/Optical1'
    files = list(optical.glob('*'))
    if len(files) != 8 or {p.stem for p in files} != {'M_Optical', 'MI_Optical_8', 'MI_Optical_10', 'MI_Optical_45'}:
        raise ValueError('Expected exactly four cooked optical assets')
    for source in files:
        if source.suffix not in ('.uasset', '.uexp'):
            raise ValueError('Unexpected optical cook output')
        payloads[Path('MortalShell2/Content/CSS/Locomotion/Faces/Optical1')/source.name] = source
    for source in payloads.values():
        protected[str(source)] = sha(source)
    out.mkdir()
    for relative, source in payloads.items():
        dest = out/'stage'/relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc', DEFAULT_REPAK, out)
    trio = out/STEM
    trio.mkdir()
    converter.run(converter.retoc, 'to-zen', out/'stage', trio/(STEM+'.utoc'), '--version', 'UE5_6', '--no-parallel')
    converter.run(converter.retoc, 'verify', trio/(STEM+'.utoc'))
    mounts = out/'containers'
    converter.base_containers(GAME, mounts)
    for p in [*map(Path, baseline['dependencies']), *trio.iterdir()]:
        (mounts/p.name).symlink_to(p)
    converter.run(converter.retoc, 'to-legacy', mounts, out/'readback', '--version', 'UE5_6',
                  '--no-parallel', '--no-shaders', '-f', '/CSS/Locomotion/')
    for relative, source in payloads.items():
        if source.suffix == '.uexp' and sha(out/'readback'/relative) != sha(source):
            raise ValueError('Changed export payload: '+str(relative))
    shutil.copytree(base/'metadata', out/'metadata')
    path = out/'metadata'/PACKAGE_ROOT/'css.locomotion/manifest.json'
    manifest = read(path)
    rigs = manifest['catalog']['animation_library']['faces']
    changed = 0
    for rig in rigs:
        for adapter in rig['adapters']:
            if adapter['target_mesh'] == audit['source']+'.'+Path(old).name:
                if adapter['source_outfit'] != 'eins0fx.commanderwhite':
                    raise ValueError('Unexpected optical adapter owner')
                adapter['target_mesh'] = audit['target']+'.'+Path(new).name
                changed += 1
    if changed != 1:
        raise ValueError('Expected exactly one CW facial adapter')
    manifest['containers'] = {suffix: {'file': STEM+suffix, 'bytes': (trio/(STEM+suffix)).stat().st_size,
        'sha256': sha(trio/(STEM+suffix))} for suffix in ('.utoc', '.ucas')}
    path.write_text(json.dumps(manifest, indent=2)+'\n')
    converter.run(DEFAULT_REPAK, 'pack', out/'metadata', trio/(STEM+'.pak'), '--version', 'V8B')
    converter.run(DEFAULT_REPAK, 'unpack', trio/(STEM+'.pak'), '--output', out/'metadata-readback')
    if read(out/'metadata-readback'/PACKAGE_ROOT/'css.locomotion/manifest.json') != manifest:
        raise ValueError('Metadata roundtrip changed')
    if any(sha(Path(p)) != digest for p, digest in protected.items()):
        raise ValueError('Protected input changed')
    report = {'passed': True, 'entries': 63, 'exports': len(payloads)//2,
              'unchanged_existing_exports': baseline['exports']-1,
              'base_verification': str(base/'verification.json'),
              'replacements': {audit['source']: audit['target']}, 'added_auxiliary_exports': 4,
              'protected': protected, 'dependencies': baseline['dependencies'],
              'files': {p.name: sha(p) for p in trio.iterdir()},
              'independent_material_readback_pending': True, 'installed': False}
    (out/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    (out/'optical-packages.txt').write_text('\n'.join('/Game/CSS/Locomotion/Faces/Optical1/'+name
        for name in ('M_Optical', 'MI_Optical_8', 'MI_Optical_10', 'MI_Optical_45'))+'\n')
    print('CW_OPTICAL_PACK_ROUNDTRIP_PASS', flush=True)


if __name__ == '__main__':
    main()
