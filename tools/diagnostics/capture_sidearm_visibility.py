"""Read the equipped sidearm and its persistent effect without changing gameplay. Python 3.14."""
import argparse
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'extensions/core/tools'))
from cssx import GAME, processes, request, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=30)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 120:
        parser.error('seconds must be 1..120')
    pids = processes()
    if len(pids) != 1:
        parser.error('Need one running game; no request was written')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    with args.output.open('x') as stream:
        def record(value):
            stream.write(json.dumps(value) + '\n')
            stream.flush()

        def query(value):
            if processes() != pids:
                raise RuntimeError('Game process changed')
            record(dict(kind='request', request=value))
            reply = request(GAME, dict(op='engine', request=value))
            record(dict(kind='reply', reply=reply))
            if not reply['ok']:
                raise RuntimeError(reply)
            return reply['result']

        def fields(target, names):
            if not target:
                return None
            properties = query(dict(op='properties', target=target, inherited=True))
            available = {p['name'] for p in properties}
            return dict(object=target,
                        missing=[name for name in names if name not in available],
                        values={name: query(dict(op='get', target=target, property=name))
                                for name in names if name in available})

        record(dict(kind='start', pids=pids, script_sha256=sha(Path(__file__)),
                    core_selector=json.loads((mod / 'core.json').read_text()),
                    gameplay_input=False, property_writes=False))
        deadline = time.monotonic() + args.seconds
        samples = 0
        while time.monotonic() < deadline:
            player = query(dict(op='player'))
            pawn = player.get('pawn')
            if not pawn:
                raise RuntimeError('No current player pawn')
            wc = query(dict(op='get', target=pawn, property='WeaponsComponent'))
            equipped = query(dict(op='get', target=wc, property='EquippedWeapons'))
            weapons = [entry['value'] for entry in equipped['$map']
                       if entry['key']['TagName'] == 'Weapon.Slot.Sidearm']
            if len(weapons) != 1 or not weapons[0]:
                raise RuntimeError('Need one equipped sidearm')
            weapon = fields(weapons[0], ('bHidden', 'RootComponent', 'WeaponMesh',
                                        'BPC_WeaponEffects', 'WeaponEffectTags',
                                        'CurrentEquipState'))
            effects = fields(weapon['values'].get('BPC_WeaponEffects'),
                             ('OwnerWeapon', 'AttachComponent', 'WeaponStaticMesh',
                              'SpawnedPermanent', 'Permanent', 'SpawnedPermanentProjectiles'))
            components = {}
            targets = {k: weapon['values'].get(k) for k in ('RootComponent', 'WeaponMesh')}
            if effects:
                targets.update({k: effects['values'].get(k) for k in
                                ('AttachComponent', 'WeaponStaticMesh', 'SpawnedPermanent')})
            for name, target in targets.items():
                components[name] = fields(target, ('bHiddenInGame', 'bVisible', 'bIsActive',
                                                   'AttachParent', 'AttachSocketName', 'Asset'))
            after = query(dict(op='player'))
            stable = player == after
            record(dict(kind='sample', time_ns=time.time_ns(), player=player,
                        weapon=weapon, effects=effects, components=components,
                        same_player_during_sample=stable))
            if not stable:
                raise RuntimeError('Player changed during sample; do not use it as a stable observation')
            samples += 1
            time.sleep(1)
        record(dict(kind='end', samples=samples,
                    limitation='Sequential property observations, not an atomic frame or proof of rendered visibility.'))
    print(args.output)


if __name__ == '__main__':
    main()
