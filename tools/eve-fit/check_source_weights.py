"""Compare saved garment weights with a snapshotted cleanup, without saving assets."""
import bpy, json, hashlib, sys
from pathlib import Path
root=Path(__file__).resolve().parents[3]
work=root/'CustomShellSystem/work/eve26'
sys.path.insert(0,str(Path(__file__).resolve().parent))
from clean_weights import clean_weights
report={}
for filename in ('holiday-f10.blend','holiday-f11.blend'):
    bpy.ops.wm.open_mainfile(filepath=str(work/filename))
    result={}
    for part in ('Dress','Arms','Legs','Panties'):
        obj=bpy.data.objects['Eve Christmas - '+part]
        raw=[[(g.group,g.weight) for g in v.groups] for v in obj.data.vertices]
        clean_weights(obj)
        cleaned=[[(g.group,g.weight) for g in v.groups] for v in obj.data.vertices]
        tiny=[(i,groups) for i,groups in enumerate(cleaned) if any(0<w<.0001 for _,w in groups)]
        result[part]={'raw_sha':hashlib.sha256(repr(raw).encode()).hexdigest(),'clean_sha':hashlib.sha256(repr(cleaned).encode()).hexdigest(),'tiny_after_cleanup':tiny}
    report[filename]=result
    print(filename,result,flush=True)
assert report['holiday-f10.blend'] == report['holiday-f11.blend'], 'Saved source or cleaned weights differ'
assert all(not part['tiny_after_cleanup'] for part in report['holiday-f10.blend'].values())
(work/'source-weights-stable.json').write_text(json.dumps(report,indent=2)+'\n')
