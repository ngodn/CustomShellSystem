#!/usr/bin/env python3
"""Reproduce leftover CSS color materials after Genessa -> Proxima -> Genessa.

Requires a safe loaded Genessa, a colored CSS outfit, an unlocked Proxima,
the staged developer core and CSSX Cheat Menu with cheats disabled. No unlocks,
items or appearance preferences are changed. Run without another runtime driver.
"""
import json
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css import ROOT, sha
from cssx_cheat_check import request as host, event, model
from cssx_dev import MOD, BUILD


def get(target, name):
    return host(dict(op='get', target=target, property=name), True)


def switch(name, tag):
    event('shell', value=name)
    event('switch_shell', confirmed=True)
    for _ in range(40):
        time.sleep(.2)
        player = host({'op': 'player'}, True)
        if player['shell'] == tag:
            return player
    raise AssertionError(f'{name} did not become the active shell')


def main():
    loader = json.loads((MOD / 'runtime/loader.json').read_text())
    assert sha(MOD / 'cores' / loader['core']) == sha(BUILD / 'css_core.dll'), 'Stage the current developer core'
    player = host({'op': 'player'}, True)
    assert player['shell'] == 'CharacterId.Player.Shell.Genessa'
    controls = model()
    assert not controls['disable_all']['enabled'] and not controls['apply_settings']['enabled']
    saved = (MOD / 'state/state.json').read_bytes()
    old_choice = controls['shell']['value']
    evidence = []
    try:
        for _ in range(3):
            mesh = get(player['pawn'], 'Mesh')
            before = get(mesh, 'OverrideMaterials')
            owned = {m['name'] for m in before if m and 'MaterialInstanceDynamic' in m['name']}
            assert owned, 'Select a colored CSS outfit before this check'
            player = switch('Proxima', 'CharacterId.Player.Shell.KnightLady')
            time.sleep(.5)
            after = get(get(player['pawn'], 'Mesh'), 'OverrideMaterials')
            leftovers = [m['name'] for m in after if m and m['name'] in owned]
            evidence.append(dict(shell=player['shell'], leftovers=leftovers))
            assert not leftovers, 'Previous-shell color materials remain on the new shell'
            player = switch('Genessa', 'CharacterId.Player.Shell.Genessa')
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                current = json.loads((MOD / 'runtime/status.json').read_text())
                if current['applied'] and not current['recovery_pending']:
                    break
                time.sleep(.1)
            else:
                raise AssertionError('Genessa outfit did not recover')
        assert (MOD / 'state/state.json').read_bytes() == saved, 'Appearance preferences changed'
    finally:
        switch('Genessa', 'CharacterId.Player.Shell.Genessa')
        event('shell', value=old_choice)
        path = ROOT / 'work/cssx-native/colored-shell-switch-check.json'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(evidence, indent=2) + '\n')
        print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    main()
