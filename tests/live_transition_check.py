#!/usr/bin/env python3
"""Run cosmetic regressions in a safe loaded game, using a developer test core.

Run from the repository root. Requires CSS_TRANSITION_TESTS=ON and no other
request client. Builds nothing, changes no game save or gameplay ability.
"""
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from css import GAME, ROOT, atomic, processes, sha, stage_core, wait_json


def main():
    if not processes():
        raise RuntimeError('Load the game in a safe location first.')
    cache = (ROOT / 'build/windows/CMakeCache.txt').read_text()
    if 'CSS_TRANSITION_TESTS:BOOL=ON' not in cache:
        raise RuntimeError('Build the developer core with CSS_TRANSITION_TESTS=ON first.')
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'
    contract = json.loads((mod / 'loader-contract.json').read_text())
    if contract['abi'] != 1 or sha(mod / 'dlls/main.dll') != contract['dll_sha256']:
        raise RuntimeError('Installed loader ABI contract differs.')
    if any(sha(ROOT / p) != value for p, value in contract['sources'].items()):
        raise RuntimeError('Loader sources changed. Do not reload this test core.')

    def request(action, **values):
        rid = str(time.time_ns())
        atomic(mod / 'request.json', dict(action=action, id=rid, **values))
        inspection = action == 'inspect'
        path = mod / ('runtime/inspection.json' if inspection else 'runtime/status.json')
        return wait_json(path, lambda j: j.get('id' if inspection else 'last_request') == rid)

    before = request('inspect')
    if before['paused'] or before['input_owned'] or before['player_hidden']:
        raise RuntimeError('Close all menus and return to safe normal gameplay first.')
    state = (mod / 'state/state.json').read_bytes()
    name = stage_core(mod)
    wait_json(mod / 'runtime/loader.json', lambda j: j.get('core') == name)
    time.sleep(1)
    before = request('inspect')
    if before['transition']['IsInGameMenu']:
        raise RuntimeError('A game menu opened before the test.')
    effect_active = False
    try:
        request('test_reset_mesh')
        time.sleep(2)
        after = request('inspect')
        assert before['transition']['mesh'] == after['transition']['mesh'], 'Stock reset did not recover'
        assert before['gameplay_animation'] == after['gameplay_animation'], 'Animation instance changed'
        request('test_effect', begin=True)
        effect_active = True
        time.sleep(1)
        effect = request('inspect')
        assert effect['transition']['mesh'] != before['transition']['mesh'], 'Recovery overwrote effect'
        request('test_effect', begin=False)
        effect_active = False
        time.sleep(2)
        settled = request('inspect')
        assert settled['transition']['mesh'] == before['transition']['mesh'], 'No retry after effect'
        request('test_cursor', visible=True)
        opened = request('open')
        assert opened['wardrobe_open'], opened['message']
        preview = request('inspect')
        assert preview['paused'] and preview['input_owned'] and preview['preview_active']
        request('close')
        closed = request('inspect')
        assert not closed['paused'] and not closed['input_owned'] and not closed['player_hidden']
        assert before['gameplay_animation'] == closed['gameplay_animation']
        output = ROOT / 'work/fixed-transition-live.json'
        output.parent.mkdir(exist_ok=True)
        output.write_text(json.dumps(dict(before=before, after_reset=after, during_effect=effect,
                                         after_effect=settled, opened=preview, closed=closed), indent=2))
        assert (mod / 'state/state.json').read_bytes() == state, 'Test changed saved preferences'
        print(f'Live transition checks passed: {output}')
    finally:
        cleanup = [('close', {})]
        if effect_active:
            cleanup.append(('test_effect', {'begin': False}))
        cleanup.extend([('enable', {}), ('test_cursor', {'visible': before['cursor_visible']})])
        for action, values in cleanup:
            try:
                request(action, **values)
            except Exception as error:
                print(f'Cleanup {action} failed: {error}', file=sys.stderr)


if __name__ == '__main__':
    main()
