"""Temporarily toggle the focused favorite and verify UI selection survives reordering.

Python 3.14. Run with CSS SHELL open and controls idle. Restores the tested
favorite bit on exit; never equips a different outfit or changes customization.
"""
import json
from pathlib import Path
import time

from css import ROOT, atomic
import css_capture as capture

out=ROOT/'work/ui-live1/favorites';assert not out.exists();out.mkdir()
capture.MEDIA=out
state_path=capture.MOD/'state/state.json'
before=json.loads(state_path.read_text());atomic(out/'state-before.json',before)
target='beaute.genessa';assert target in before['favorites']
def inspect(label):
    r=capture.command('inventory_inspect');atomic(out/(label+'.json'),r)
    assert r['css']['active'] and r['css']['section']==0
    favorite=[h['action'] for h in r['hit_points'] if h['action'].get('action')=='favorite']
    assert len(favorite)==1 and favorite[0]['outfit']==target,(label,favorite)
    return r['css']['row']

capture.command('inventory_capture_row',section=0,row=3,offset=0)
time.sleep(.4)
first=inspect('before');assert first==3
try:
    capture.command('favorite',outfit=target);time.sleep(.5)
    removed=inspect('removed')
    assert removed>first,'Unfavorited item did not move below remaining favorites'
    capture.shot('removed')
finally:
    current=json.loads(state_path.read_text())
    if target not in current['favorites']:capture.command('favorite',outfit=target)
time.sleep(.5)
restored=inspect('restored');assert restored==first
after=json.loads(state_path.read_text());atomic(out/'state-after.json',after)
assert after==before,'Saved state changed beyond the temporary favorite toggle'
capture.shot('restored')
atomic(out/'verification.json',dict(passed=True,selected_outfit=target,
    row_before=first,row_unfavorited=removed,row_restored=restored,saved_state_restored=True))
print(json.dumps(dict(passed=True,rows=[first,removed,restored])))
