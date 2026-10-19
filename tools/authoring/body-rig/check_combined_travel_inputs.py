"""Author a combined body/hair AnimBP on BodyAngularV11 and check the travel pair.

Needs the CSSAuthoring editor module built with
tools/authoring-patches/body-travel-inputs.patch (run_stage.py --module). It
creates /Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeCombinedV4 in the
stage, then checks:

- the generated class has CSSBodyRegionTravel (seven zeros) and CSSBodyTravel
  (0), with the region and contact inputs of the V3 combined graph;
- the existing component fixtures still run on it;
- region_travel: distinct per-region values and a global value, changed at
  one second, reach the live body rig every frame (the C++ fixture fails on
  any mismatch and on any property shape the native adapter would refuse).

Writes combined-v4-travel.json to CSS_BODY_RIG_STAGE.
"""
import json
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_rig_paths import stage

PACKAGE = '/Game/CSS/TransientProbes/ABP_CSS_ControlRigProbeCombinedV4'
MESH = '/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_StockingUVV40'
HAIR = '/Game/CSS/CSS_SeduXtress_eins0fx_P/CR_SeduXtress_HairV37'
BODY = '/Game/CSSAuthoring/TransientProbes/CR_CSS_BodyAngularV11'
OLD_BODY = '/Game/CSSAuthoring/TransientProbes/CR_CSS_BodyAngularV10'


def main():
    output = stage()/'combined-v4-travel.json'
    if output.exists() or unreal.EditorAssetLibrary.does_asset_exist(PACKAGE):
        raise FileExistsError('Preserve the previous combined candidate')
    mesh, hair, body = [unreal.load_asset(p) for p in (MESH, HAIR, BODY)]
    if not all((mesh, hair, body)):
        raise ValueError('Missing mesh, hair rig or V11 body rig')
    blueprint = unreal.CSSControlRigLibrary.create_post_process_combined_probe(mesh, hair, body, PACKAGE)
    if not blueprint:
        raise RuntimeError('Combined AnimBP authoring failed')
    defaults = unreal.get_default_object(blueprint.generated_class())
    report = dict(package=PACKAGE, mesh=MESH, hair_rig=HAIR, body_rig=BODY, cases=[])
    report['defaults'] = dict(
        CSSBodyRegionTravel=list(defaults.get_editor_property('CSSBodyRegionTravel')),
        CSSBodyTravel=defaults.get_editor_property('CSSBodyTravel'),
        CSSBodyUseRegionSettings=defaults.get_editor_property('CSSBodyUseRegionSettings'),
        CSSBodyRegionFrequencies=list(defaults.get_editor_property('CSSBodyRegionFrequencies')))
    if report['defaults']['CSSBodyRegionTravel'] != [0.]*7 or report['defaults']['CSSBodyTravel'] != 0:
        raise ValueError('Unexpected travel defaults on the combined AnimBP')
    # An older body rig must still author, without the pair.
    old = unreal.CSSControlRigLibrary.create_post_process_combined_probe(
        mesh, hair, unreal.load_asset(OLD_BODY), PACKAGE + 'Legacy')
    if not old:
        raise RuntimeError('Combined AnimBP on BodyAngularV10 no longer authors')
    legacy = unreal.get_default_object(old.generated_class())
    try:
        legacy.get_editor_property('CSSBodyTravel')
        report['legacy_has_travel'] = True
    except Exception:
        report['legacy_has_travel'] = False
    if report['legacy_has_travel']:
        raise ValueError('A V10 body rig must not get travel inputs')
    for mode, fps in [('both', 60), ('region_defaults', 60), ('region_motion', 60), ('body_only_reset', 60),
                      ('region_travel', 30), ('region_travel', 60), ('region_travel', 120)]:
        raw = unreal.CSSControlRigLibrary.evaluate_combined_probe(blueprint, mesh, mode, fps)
        if not raw:
            raise RuntimeError(f'Combined component fixture failed: {mode} {fps}')
        value = json.loads(raw)
        samples = value.get('samples', [])
        inputs = [s.get('body_array_inputs', {}) for s in samples]
        report['cases'].append(dict(name=f'{mode}_{fps}', frames=len(samples),
            travel_first=inputs[0].get('RegionTravel') if inputs else None,
            travel_last=inputs[-1].get('RegionTravel') if inputs else None,
            global_first=inputs[0].get('Travel') if inputs else None,
            global_last=inputs[-1].get('Travel') if inputs else None,
            soft_zone=inputs[-1].get('SoftZone') if inputs else None,
            clamp_contacts=inputs[-1].get('ClampContacts') if inputs else None,
            transferred_frames=sum(1 for i in inputs if 'RegionTravel' in i)))
        unreal.log(f'Combined travel fixture completed: {mode} {fps}')
    output.write_text(json.dumps(report, indent=2)+'\n')


if __name__ == '__main__':
    main()
