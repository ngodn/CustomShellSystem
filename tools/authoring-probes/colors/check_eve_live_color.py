"""Check live CSS skin texels against the authored mask and restore the selection. Python 3.14."""
import json, sys, time
from pathlib import Path
import numpy as np
from PIL import Image
CSS=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(CSS/'tools'))
import css_capture as c
from css_live_snapshot import Probe
from check_inventory_camera import checked_call, checked_get
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--metadata',type=Path,required=True)
parser.add_argument('--rgb',default='DDC0A2',help='Six hexadecimal sRGB digits')
args=parser.parse_args()
assert len(args.rgb)==6
rgb=np.array([int(args.rgb[i:i+2],16) for i in (0,2,4)])
out=args.output.resolve()
assert out.is_relative_to(CSS/'work') and not out.exists()
out.mkdir(parents=True)
c.MEDIA=out
state_path=c.MOD/'state/state.json'
def state(): return json.loads(state_path.read_text())
def lin(v): return np.where(v<=.04045,v/12.92,((v+.055)/1.055)**2.4)
def enc(v): return np.where(v<=.0031308,v*12.92,1.055*v**(1/2.4)-.055)
with (out/'pixels.jsonl').open('w') as log:
 p=Probe(log); player=p.send('player'); shell=player['shell']; before=state()
 selected=before['selections'][shell]; assert selected['outfit']=='eins0fx.seduxtress'
 custom=selected['customize']; assert not custom.get('tints'), 'Neutral tint required'
 old=custom['values'].get('skin'); (out/'trial-before.json').write_text(json.dumps(before,indent=2))
 lib=p.send('find',path='/Script/Engine.Default__KismetRenderingLibrary')
 mesh=checked_get(p,player['pawn'],'Mesh')
 results=[]
 try:
  menu=c.command('inventory_inspect')
  assert menu['css']['active'], 'Open CSS before this visual check'
  c.shot('before')
  c.command('control',control='skin',rgb=(rgb/255).tolist()); time.sleep(1)
  (out/'status-active.json').write_text(json.dumps(c.command('status'),indent=2))
  c.shot('custom')
  for slot, filename, points in [(2,'dye-skin-body.png',[(512,400),(512,650),(420,560)]),(0,'dye-00.png',[(512,512)]),(5,'dye-skin-groin.png',[(512,870),(475,890)])]:
   mid=checked_call(p,mesh,'GetMaterial',ElementIndex=slot)['ReturnValue']
   target=checked_call(p,mid,'K2_GetTextureParameterValue',ParameterName='BaseColorMap  non VT')['ReturnValue']
   if 'TextureRenderTarget2D' not in target['name']:
    results.append(dict(slot=slot,error='Expected dye render target',actual=target))
    continue
   a=np.asarray(Image.open(args.metadata/filename).convert('RGBA'))
   for x,y in points:
    pixel=checked_call(p,lib,'ReadRenderTargetPixel',WorldContextObject=mesh,TextureRenderTarget=target,X=x,Y=y)['ReturnValue']
    expected=np.rint(enc(lin(a[y,x,:3]/255)*lin(rgb/255))*255).astype(int).tolist()
    results.append(dict(slot=slot,xy=[x,y],mask=a[y,x].tolist(),actual=pixel,expected=expected))
  assert p.send('player')==player,'Player changed'
 finally:
  if p.send('player')==player:
   if old is None: c.command('reset_control',control='skin')
   else: c.command('control',control='skin',rgb=old[:3])
   time.sleep(1)
 after=state(); (out/'trial-after.json').write_text(json.dumps(after,indent=2))
 result=dict(pixels=results,state_restored=before==after)
 (out/'pixels.json').write_text(json.dumps(result,indent=2)); print(json.dumps(result))
 assert before==after,'State changed during trial'
 for r in results:
  assert 'error' not in r,r
  if r['mask'][3]==255:
   assert max(abs(r['actual'][k]-v) for k,v in zip(('R','G','B'),r['expected']))<=5,'CSS dye RGB differs from expected linear-light tint'
