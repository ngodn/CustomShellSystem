"""Package a cloth-preserving pair and audit its source geometry and surface fields."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import numpy as np

from check_import_cooked_mesh import sha

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from css import GAME
from css_convert import Converter, DEFAULT_REPAK, asset_info


def read(path):
    return json.loads(path.read_text())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=('planet','knit','alice'), required=True)
    args = parser.parse_args()
    WORK = (ROOT/'work/three-outfit-rig-physics/eve-surface-20261014'/(args.variant+'-ordered')).resolve()
    cook = WORK.parent/'cloth-transfer'/(args.variant+'-cook')
    if read(cook/'exit.json') != dict(returncode=0,changed_inputs=[]):
        raise ValueError('Bound cook failed')
    plan = read(WORK/'binding-plan.json')
    protected = dict(plan['protected'])
    protected[str(Path(__file__))] = sha(Path(__file__))
    if any(sha(Path(p)) != digest for p,digest in protected.items()):
        raise ValueError('Protected input changed')
    out = WORK.parent/'cloth-transfer'/(args.variant+'-pack')
    out.mkdir(exist_ok=False)
    packages = read(cook/'shipping-allowlist.json')
    if len(packages)!=2 or any(not p.startswith('/Game/CSSAuthoring/EveSurface20261014/SK_'+args.variant.title()) for p in packages):
        raise ValueError('Expected the two private cloth meshes')
    stage, cooked = out/'stage', cook/'project/Saved/Cooked/Windows/CSSShared/Content'
    payloads = {}
    for package in packages:
        file = cooked/(package.removeprefix('/Game/')+'.uasset')
        if asset_info(file.read_bytes())['package'] != package:
            raise ValueError('Cooked package identity differs')
        for path in file.parent.glob(file.stem+'.*'):
            if path.suffix not in ('.uasset','.uexp','.ubulk','.uptnl'):
                raise ValueError('Unexpected payload')
            relative = Path('MortalShell2/Content')/path.relative_to(cooked)
            destination = stage/relative
            destination.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(path,destination)
            payloads[str(relative)] = sha(path)
    converter = Converter(ROOT/'build/retoc-css-target/release/retoc',DEFAULT_REPAK,out)
    container = out/'container'
    container.mkdir()
    toc = container/'CSS_EveClothPairTrial_P.utoc'
    converter.run(converter.retoc,'to-zen',stage,toc,'--version','UE5_6','--no-parallel')
    converter.run(converter.retoc,'verify',toc)
    mount = out/'containers'
    converter.base_containers(GAME,mount)
    for p in protected:
        path = Path(p)
        if path.suffix in ('.utoc','.ucas'):
            (mount/path.name).symlink_to(path)
    for path in container.iterdir():
        (mount/path.name).symlink_to(path)
        protected[str(path)] = sha(path)
    filters = [v for p in packages for v in ('-f',p.removeprefix('/Game'))]
    converter.run(converter.retoc,'to-legacy',mount,out/'roundtrip','--version','UE5_6',
                  '--no-parallel','--no-shaders',*filters)
    for relative,digest in payloads.items():
        path = out/'roundtrip'/relative
        if path.suffix == '.uasset':
            if asset_info(path.read_bytes())['package'] != asset_info((stage/relative).read_bytes())['package']:
                raise ValueError('Roundtrip changed package identity')
        elif sha(path) != digest:
            raise ValueError('Roundtrip changed export/bulk data')
    (out/'roundtrip.json').write_text(json.dumps(dict(passed=True,packages=packages,payloads=payloads),indent=2)+'\n')
    old = read(WORK/'pack/skin-command.json')['command']
    dotnet, reader, mappings = old[0],old[1],old[3]
    asset_reader = ROOT/'tools/AssetReadback/bin/Release/net10.0/AssetReadback.dll'
    for path in (Path(reader),Path(mappings),asset_reader,converter.retoc):
        protected[str(path)] = sha(path)
    source = Path(read(WORK/'candidate.mesh.receipt.json')['source'])
    facial_source = read(source)
    expressions = read(Path(plan['facial_input']))
    points = expressions.get('point_indices',list(range(len(expressions['body_points']))))
    distance = np.linalg.norm(np.asarray(facial_source['points'])[points]-np.asarray(expressions['body_points']),axis=1)
    if distance.max() > .0002 or len(expressions['morph_targets']) != 115:
        raise ValueError('Facial source correspondence differs')
    facial_source['morph_targets'].extend(dict(name=r['name'],deltas=[[points[d[0]],*d[1:]] for d in r['deltas']])
                                          for r in expressions['morph_targets'])
    expected_face = out/'expected-facial.mesh.json'
    expected_face.write_text(json.dumps(facial_source,separators=(',',':'),allow_nan=False))
    del facial_source
    protected[str(source)] = sha(source)
    protected[str(expected_face)] = sha(expected_face)
    results = []
    def run(label,command,env=None):
        env = env or {}
        (out/(label+'-command.json')).write_text(json.dumps(dict(command=command,environment=env),indent=2)+'\n')
        with (out/(label+'.log')).open('w') as log:
            p = subprocess.run(command,env={**os.environ,**env},stdout=log,stderr=subprocess.STDOUT)
        row = dict(mode=label,returncode=p.returncode)
        results.append(row)
        (out/(label+'-exit.json')).write_text(json.dumps(row)+'\n')
        print(row,flush=True)
        p.check_returncode()
    listing = out/'meshes.txt'
    listing.write_text('\n'.join(packages)+'\n')
    run('decoded',[dotnet,str(asset_reader),str(mount),mappings,str(listing),str(out/'meshes.json'),'--mesh-surface-digests'])
    for name,package,expected in zip(('normal','facial'),packages,(source,expected_face)):
        folder = out/name
        folder.mkdir()
        obj = package+'.'+package.rsplit('/',1)[1]
        for mode,env in (('skin',{'CSS_SKIN_AUDIT':'1','CSS_SKIN_SURFACE':'1'}),('morph',{'CSS_MORPH_AUDIT':'1'})):
            run(name+'-'+mode,[dotnet,reader,str(mount),mappings,obj,str(folder/mode)],env)
        run(name+'-geometry',[sys.executable,'-B',str(ROOT/'tools/diagnostics/check_import_cooked_mesh.py'),
            '--source',str(expected),'--skin',str(folder/'skin'),'--morph',str(folder/'morph'),'--output',str(folder/'geometry.json')])
        run(name+'-surface',[sys.executable,'-B',str(ROOT/'tools/diagnostics/check_import_cooked_surface.py'),
            '--source',str(expected),'--readback',str(folder/'skin'),'--output',str(folder/'surface.json')])
        run(name+'-basis',[sys.executable,'-B',str(ROOT/'tools/diagnostics/compare_eve_tangent_bake.py'),
            '--before',str(WORK/'pack/skin'),'--after',str(folder/'skin'),'--output',str(folder/'basis.json')])
        run(name+'-fallback',[sys.executable,'-B',str(ROOT/'tools/diagnostics/classify_eve_tangent_fallbacks.py'),
            '--readback',str(folder/'skin'),'--output',str(folder/'fallback.json')])
    for name in ('normal','facial'):
        fallback = read(out/name/'fallback.json')
        if not fallback['unit_lengths_within_bound'] or fallback['unexpected_nonorthogonal_vertices']:
            raise ValueError('Tangent quality gate failed')
    changed = [p for p,digest in protected.items() if sha(Path(p)) != digest]
    (out/'readbacks-verified.json').write_text(json.dumps(dict(passed=not changed,results=results,
        changed_inputs=changed,protected=protected,
        scope='Two-package roundtrip and independent geometry/surface/skin/morph checks. Cooked cloth/config, mapping and paired render comparison still required.'),indent=2)+'\n')
    if changed:
        raise ValueError('Inputs changed')


if __name__ == '__main__':
    main()
