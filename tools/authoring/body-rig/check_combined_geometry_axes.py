"""Author production combined AnimBPs with 6- and 8-axis body geometry models.

Needs the CSSAuthoring editor module built with
tools/authoring-patches/body-geometry-axes.patch (run_stage.py --module
--writable CSS/CSS_SeduXtress_eins0fx_P). For each model it calls
CreatePostProcessCombined on the V40 mesh, HairV37 and BodyV42 (the cooked
production trio), into the stage's copy of the outfit folder, and records
whether the step accepted it and kept the string exactly:

- six:    the cooked schema 1 model (body-runtime-geometry-v1.json), must author;
- six2:   the same model rebuilt as schema 2 with 64 corners, must author;
- eight:  Eve's 8-axis schema 2 model (256 corners) on the fixture mesh
          (-CSSGeometryFixture=<mesh>): Eve's current export plus the authored
          PBMBreastsImplants/PBMBreastsCleavage deltas, imported into the stage
          with CSSImportMesh, because no package export with the new morphs has
          been cooked yet. Proves the step and the AnimBP string property at the
          8-axis size;
- eight_missing: the same model on V40, must be refused (V40 lacks the morphs);
- seven_corners: the 8-axis model claiming 255 corners, must be refused.

-CSSGeometryModels=name=path,... names the model files. Writes
combined-geometry-axes.json to CSS_BODY_RIG_STAGE.
"""
import json
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_rig_paths import stage

FOLDER = '/Game/CSS/CSS_SeduXtress_eins0fx_P'
MESH = '/Game/CSSAuthoring/CSS_SeduXtress_eins0fx_P/SK_SeduXtress_StockingUVV40'
HAIR = FOLDER + '/CR_SeduXtress_HairV37'
BODY = FOLDER + '/CR_SeduXtress_BodyV42'


def option(name):
    args = unreal.SystemLibrary.get_command_line().split()
    return next((a.split('=', 1)[1] for a in args if a.startswith(f'-{name}=')), '')


def main():
    output = stage()/'combined-geometry-axes.json'
    if output.exists():
        raise FileExistsError(output)
    models = dict(item.split('=', 1) for item in option('CSSGeometryModels').split(',') if item)
    fixture_path = option('CSSGeometryFixture')
    mesh, fixture, hair, body = [unreal.load_asset(p) for p in (MESH, fixture_path, HAIR, BODY)]
    if not all((mesh, fixture, hair, body)):
        raise ValueError('Missing production mesh, fixture mesh, hair rig or body rig')
    strings = {name: Path(path).read_text().strip() for name, path in models.items()}
    eight = json.loads(strings['eight'])
    cases = [('six', mesh, strings['six'], True), ('six2', mesh, strings['six2'], True),
             ('eight', fixture, strings['eight'], True),
             ('eight_missing', mesh, strings['eight'], False),
             ('seven_corners', fixture, json.dumps(dict(eight, corners=255), separators=(',', ':')), False)]
    report = dict(mesh=MESH, fixture=fixture_path, hair_rig=HAIR, body_rig=BODY, cases=[])
    for name, target, text, expected in cases:
        package = f'{FOLDER}/ABP_CSS_GeometryAxesProbe_{name}'
        blueprint = unreal.CSSControlRigLibrary.create_post_process_combined(target, hair, body, package, text)
        row = dict(name=name, mesh=target.get_path_name(), characters=len(text), expected=expected,
                   authored=bool(blueprint))
        if blueprint:
            defaults = unreal.get_default_object(blueprint.generated_class())
            kept = defaults.get_editor_property('CSSBodyGeometryJson')
            row.update(string_preserved=kept == text, morphs=json.loads(kept)['morphs'],
                       corners=json.loads(kept).get('corners'))
        row['passed'] = row['authored'] == expected and (not expected or row.get('string_preserved', False))
        report['cases'].append(row)
        unreal.log(f'Geometry axes case {name}: {row}')
    report['passed'] = all(r['passed'] for r in report['cases'])
    output.write_text(json.dumps(report, indent=2)+'\n')
    if not report['passed']:
        raise RuntimeError('Geometry axes check failed')


if __name__ == '__main__':
    main()
