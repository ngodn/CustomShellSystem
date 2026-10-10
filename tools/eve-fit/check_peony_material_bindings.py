"""Read saved Peony material defaults through UE 5.6.1 without saving assets."""
import hashlib
import json
from pathlib import Path

import unreal


WORK = Path(unreal.Paths.project_dir()).resolve().parent
EDIT = unreal.MaterialEditingLibrary


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linear(value):
    return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4


def main():
    if WORK.name != 'peony-customize1':
        raise ValueError('Use the private customization check project')
    report = WORK / 'material-readback.json'
    if report.exists():
        raise FileExistsError(report)
    recipe = json.loads((WORK / 'wardrobe-customize.json').read_text())
    plan = json.loads((WORK / 'binding-plan.json').read_text())
    stage = json.loads((WORK / 'material-stage.json').read_text())
    if any(sha(WORK / path) != checksum for path, checksum in stage['files'].items()):
        raise ValueError('Staged material changed before readback')
    loaded = {}
    rows = []
    checked = set()
    for palette in recipe['palettes']:
        for control in recipe['controls']:
            for binding in control.get('bindings', []):
                path = plan['bindings'][binding['slot']]['material'].replace(
                    'MI_peony_monochrome_', 'MI_' + palette['id'] + '_')
                parameter = binding['parameter']
                key = path, parameter
                if key in checked:
                    continue
                checked.add(key)
                if path not in loaded:
                    material = unreal.load_asset(path)
                    if not isinstance(material, unreal.MaterialInstanceConstant):
                        raise ValueError('Missing material instance: ' + path)
                    loaded[path] = material
                material = loaded[path]
                vector = control['kind'] == 'color'
                getter = EDIT.get_vector_parameter_names if vector else EDIT.get_scalar_parameter_names
                if parameter not in {str(name) for name in getter(material)}:
                    raise ValueError('Missing material parameter: ' + str(key))
                expected = palette['values'].get(control['id'], control['default'])
                if vector:
                    value = EDIT.get_material_instance_vector_parameter_value(material, parameter)
                    actual = [getattr(value, channel) for channel in ('r', 'g', 'b', 'a')]
                    expected = [linear(v) for v in expected[:3]] + [expected[3]]
                else:
                    actual = [EDIT.get_material_instance_scalar_parameter_value(material, parameter)]
                    expected = expected[:1]
                if any(abs(a - b) > 1e-5 for a, b in zip(actual, expected)):
                    raise ValueError('Material/catalog default mismatch: ' + str((key, actual, expected)))
                rows.append(dict(material=path, parameter=parameter, value=actual))
    if any(sha(WORK / path) != checksum for path, checksum in stage['files'].items()):
        raise ValueError('Readback altered a staged asset')
    report.write_text(json.dumps(dict(passed=True, material_instances=len(loaded),
        parameter_checks=len(rows), rows=rows, recipe_sha256=sha(WORK / 'wardrobe-customize.json'),
        checker_sha256=sha(Path(__file__)), staged_assets_unchanged=True,
        limitation='Saved editor parameter defaults only; shader pixels, runtime writes and game appearance are separate.'), indent=2) + '\n')
    print('PEONY_MATERIAL_READBACK', len(loaded), len(rows), flush=True)


if __name__ == '__main__':
    main()
