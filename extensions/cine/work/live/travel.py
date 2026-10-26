"""Teleport her to a landing area by name, the way Traverse does (plain streaming teleport, no meteor). Python 3.14.

  travel.py "Area Name"     teleport
  travel.py --list          every landing area, nearest first
"""
import sys
import time

import reel as r


def areas():
    pawn = r.eng({'op': 'player'})['pawn']
    gs = r.eng({'op': 'find', 'path': '/Script/Engine.Default__GameplayStatics'})
    cls = r.eng({'op': 'load', 'path': '/Game/Sparta/Core/World/Launch/BP_LandingAreaBase.BP_LandingAreaBase_C'})
    out = r.eng({'op': 'call', 'target': gs, 'function': 'GetAllActorsOfClass', 'args': {'WorldContextObject': pawn, 'ActorClass': cls}}).get('OutActors', [])
    me = r.call(pawn, 'K2_GetActorLocation', {})
    rows = []
    for a in out:
        try:
            name = str(r.eng({'op': 'get', 'target': a, 'property': 'Area Name'})).strip()
            loc = r.call(a, 'K2_GetActorLocation', {})
            d = ((loc['X'] - me['X']) ** 2 + (loc['Y'] - me['Y']) ** 2 + (loc['Z'] - me['Z']) ** 2) ** 0.5 / 100
            rows.append((round(d), name, a))
        except Exception:
            pass
    rows.sort(key=lambda x: x[0])
    return rows


def travel(name):
    pawn = r.eng({'op': 'player'})['pawn']
    world = r.eng({'op': 'get', 'target': pawn, 'property': 'Owner'})   # any world object serves as the context
    target = next((a for d, n, a in areas() if n.lower() == name.lower()), None)
    if target is None:
        raise SystemExit(f'No landing area named {name!r}. See --list.')
    tf = r.eng({'op': 'call', 'target': target, 'function': 'GetStartTransform', 'args': {'InvertRotation': False}})
    tf = tf.get('ReturnValue', tf)
    math = r.eng({'op': 'find', 'path': '/Script/Engine.Default__KismetMathLibrary'})
    br = r.eng({'op': 'call', 'target': math, 'function': 'BreakTransform', 'args': {'InTransform': tf}})
    loc, rot = tf['Translation'], br['Rotation']
    zone = r.eng({'op': 'get', 'target': target, 'property': 'ZoneData'})
    if not isinstance(zone, dict):
        z = r.eng({'op': 'call', 'target': target, 'function': 'GetZone', 'args': {}})
        zone = z.get('ZoneData') if isinstance(z, dict) else None
    lib = r.eng({'op': 'class_default', 'class': 'BPFL_WorldStreaming_C'})
    manager = r.eng({'op': 'call', 'target': lib, 'function': 'GetTeleportManager', 'args': {'__WorldContext': pawn}})
    m = manager.get('ReturnValue', manager)
    if isinstance(m, dict) and r.eng({'op': 'get', 'target': m, 'property': 'bIsInDungeon'}):
        raise SystemExit('She is in a dungeon; leave it by its own exit first.')
    r.eng({'op': 'call', 'target': lib, 'function': 'TeleportPlayerWithStreaming', 'args': {
        'Location': loc, 'Rotation': rot, 'ControlRotation': rot, 'ScreenTransitionClass': None, 'TransitionZOrder': 20,
        'FadeInDuration': 0.4, 'FadeOutDuration': 0.5, 'OptionalZoneData': zone, '__WorldContext': pawn}})
    print(f'traversing to {name}')
    for _ in range(40):
        time.sleep(0.5)
        try:
            now = r.call(r.eng({'op': 'player'})['pawn'], 'K2_GetActorLocation', {})
            if ((now['X'] - loc['X']) ** 2 + (now['Y'] - loc['Y']) ** 2) ** 0.5 < 300:
                print('arrived', {k: round(v) for k, v in now.items()})
                return
        except Exception:
            pass
    print('not confirmed arrived yet')


if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] == '--list':
        for d, n, a in areas():
            print(f'{d:6d} m  {n}')
    else:
        travel(' '.join(sys.argv[1:]))
