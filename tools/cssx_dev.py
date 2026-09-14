#!/usr/bin/env python3
"""Local CSSX build/probe driver. This tool is excluded from player packages."""
import argparse
import json
import shutil
import subprocess
import time
from pathlib import Path
from css import GAME, ROOT, atomic, copy_verified, processes, sha, wait_json

MOD = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
EVIDENCE = ROOT / 'work/cssx-native'
BUILD = ROOT / 'build/cssx-native'


def stage():
    subprocess.run(['cmake', '-S', str(ROOT/'native'), '-B', str(BUILD), '-G', 'Ninja',
                    '-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'native/toolchain-clang-cl.cmake'),
                    '-DCMAKE_BUILD_TYPE=Release', '-DCSS_INVENTORY_DEV=ON'], check=True)
    subprocess.run(['cmake', '--build', str(BUILD), '-j4', '--target', 'main', 'css_core', 'cssx_core', 'cheat_menu'], check=True)
    contract = json.loads((MOD/'loader-contract.json').read_text())
    if contract['abi'] != 1 or sha(MOD/'dlls/main.dll') != contract['dll_sha256']:
        raise RuntimeError('Installed loader does not match its recorded contract')
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    if any(sha(ROOT/name) != digest for name, digest in contract['sources'].items()):
        if processes():
            raise RuntimeError('Permanent loader sources changed; close the game before updating it')
        backup=EVIDENCE/('loader-backup-'+str(time.time_ns()))
        backup.mkdir()
        copy_verified(MOD/'dlls/main.dll',backup/'main.dll')
        copy_verified(MOD/'loader-contract.json',backup/'loader-contract.json')
        copy_verified(BUILD/'main.dll',MOD/'dlls/main.dll')
        atomic(MOD/'loader-contract.json',{'abi':1,'dll_sha256':sha(MOD/'dlls/main.dll'),
               'sources':{name:sha(ROOT/name) for name in ('native/src/loader.cpp','native/src/api.hpp')}})
    if not (EVIDENCE/'original-core.json').exists():
        shutil.copy2(MOD/'core.json', EVIDENCE/'original-core.json')
    for name in ('css_core', 'cssx_core'):
        source=BUILD/(name+'.dll')
        filename=f'{name}-dev-{sha(source)[:16]}' + (f'-{time.time_ns()}' if name=='css_core' else '') + '.dll'
        destination=MOD/'cores'/filename
        if not destination.exists(): copy_verified(source,destination)
        if name=='css_core': core=filename
        else: atomic(MOD/'cssx.json',{'file':filename})
    target=MOD/'extensions/eins0fx.cheat-menu'
    target.mkdir(parents=True,exist_ok=True)
    for name in ('menu.json','LICENSE'):
        copy_verified(ROOT/'extensions/cheat-menu'/name,target/name)
    source=BUILD/'cheat_menu.dll'
    filename=f'cheat_menu-{sha(source)[:16]}.dll'
    if not (target/filename).exists(): copy_verified(source,target/filename)
    manifest=json.loads((ROOT/'extensions/cheat-menu/extension.json').read_text())
    manifest['entry']=filename
    atomic(target/'extension.json',manifest)
    atomic(MOD/'core.json',{'abi':1,'file':core})
    print('Staged',core)
    if processes():
        print(json.dumps(wait_json(MOD/'runtime/loader.json',lambda j:j.get('core')==core),indent=2))
    else:
        print('Game is closed. The staged build will load on launch.')


def command(request,host=False):
    rid=str(time.time_ns())
    atomic(MOD/'request.json',{'id':rid,'action':'cssx_debug','request':request,'host':host})
    result=wait_json(MOD/'runtime/cssx-debug.json',lambda j:j.get('id')==rid)
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    (EVIDENCE/(rid+'-probe.json')).write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=['stage','request','host'])
    parser.add_argument('json',nargs='?',default='{"op":"library"}')
    args=parser.parse_args()
    if args.action=='stage':stage()
    else:print(json.dumps(command(json.loads(args.json),args.action=='host'),indent=2))


if __name__=='__main__':main()
