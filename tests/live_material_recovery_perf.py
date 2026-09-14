#!/usr/bin/env python3
"""Check colored-outfit recovery on a safe, idle player with a developer core.

Run alone, after staging build/cssx-native. Clears one cosmetic material slot
three times; CSS must restore it without rebuilding its dye textures. No input,
mesh, gameplay or saved setting is changed. Evidence stays under ignored work/.
"""
import argparse
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css import ROOT, sha
from css_capture import command
from cssx_cheat_check import request as host
from cssx_dev import MOD, BUILD


def get(target, name):
    return host(dict(op='get', target=target, property=name), True)


def call(target, name, **args):
    return host(dict(op='call', target=target, function=name, args=args), True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mesh', action='store_true', help='Reset the stock mesh as well as its materials')
    args = parser.parse_args()
    if args.mesh:
        assert 'CSS_TRANSITION_TESTS:BOOL=ON' in (BUILD / 'CMakeCache.txt').read_text()
    loader = json.loads((MOD / 'runtime/loader.json').read_text())
    assert sha(MOD / 'cores' / loader['core']) == sha(BUILD / 'css_core.dll'), 'Stage the current developer core'
    player = host({'op': 'player'}, True)
    pc = get(player['pawn'], 'Controller')
    for name in ('IsInGameMenu', 'IsMoveInputIgnored', 'IsLookInputIgnored'):
        assert not call(pc, name)['ReturnValue'], 'Wait until gameplay is idle'
    mesh = get(player['pawn'], 'Mesh')
    before = get(mesh, 'OverrideMaterials')
    slot = next((i for i, m in enumerate(before) if m and 'MaterialInstanceDynamic' in m['name']), None)
    assert slot is not None, 'Select an outfit with a non-original color palette'
    state = (MOD / 'state/state.json').read_bytes()
    evidence = []
    try:
        for _ in range(3):
            offset = (MOD / 'CSS.log').stat().st_size
            if args.mesh:
                command('test_reset_mesh')
            else:
                call(mesh, 'SetMaterial', ElementIndex=slot, Material=None)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                with (MOD / 'CSS.log').open('rb') as log:
                    log.seek(offset)
                    lines = log.read().decode('utf-8', errors='replace')
                if 'Appearance verified:' in lines:
                    break
                time.sleep(.02)
            else:
                raise AssertionError('No material recovery within five seconds')
            time.sleep(.1)
            status = json.loads((MOD / 'runtime/status.json').read_text())
            after = get(mesh, 'OverrideMaterials')
            evidence.append(dict(apply_ms=status['apply_ms'], same_materials=before == after,
                                 log=lines.strip().splitlines()))
            before = after
        assert (MOD / 'state/state.json').read_bytes() == state, 'Saved preferences changed'
        assert all(e['same_materials'] for e in evidence), 'Recovery rebuilt the color materials'
        assert max(e['apply_ms'] for e in evidence) < 100, 'Material recovery exceeded the 100 ms regression budget'
    finally:
        target = ROOT / 'work/cssx-native' / ('mesh-color-recovery-perf.json' if args.mesh else 'material-recovery-perf.json')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(json.dumps(evidence, indent=2) + '\n')
        print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    main()
