"""Fresh-load the private heel graph and record its compiled bone controls."""
import json
import os
from pathlib import Path
import unreal

work = Path('/home/eins0fx/development/mods/msII/CustomShellSystem/work/eve26')
kind = os.environ.get('CSS_HEEL_GRAPH_KIND', 'bikini')
assert kind in ('bikini', 'knit')
output = work / f'{kind}-foot-graph.json'
assert not output.exists()
base = unreal.load_asset('/Game/CSS/SeduXtress/ABP_Secondary')
candidate = unreal.load_asset('/Game/CSS/EveTest/' + ('ABP_BikiniFeet' if kind == 'bikini' else 'ABP_KnitFeet1'))
assert base and candidate
assert base.get_editor_property('target_skeleton') == candidate.get_editor_property('target_skeleton')
rows = {}
for label, blueprint in [('base', base), ('candidate', candidate)]:
    rows[label] = json.loads(unreal.CSSRetargetLibrary.inspect_pose_corrections(blueprint))
assert len(rows['candidate']) == len(rows['base']) + 2
feet = {row['boneToModify']['boneName']: row for row in rows['candidate']}
assert set(feet) == {'foot_l', 'foot_r'} and not rows['base']
for row in feet.values():
    assert row['rotationMode'] == 'BMM_Additive' and row['rotationSpace'] == 'BCS_BoneSpace'
    assert row['translationMode'] == row['scaleMode'] == 'BMM_Ignore'
    assert row['alpha'] == 1
output.write_text(json.dumps(dict(nodes=rows, shared_skeleton=True,
    scope='Fresh-loaded compiled defaults only. Pose playback comparison and game acceptance remain required.'), indent=2)+'\n')
print('HEEL_GRAPH_READBACK', len(rows['base']), len(rows['candidate']), flush=True)
