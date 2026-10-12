"""Resolve a shared animation pack with each audited outfit installed alone."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, PACKAGE_ROOT, asset_info


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pack',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--dotnet',type=Path,required=True)
    parser.add_argument('--mappings',type=Path,required=True)
    parser.add_argument('--exporter',type=Path,
                        default=ROOT/'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll')
    args=parser.parse_args()
    pack=args.pack.resolve(strict=True)
    output=args.output.resolve()
    if not output.resolve().is_relative_to((ROOT/'work/cine-animation-port').resolve()):
        raise ValueError('Use the private animation workspace')
    verified=json.loads((pack/'verification.json').read_text())
    manifests=list((pack/'metadata-readback'/PACKAGE_ROOT).glob('*/manifest.json'))
    if not verified.get('passed') or len(manifests)!=1:
        raise ValueError('Expected a verified shared package')
    manifest=json.loads(manifests[0].read_text())
    library=manifest['catalog']['animation_library']
    trio=pack/'CSS_LocomotionSharedAssets_P'
    protected={**verified['dependencies'],str(args.mappings.resolve(strict=True)):digest(args.mappings),
               str(manifests[0]):digest(manifests[0]),
               **{str(trio/name):checksum for name,checksum in verified['files'].items()}}
    if any(digest(path)!=checksum for path,checksum in protected.items()):
        raise ValueError('Verified inputs changed')
    output.mkdir(parents=True,exist_ok=False)
    results=[]
    report=output/'report.json'
    report.write_text(json.dumps(dict(complete=False,results=[]))+'\n')
    environment={key:value for key,value in os.environ.items() if not key.startswith('CSS_')}
    exporter=args.exporter.resolve(strict=True)
    protected[str(exporter)]=digest(exporter)
    for filename,checksum in verified['dependencies'].items():
        dependency=Path(filename)
        if dependency.suffix!='.utoc':
            continue
        work=output/dependency.stem
        work.mkdir()
        converter=Converter(ROOT/'build/retoc-css-target/release/retoc',DEFAULT_REPAK,work)
        mount=work/'containers'
        converter.base_containers(GAME,mount)
        discovery=work/'package-discovery'
        discovery.mkdir()
        for stem in (dependency,trio/'CSS_LocomotionSharedAssets_P.utoc'):
            for suffix in ('.pak','.utoc','.ucas'):
                path=stem.with_suffix(suffix).resolve(strict=True)
                (mount/path.name).symlink_to(path)
                (discovery/path.name).symlink_to(path)
                protected[str(path)]=digest(path)
        revision=lambda entry:entry['source_toc_sha256']==checksum and entry['source_bulk_bytes']==dependency.with_suffix('.ucas').stat().st_size
        adapters=[entry for entry in library['adapters'] if revision(entry)]
        faces=[entry for rig in library.get('faces',[]) for entry in rig['adapters'] if revision(entry)]
        if not adapters:
            raise ValueError('No adapter matches the isolated outfit revision')
        sequences={asset['asset'].split('.')[0] for entry in library['entries'] for asset in entry['assets']}
        adapter_packages={entry['target_class'].split('.')[0] for entry in adapters}
        face_packages={entry['target_mesh'].split('.')[0] for entry in faces}
        compression_packages=set(verified.get('compression_settings',[]))
        if any(not package.startswith('/Game/CSS/Locomotion/') or '..' in package
               for package in compression_packages):
            raise ValueError('Unexpected shared compression package')
        required=sequences|adapter_packages|face_packages|compression_packages
        # Distinct clip families may share only the library root with unrelated
        # outfit faces and graphs. Extract their directories, not that root.
        filters={package.rsplit('/',1)[0] for package in sequences}
        filters.update(compression_packages)
        filters.update(adapter_packages)
        if face_packages:
            filters.add(os.path.commonpath(sorted(face_packages)))
        # ABP_Eve also selects its Bikini/Knit siblings, so avoid re-extracting them.
        filters={path for path in filters if not any(path!=other and path.startswith(other) for other in filters)}
        legacy=work/'readback'
        for selection in sorted(filters):
            converter.run(converter.retoc,'to-legacy',mount,legacy,'--version','UE5_6',
                          '--no-parallel','--no-shaders','-f',selection.removeprefix('/Game'))
        headers={asset_info(path.read_bytes())['package']:path for path in legacy.rglob('*.uasset')}
        if set(headers)!=required:
            raise ValueError('Isolated extraction differs from the required package set')
        for package,path in headers.items():
            original=pack/'readback'/path.relative_to(legacy)
            if digest(path.with_suffix('.uexp'))!=digest(original.with_suffix('.uexp')):
                raise ValueError('Isolated export payload differs: '+package)
        packages=work/'packages.txt'
        # Read the local skeleton too, rather than trusting its presence in an index.
        packages.write_text('\n'.join(sorted(required|{'/Game/CSS/Shared/SKEL_Base'}))+'\n')
        command=[str(args.dotnet.resolve(strict=True)),str(exporter),str(mount),
                 str(args.mappings.resolve(strict=True)),str(packages),str(work/'properties.json')]
        (work/'command.json').write_text(json.dumps(command,indent=2)+'\n')
        with (work/'decode.log').open('x') as log:
            completed=subprocess.run(command,env=environment,stdout=log,stderr=subprocess.STDOUT)
        (work/'exit.json').write_text(json.dumps(dict(exit_code=completed.returncode))+'\n')
        if completed.returncode:
            raise RuntimeError('Independent isolated readback failed: '+dependency.stem)
        decoded=json.loads((work/'properties.json').read_text())
        if set(decoded)!=required|{'/Game/CSS/Shared/SKEL_Base'} or any(not row for row in decoded.values()):
            raise ValueError('Independent readback is incomplete')
        skeletons=[entry for entry in decoded['/Game/CSS/Shared/SKEL_Base'] if entry['Type']=='Skeleton']
        if len(skeletons)!=1:
            raise ValueError('Expected one shared skeleton per source outfit')
        skeleton_sha=hashlib.sha256(json.dumps(skeletons[0],sort_keys=True,separators=(',',':')).encode()).hexdigest()
        row=dict(outfit=dependency.stem,source_toc_sha256=checksum,sequences=len(sequences),
                 adapters=len(adapters),faces=len(faces),compression_settings=len(compression_packages),
                 decoded_packages=len(decoded),
                 exported_payloads_unchanged=True,unresolved_header_imports=False,
                 properties_sha256=digest(work/'properties.json'),
                 decoded_skeleton_sha256=skeleton_sha,
                 mount={path.name:str(path.resolve(strict=True)) for path in mount.iterdir()})
        results.append(row)
        report.write_text(json.dumps(dict(complete=False,results=results),indent=2)+'\n')
        print(json.dumps({key:value for key,value in row.items() if key!='mount'}),flush=True)
    expected_outfits={Path(path).stem for path in verified['dependencies'] if Path(path).suffix=='.utoc'}
    if (not expected_outfits or len(results)!=len(expected_outfits) or
            {row['outfit'] for row in results}!=expected_outfits or
            any(digest(path)!=checksum for path,checksum in protected.items())):
        raise ValueError('Incomplete outfit audit or changed protected input')
    if len({row['decoded_skeleton_sha256'] for row in results})!=1:
        raise ValueError('Outfits supply different shared skeletons; compare before acceptance')
    report.write_text(json.dumps(dict(complete=True,results=results,protected=protected,
        scope='Isolated package conversion and independent property readback. Not live playback or recursive soft-asset loading.'),indent=2)+'\n')


if __name__=='__main__':
    main()
