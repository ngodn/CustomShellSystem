"""Summon Faithful copies across release outfits and restore appearance and view."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from capture_eve_surface_variant import ROOT, GAME, atomic, describe, identity, settled, processes
from check_peony_live_presets import customize, engine, request
from check_outfit_switch_lifecycle import OUTFITS
from check_sidearm_aim_visibility import MOD, geometry_restored
from verify_doubles_summon import verify_appearance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--core-sha256', required=True)
    parser.add_argument('--resume-from', type=Path)
    parser.add_argument('--outfits', nargs='+', choices=tuple(OUTFITS), default=list(OUTFITS))
    args = parser.parse_args()
    selected = {k: OUTFITS[k] for k in dict.fromkeys(args.outfits)}
    out = args.output.resolve()
    original, pids = describe(), processes()
    player = engine({'op': 'player'})
    if out.exists() or len(pids) != 1 or original['busy'] or original['idle_override']:
        raise RuntimeError('Need one settled player without an idle override and a new output')
    if original['shell'] != 'CharacterId.Player.Shell.Genessa':
        raise RuntimeError('This fixture checks Faithful summons')
    before = json.loads((MOD / 'state/state.json').read_text())
    if before['doubles']['faithful'] != 'css':
        raise RuntimeError('Use CSS must already be selected for Faithful doubles')
    imported = []
    if args.resume_from:
        prior = args.resume_from.resolve()
        previous = json.loads((prior / 'result.json').read_text())
        if previous['core_sha256'] != args.core_sha256 or json.loads((prior / 'state-before.json').read_text()) != before:
            raise RuntimeError('Prior cases used different code or saved settings')
        for row in previous['cases']:
            if row.get('passed') and row.get('returncode') == 0:
                verify_appearance(Path(row['evidence']))
                imported.append(dict(row, reused=True))
    completed = {(r['outfit'],r['variant']) for r in imported}
    memories = before['remembered_custom']
    restore_variants = {}
    for outfit, variants in selected.items():
        if any(outfit+'/'+v not in memories for v in variants):
            raise RuntimeError('Missing original variant memory')
        restore_variants[outfit] = next((v for v in variants if memories[outfit+'/'+v] == memories[outfit]), None)
        if restore_variants[outfit] is None:
            raise RuntimeError('Cannot restore outfit-level memory through a known variant')
    cine = request(GAME, {'op':'model','extension':'eins0fx.cine'})
    if not cine['ok']:
        raise RuntimeError('Cannot read CINE')
    cine_path = MOD.parent / 'CSSX/state/eins0fx.cine.json'
    cine_saved = json.loads(cine_path.read_text())

    def call(target, function, **fields):
        engine({'op':'describe','target':target,'function':function})
        return engine({'op':'call','target':target,'function':function,'args':fields})['ReturnValue']

    def geometry():
        return {k:call(t,f) for k,t,f in (
            ('position',player['pawn'],'K2_GetActorLocation'),
            ('rotation',player['pawn'],'K2_GetActorRotation'),
            ('view_target',player['controller'],'GetViewTarget'),
            ('control_rotation',player['controller'],'GetControlRotation'))}

    baseline = geometry()
    target, expected, idle = identity(original), original['customize'], original['idle_override']

    def guard(check_geometry=True):
        current = describe()
        if processes() != pids or engine({'op':'player'}) != player:
            raise RuntimeError('Player or process changed')
        if identity(current) != target or current['customize'] != expected or current['idle_override'] != idle:
            raise RuntimeError('Concurrent appearance change')
        if check_geometry:
            current_geometry = geometry()
            if any(current_geometry[k] != baseline[k] for k in ('position','view_target','control_rotation')):
                raise RuntimeError('Player or camera moved between cases')

    out.mkdir(parents=True)
    shutil.copyfile(__file__, out/'recipe.py')
    shutil.copyfile(ROOT/'tools/diagnostics/check_outfit_switch_lifecycle.py', out/'outfit-matrix-source.py')
    atomic(out/'state-before.json',before)
    atomic(out/'cine-before.json',cine)
    report = dict(passed=False, original=original, player=player, geometry_before=baseline,
                  core_sha256=args.core_sha256, cases=imported, requested_outfits=list(selected),
                  excluded_previously_checked=['eins0fx.seduxtress/peony'])
    try:
        customize({'action':'idle','value':'eve'})
        idle = 'eve'
        for outfit, variants in selected.items():
            for variant in variants:
                if (outfit,variant) in completed or (outfit,variant) == ('eins0fx.seduxtress','peony'):
                    continue
                guard()
                target, expected = (original['shell'],outfit,variant), memories[outfit+'/'+variant]
                customize({'action':'wear','outfit':outfit,'variant':variant,'customize':expected,'persist':False})
                settled(target,custom=expected)
                guard()
                folder = out/(outfit+'-'+variant)
                command = [sys.executable,str(ROOT/'tools/diagnostics/check_doubles_summon.py'),
                    '--output',str(folder),'--core-sha256',args.core_sha256]
                row = dict(outfit=outfit,variant=variant,evidence=str(folder),command=command)
                report['cases'].append(row)
                atomic(out/'result.json',report)
                with (out/(folder.name+'.log')).open('x') as log:
                    result = subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
                row['returncode'] = result.returncode
                atomic(out/'result.json',report)
                result.check_returncode()
                row['verification'] = verify_appearance(folder)
                row['passed'] = True
                atomic(out/'result.json',report)
                print(outfit,variant,'pass',flush=True)
        wanted = {(o,v) for o,vs in selected.items() for v in vs
                  if (o,v) != ('eins0fx.seduxtress','peony')}
        report['cases_passed'] = {(r['outfit'],r['variant']) for r in report['cases']} == wanted and all(row.get('passed') for row in report['cases'])
    except Exception as error:
        report['error'] = repr(error)
    finally:
        try:
            guard(check_geometry=False)
            for outfit,variant in restore_variants.items():
                customize({'action':'wear','outfit':outfit,'variant':variant,'customize':memories[outfit],'persist':False})
                settled((original['shell'],outfit,variant),custom=memories[outfit])
            customize({'action':'idle','value':original['idle_override']})
            customize({'action':'wear','outfit':original['outfit'],'variant':original['variant'],
                       'customize':original['customize'],'persist':True})
            settled(identity(original),custom=original['customize'])
            time.sleep(1)
            current = geometry()
            changed_view = any(current[k] != baseline[k] for k in ('position','view_target','control_rotation'))
            if changed_view:
                report['facing_restore_skipped'] = 'Preserved newer player position or camera view'
            else:
                mesh = engine({'op':'get','target':player['pawn'],'property':'Mesh'})
                anim = call(mesh,'GetAnimInstance')
                if engine({'op':'get','target':anim,'property':'Speed'}) > .05 or call(anim,'IsAnyMontagePlaying'):
                    raise RuntimeError('Player is moving; facing restoration stopped')
                time.sleep(.5)
                if geometry() != current:
                    raise RuntimeError('Player moved; facing restoration stopped')
                if not geometry_restored(baseline,current):
                    if not call(player['pawn'],'K2_SetActorLocationAndRotation',NewLocation=baseline['position'],
                                NewRotation=baseline['rotation'],bSweep=False,bTeleport=True):
                        raise RuntimeError('Facing restoration failed')
                    time.sleep(.5)
            after = json.loads((MOD/'state/state.json').read_text())
            atomic(out/'state-after.json',after)
            report['state_restored'] = after == before
            report['geometry_after'] = geometry()
            report['geometry_restored'] = geometry_restored(baseline,report['geometry_after'])
            cine_after = request(GAME,{'op':'model','extension':'eins0fx.cine'})
            atomic(out/'cine-after.json',cine_after)
            def values(model):
                return {c['id']:(c.get('value'),c.get('enabled')) for s in model['result']['sections'] for c in s['controls']}
            report['cine_model_equal'] = cine_after['result'] == cine['result']
            report['cine_restored'] = json.loads(cine_path.read_text()) == cine_saved and values(cine_after) == values(cine)
            report['process_preserved'] = processes() == pids
            report['passed'] = bool(report.get('cases_passed') and all(report[k] for k in
                ('state_restored','geometry_restored','cine_restored','process_preserved')))
        except Exception as error:
            report['restore_error'] = repr(error)
        atomic(out/'result.json',report)
    print(json.dumps({k:report.get(k) for k in ('passed','error','restore_error','state_restored','geometry_restored','cine_restored')}))
    if not report['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
