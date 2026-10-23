"""Reload and verify Profane Proxima's shared ghost graphs before cooking (adapted from verify_peony_astral.py)."""
import hashlib
import json
import os
from pathlib import Path

import unreal

stage = Path(os.environ['CSS_PROFANE_ASTRAL_STAGE']).resolve(strict=True)
assert stage.name.startswith('profane-astral')
v2 = True
assert Path(unreal.Paths.project_dir()).resolve() == stage / 'editor'
output = stage / 'reloaded.json'
assert not output.exists()
manifest = json.loads((stage / 'authored.json').read_text())
expected_count = 7
assert len(manifest['materials']) == len(manifest['variants']) == expected_count
edit = unreal.MaterialEditingLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True, include_searchable_names=False,
    include_soft_management_references=False, include_hard_management_references=False)
inputs = {}
for path, checksum in manifest['protected'].items():
    assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == checksum, path
    inputs[path] = checksum

rows = []
mask_path = '/Game/CSS/eins0fx/Shared/Astral/Materials/T_NeutralMasks.T_NeutralMasks'   # reused from the Peony run
mask = unreal.load_asset(mask_path)
assert isinstance(mask, unreal.Texture2D)
assert not mask.get_editor_property('srgb')
assert mask.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_MASKS
mask_file = stage / 'editor/Content' / (mask_path.split('.')[0].removeprefix('/Game/') + '.uasset')
inputs[str(mask_file)] = hashlib.sha256(mask_file.read_bytes()).hexdigest()
for spec in manifest['materials']:
    material = unreal.load_asset(spec['material'])
    assert isinstance(material, unreal.Material)
    assert material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
    assert material.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_UNLIT
    assert not material.get_editor_property('output_translucent_velocity')
    for prop in ('used_with_skeletal_mesh', 'used_with_morph_targets', 'used_with_clothing'):
        assert material.get_editor_property(prop)
    scalars = {str(n): edit.get_material_default_scalar_parameter_value(material, n)
               for n in edit.get_scalar_parameter_names(material)}
    vectors = {}
    for name in edit.get_vector_parameter_names(material):
        color = edit.get_material_default_vector_parameter_value(material, name)
        vectors[str(name)] = [color.r, color.g, color.b, color.a]
    assert dict(scalar=scalars, vector=vectors) == spec['authored_parameters']
    assert scalars['CSS_AstralOpacity'] == scalars['CSS_AstralUseFixedTime'] == 0
    textures = {}
    for expression in unreal.CSSMaterialLibrary.get_expressions(material):
        if not isinstance(expression, unreal.MaterialExpressionTextureBase):
            continue
        assert isinstance(expression, unreal.MaterialExpressionTextureSampleParameter)
        name = str(expression.get_editor_property('parameter_name'))
        value = (expression.get_editor_property('texture').get_path_name(),
                 str(expression.get_editor_property('sampler_type')))
        assert name not in textures or textures[name] == value
        textures[name] = value
    expected_textures = {t['parameter']: (t['placeholder'], t['sampler']) for t in spec['textures']}
    if textures != expected_textures:
        (stage / 'texture-mismatch.json').write_text(json.dumps(dict(
            material=spec['material'], expected=expected_textures, actual=textures), indent=2) + '\n')
        raise AssertionError('Saved texture binding differs: ' + spec['material'])
    dependencies = sorted(str(d) for d in registry.get_dependencies(spec['material'].split('.')[0], options))
    assert dependencies == spec['dependencies']
    package = spec['material'].split('.')[0]
    path = stage / 'editor/Content' / (package.removeprefix('/Game/') + '.uasset')
    inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    rows.append(dict(material=spec['material'], scalar_count=len(scalars), vector_count=len(vectors),
                     texture_count=len(textures), preserved=True))

for spec in manifest['variants']:
    instance = unreal.load_asset(spec['variant'])
    assert isinstance(instance, unreal.MaterialInstanceConstant)
    parent = instance.get_editor_property('parent')
    assert parent.get_path_name() == spec['parent']
    assert parent.get_editor_property('two_sided') == spec['parent_two_sided']
    properties = instance.get_editor_property('base_property_overrides')
    assert properties.get_editor_property('override_two_sided')
    assert properties.get_editor_property('two_sided') == spec['variant_two_sided']
    assert spec['variant_two_sided'] != spec['parent_two_sided']
    package = spec['variant'].split('.')[0]
    path = stage / 'editor/Content' / (package.removeprefix('/Game/') + '.uasset')
    inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()

output.write_text(json.dumps(dict(passed=True, materials=rows, variants=expected_count, inputs=inputs,
    scope='Fresh-process editor parameter, texture, dependency and culling verification. '
          'Not compiled-shader, rendered-appearance or runtime acceptance.'), indent=2) + '\n')
if v2:
    for package, expected in manifest['packages'].items():
        path = stage / 'editor/Content' / (package.removeprefix('/Game/') + '.uasset')
        assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, package
    (stage / 'verified.json').write_text(json.dumps(dict(passed=True,
        authored_sha256=hashlib.sha256((stage / 'authored.json').read_bytes()).hexdigest(),
        reloaded_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        packages=len(manifest['packages'])), indent=2) + '\n')
