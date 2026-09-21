#!/usr/bin/env python3
"""Verify or install CSS.Package v1 outfit trios. Python 3.14."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

from css import GAME, ROOT, atomic, copy_verified, processes
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest, png_info
from css_paths import new_directory, temporary_directory


def verify(directory: Path, repak: Path = DEFAULT_REPAK) -> dict:
    paks=list(directory.glob('*.pak'))
    if len(paks)!=1:
        raise ValueError(f'Expected one package trio per directory: {directory}')
    pak=paks[0]
    expected={pak.with_suffix(suffix).name for suffix in ('.pak','.utoc','.ucas')}
    if {p.name for p in directory.iterdir()}!=expected:
        raise ValueError(f'Package directory must contain exactly the matching three files: {directory}')
    with temporary_directory(ROOT/'work/tmp', 'verify') as temporary:
        unpacked=Path(temporary)
        subprocess.run([str(repak),'unpack',str(pak),'--output',str(unpacked)],check=True,stdout=subprocess.DEVNULL)
        manifests=list((unpacked/PACKAGE_ROOT).glob('*/manifest.json'))
        if len(manifests)!=1:
            raise ValueError('Expected one embedded CSS manifest')
        source=manifests[0]
        manifest=json.loads(source.read_text())
        if (manifest['format'],manifest['format_version'],manifest['game'],manifest['engine'])!=('CSS.Package',1,'MortalShell2','5.6'):
            raise ValueError('Unsupported CSS package format')
        if source.parent.name!=manifest['id']:
            raise ValueError('Package identity mismatch')
        if png_info(source.parent/'thumbnail.png')!=manifest['thumbnail']:
            raise ValueError('Thumbnail checksum or dimensions do not match')
        for suffix in ('.utoc','.ucas'):
            file=pak.with_suffix(suffix)
            if manifest['containers'][suffix]!={'file':file.name,'bytes':file.stat().st_size,'sha256':digest(file)}:
                raise ValueError(f'Container checksum mismatch: {file.name}')
        outfits=manifest['catalog']['outfits']
        if len(outfits)!=1 or any(outfits[0][key]!=manifest[key] for key in ('id','name','author')):
            raise ValueError('Catalog identity mismatch')
        # The runtime reads the catalog entry's own thumbnail, not the manifest's, and
        # rejects a package without it. Verifying only the manifest let a package through
        # here and get refused in game, which is the wrong order to find out.
        if outfits[0].get('thumbnail')!='thumbnail.png':
            raise ValueError('Catalog outfit must name thumbnail.png')
        from css_controls import verify_resources
        verify_resources(manifest,source.parent)
        return manifest


def release_zip(directory: Path, output: Path, repak: Path = DEFAULT_REPAK) -> Path:
    """Snapshot and verify the trio before writing an install-ready ZIP."""
    if output.suffix.lower() != '.zip':
        raise ValueError('Release output must end in .zip')
    if output.exists():
        raise FileExistsError(output)
    verify(directory, repak)
    pak = next(directory.glob('*.pak'))
    stem = pak.stem
    from css_convert import output_name
    # Reuse the converter's portable filename rules, without renaming containers.
    if output_name('unused', 'unused', stem) != stem:
        raise ValueError('Release containers must have a portable name ending in _P')
    if any(p.is_symlink() for p in directory.iterdir()):
        raise ValueError('Release package members must be regular files, not symlinks')
    output.parent.mkdir(parents=True, exist_ok=True)
    with temporary_directory(output.parent, 'release') as temporary:
        root = Path(temporary)
        snapshot = root / stem
        shutil.copytree(directory, snapshot)
        verify(snapshot, repak)
        staged = root / 'release.zip'
        with zipfile.ZipFile(staged, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for source in sorted(snapshot.iterdir()):
                archive.write(source, f'{stem}/{source.name}')
        # Read back the archive, including bulk data, before publishing anything.
        with zipfile.ZipFile(staged) as archive:
            expected = {f'{stem}/{p.name}' for p in snapshot.iterdir()}
            if set(archive.namelist()) != expected or archive.testzip() is not None:
                raise ValueError('Release ZIP did not round-trip')
            extracted = root / 'readback'
            archive.extractall(extracted)
        verify(extracted / stem, repak)
        for source in snapshot.iterdir():
            if digest(source) != digest(extracted / stem / source.name):
                raise ValueError('Release ZIP changed package bytes')
        # Exclusive creation preserves an earlier release, even if created meanwhile.
        with output.open('xb') as target:
            try:
                with staged.open('rb') as source:
                    shutil.copyfileobj(source, target)
            except BaseException:
                target.close()
                output.unlink()
                raise
    return output


def install(directories: list[Path], game: Path, migrate: bool, repak: Path, replace: bool = False) -> None:
    manifests=[verify(directory,repak) for directory in directories]
    ids=[m['id'] for m in manifests]
    if len(set(ids))!=len(ids):
        raise ValueError('Duplicate outfit IDs in installation request')
    if migrate and not {'beaute.genessa','beaute.knightlady'}.issubset(ids):
        raise ValueError('Beaute migration requires both replacement packages together')
    if processes():
        raise RuntimeError('Close Mortal Shell II before installing new mounted containers. Native core updates can use live reload.')
    paks=game/'Content/Paks/~mods'
    mod=game/'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    if not (mod/'dlls/main.dll').is_file():
        raise ValueError('Install the CSS native runtime before its outfit packages')
    targets=[paks/d.name for d in directories]
    retiring=[]
    retired_directories=[]
    for directory in sorted(paks.iterdir()) if paks.is_dir() else []:
        if not directory.is_dir() or len(list(directory.glob('*.pak')))!=1:
            continue
        try:
            old=verify(directory,repak)
        except (ValueError,KeyError,subprocess.CalledProcessError):
            continue
        if old['id'] in ids:
            if not replace:
                raise FileExistsError(f"Outfit {old['id']} is already installed. Use --replace to back up and replace it.")
            retiring.extend(directory.iterdir())
            retired_directories.append(directory)
    if any(t.exists() and t not in retired_directories for t in targets):
        raise FileExistsError('A target directory belongs to another package or contains unrecognized files')
    from css_controls import block, validate
    retired_masks=set(); retained_masks=set()
    for file in sorted([*(mod/'catalog').glob('*.customize.json'),*(mod/'catalog').glob('*.colors.json')]):
        recipe=json.loads(file.read_text())
        masks={file.parent/name for name in validate(block(recipe))}
        if recipe.get('id') in ids:
            retiring.append(file); retired_masks.update(masks)
        else:
            retained_masks.update(masks)
    retiring.extend(sorted(retired_masks-retained_masks))
    if migrate:
        retiring.extend(paks/('CSS_Beaute_P'+s) for s in ('.pak','.utoc','.ucas'))
        retiring.append(mod/'catalog/beaute.css.json')
        retiring.append(ROOT/'catalog/beaute.css.json')
        for name in ('BeauteGenessa_P','BeauteKnightLady_P'):
            retiring.extend(paks/(name+s) for s in ('.pak','.utoc','.ucas'))
    retiring=list(dict.fromkeys(p for p in retiring if p.exists()))
    backup=new_directory(ROOT/'backups', 'packages')
    records=[]
    for index,source in enumerate(retiring):
        saved=backup/f'{index:02d}-{source.name}'
        copy_verified(source,saved)
        records.append({'original':str(source),'backup':str(saved),'sha256':digest(saved)})
    atomic(backup/'manifest.json',{'retired':records,'installed':[str(p) for p in targets]})
    published=[]
    try:
        paks.mkdir(parents=True,exist_ok=True)
        stages=[]
        for source,target in zip(directories,targets):
            # Stage outside Paks so a partial copy can never become mountable.
            stage=backup/('stage-'+target.name)
            shutil.copytree(source,stage)
            verify(stage,repak)
            stages.append(stage)
        for source in retiring: source.unlink()
        for directory in retired_directories: directory.rmdir()
        for stage,target in zip(stages,targets):
            published.append(target)
            shutil.copytree(stage,target)
            verify(target,repak)
        (mod/'catalog').mkdir(parents=True,exist_ok=True)
    except Exception:
        for target in published:
            if target.exists(): shutil.rmtree(target)
        for record in records: copy_verified(Path(record['backup']),Path(record['original']))
        raise
    print(f'Installed {len(targets)} self-contained CSS packages. Backup: {backup}')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('verify','install','zip'))
    parser.add_argument('packages',nargs='+',type=Path)
    parser.add_argument('--game',type=Path,default=GAME)
    parser.add_argument('--repak',type=Path,default=DEFAULT_REPAK)
    parser.add_argument('--migrate-beaute',action='store_true',help='Back up and retire the old merged Beaute prototype and its loose catalog')
    parser.add_argument('--replace',action='store_true',help='Back up and replace installed packages with the same stable IDs, including renamed packages')
    parser.add_argument('--output',type=Path,help='ZIP filename; zip accepts exactly one package')
    args=parser.parse_args()
    try:
        if args.action=='zip':
            if len(args.packages)!=1 or not args.output:
                raise ValueError('zip requires one package and --output FILE.zip')
            result=release_zip(args.packages[0],args.output,args.repak)
            print(f'Verified release ZIP: {result}\nSHA-256: {digest(result)}')
        elif args.output:
            raise ValueError('--output is only used with zip')
        elif args.action=='install':
            install(args.packages,args.game,args.migrate_beaute,args.repak,args.replace)
        else:
            for directory in args.packages:
                manifest=verify(directory,args.repak)
                print(f"Verified {manifest['name']} by {manifest['author']}: {len(manifest['catalog']['outfits'][0]['variants'])} variant(s), embedded thumbnail, full container hashes")
    except (ValueError,OSError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        parser.exit(1,f'CSS package {args.action} failed: {error}\n')


if __name__=='__main__': main()
