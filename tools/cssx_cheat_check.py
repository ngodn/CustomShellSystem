#!/usr/bin/env python3
"""Live reversible checks for native Cheat Menu drafts. Run somewhere safe."""
import time
from cssx_dev import command

ID='eins0fx.cheat-menu'
def request(value,engine=False):
    result=command(value,engine)
    if not result['ok']: raise RuntimeError(result.get('error',str(result)))
    return result['result']
def event(id,**fields):return request({'op':'event','id':ID,'event':{'id':id,**fields}})
def model():
    m=request({'op':'model','id':ID})
    return {c['id']:c for s in m['sections'] for c in s['controls']}
def main():
    c=model()
    assert not c['apply_settings'].get('enabled',True), 'Apply or discard existing edits before this test'
    assert not c['god']['value'], 'Disable God before this test'
    pawn=request({'op':'player'},True)['pawn']
    before=request({'op':'get','target':pawn,'property':'bCanBeDamaged'},True)
    assert before is True,'Test requires a damageable pawn'
    try:
        event('god',value=True);time.sleep(.4)
        assert request({'op':'get','target':pawn,'property':'bCanBeDamaged'},True)==before,'Draft changed gameplay'
        assert model()['apply_settings']['enabled'],'Apply did not notice the edit'
        event('discard_changes');assert model()['god']['value'] is False,'Discard failed'
        event('god',value=True);event('apply_settings')
        assert request({'op':'get','target':pawn,'property':'bCanBeDamaged'},True) is False,'Apply did not enable God'
        event('disable_all')
        assert request({'op':'get','target':pawn,'property':'bCanBeDamaged'},True)==before,'Disable all did not restore original flag'
        assert not model()['apply_settings']['enabled'],'False pending edits after cleanup'
        print('Draft did not touch gameplay; Discard, Apply, Disable all and original-value restoration passed')
    finally:
        # Even an assertion failure must attempt to release owned flags.
        c=model()
        if c['disable_all'].get('enabled',False):event('disable_all')
        if c['discard_changes'].get('enabled',False):event('discard_changes')
if __name__=='__main__':main()
