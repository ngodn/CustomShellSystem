#!/usr/bin/env python3
"""Live CSSX picker regression against the no-game-effects UI Kit preview."""
import time
from css_capture import command
from cssx_dev import command as probe

command('inventory_select',index=2)
command('inventory_cssx',event={'action':'x_open','id':'cssx.ui-kit'})
command('inventory_cssx',event={'action':'x_row','row':6})
command('inventory_cssx',event={'action':'x_pick'})
time.sleep(.3)
assert command('inventory_inspect')['css']['picker']
command('inventory_cssx_search',text='option 256')
time.sleep(.2)
view=command('inventory_inspect')['css']
assert view['matches']==1 and view['selected_option']=='item-256',view
command('inventory_cssx_search',text='no matching item')
time.sleep(.2)
view=command('inventory_inspect')['css']
assert view['matches']==0 and view['selected_option'] is None,view
command('inventory_cssx',event={'action':'x_pick_apply'})
assert command('inventory_inspect')['css']['picker'], 'Empty results must not commit'
command('inventory_cssx_search',text='OPTION 256')
time.sleep(.2)
command('inventory_cssx',event={'action':'x_pick_apply'})
time.sleep(.2)
model=probe({'op':'model','id':'cssx.ui-kit'})
assert model['ok'] and model['result']['sections'][0]['controls'][6]['value']=='item-256',model
assert not command('inventory_inspect')['css']['picker']
print('Picker filters 256 options, handles empty results, matches case-insensitively and commits only on Select')
