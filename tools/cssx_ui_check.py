#!/usr/bin/env python3
"""Live development regression for CSSX text-field construction. No game changes."""
import argparse
import time
from css_capture import command
from cssx_dev import command as probe


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extension',default='cssx.ui-kit')
    args=parser.parse_args()
    library=probe({'op':'library'})
    assert library['ok'],library
    assert any(e['id']==args.extension and e['available'] for e in library['result']['extensions']),library
    command('inventory_select',index=2)
    command('inventory_cssx',event={'action':'x_open','id':args.extension})
    command('inventory_cssx',event={'action':'x_row','row':5})
    time.sleep(.7)
    result=command('inventory_inspect')
    switcher=result['main_objects']['BP_WS_Menu_Game']
    assert len(switcher['children'])==5 and switcher['active_index']==2, 'Text field detached the CSSX page'
    draft='颜色 test draft'
    command('inventory_cssx_text',text=draft)
    command('inventory_cssx',event={'action':'x_row','row':4})
    time.sleep(.2)
    command('inventory_cssx',event={'action':'x_row','row':5})
    time.sleep(.2)
    assert command('inventory_inspect')['css']['text']==draft, 'UI rebuild lost the draft'
    command('inventory_cssx',event={'action':'x_text'})
    model=probe({'op':'model','id':args.extension})
    assert model['ok'] and model['result']['sections'][0]['controls'][5]['value']==draft,model
    print('Text input stays open, keeps its Unicode draft across rebuilds and saves it')


if __name__=='__main__':main()
