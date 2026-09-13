#!/usr/bin/env python3
"""Verify or install CSS.Package v1 outfit trios. Python 3.14."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from css import GAME, ROOT, atomic, copy_verified, processes
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest, png_info


def verify(directory: Path, repak: Path = DEFAULT_REPAK) -> dict:
    paks=list(directory.glob('*.pak'))
    if len(paks)!=1:
        raise ValueError(f'Expected one package trio per directory: {directory}')
    pak=paks[0]
    expected={pak.with_suffix(suffix).name for suffix in ('.pak','.utoc','.ucas')}
    if {p.name for p in directory.iterdir()}!=expected:
        raise ValueError(f'Package directory must contain exactly the matching three files: {directory}')
    with tempfile.TemporaryDirectory(prefix='css-verify-') as temporary:
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
        return manifest


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
    if migrate:
        retiring.extend(paks/('CSS_Beaute_P'+s) for s in ('.pak','.utoc','.ucas'))
        retiring.append(mod/'catalog/beaute.css.json')
        retiring.append(ROOT/'catalog/beaute.css.json')
        for name in ('BeauteGenessa_P','BeauteKnightLady_P'):
            retiring.extend(paks/(name+s) for s in ('.pak','.utoc','.ucas'))
    retiring=[p for p in retiring if p.exists()]
    backup=ROOT/'backups'/f'packages-{time.time_ns()}'
    backup.mkdir(parents=True)
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
    parser.add_argument('action',choices=('verify','install'))
    parser.add_argument('packages',nargs='+',type=Path)
    parser.add_argument('--game',type=Path,default=GAME)
    parser.add_argument('--repak',type=Path,default=DEFAULT_REPAK)
    parser.add_argument('--migrate-beaute',action='store_true',help='Back up and retire the old merged Beaute prototype and its loose catalog')
    parser.add_argument('--replace',action='store_true',help='Back up and replace installed packages with the same stable IDs, including renamed packages')
    args=parser.parse_args()
    try:
        if args.action=='install':
            install(args.packages,args.game,args.migrate_beaute,args.repak,args.replace)
        else:
            for directory in args.packages:
                manifest=verify(directory,args.repak)
                print(f"Verified {manifest['name']} by {manifest['author']}: {len(manifest['catalog']['outfits'][0]['variants'])} variant(s), embedded thumbnail, full container hashes")
    except (ValueError,OSError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        parser.exit(1,f'CSS package {args.action} failed: {error}\n')


if __name__=='__main__': main()
