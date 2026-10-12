"""Author Peony ghost companions with source customization and shared-only imports."""
import hashlib
import json
import os
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'diagnostics/genessa-doubles'))
from native_ghost_graph import build_native_ghost, node, wire

stage = Path(os.environ['CSS_PEONY_ASTRAL_STAGE']).resolve(strict=True)
v2 = stage == Path('/mnt/eins0fxE/CSS-Development/peony-v2-astral1')
assert v2 or stage.name in ('peony-astral1', 'peony-astral2')
assert Path(unreal.Paths.project_dir()).resolve() == stage / 'editor'
assert unreal.SystemLibrary.get_engine_version().startswith('5.6.1-')
output = stage / 'authored.json'
assert not output.exists()
protected = json.loads((stage / 'inputs.json').read_text())
for filename in ('native_ghost_graph.py', 'native-ghost-response.hlsl'):
    path = Path(__file__).resolve().parents[1] / 'diagnostics/genessa-doubles' / filename
    protected[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()

edit = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
mp = unreal.MaterialProperty
prefix = '/Game/CSS/eins0fx/Shared/Astral/Materials/'
neutral = {name: unreal.load_asset(prefix + 'T_Neutral' + name) for name in ('Color', 'Normal', 'Linear')}
assert all(isinstance(t, unreal.Texture2D) for t in neutral.values())
assert not assets.does_asset_exist(prefix + 'T_NeutralMasks')
neutral['Masks'] = assets.duplicate_asset(prefix + 'T_NeutralLinear', prefix + 'T_NeutralMasks')
assert isinstance(neutral['Masks'], unreal.Texture2D)
neutral['Masks'].set_editor_properties(dict(srgb=False, compression_settings=unreal.TextureCompressionSettings.TC_MASKS))
assert assets.save_loaded_asset(neutral['Masks'], False)
rows, variants = [], []


def parameters(material):
    scalar = {str(n): edit.get_material_default_scalar_parameter_value(material, n)
              for n in edit.get_scalar_parameter_names(material)}
    vector = {}
    for name in edit.get_vector_parameter_names(material):
        color = edit.get_material_default_vector_parameter_value(material, name)
        vector[str(name)] = [color.r, color.g, color.b, color.a]
    return dict(scalar=scalar, vector=vector)


def save(asset):
    assert assets.save_loaded_asset(asset, False), asset.get_path_name()


try:
    source_paths = (['/Game/CSS/eins0fx/EveSB/PeonyV2/Materials/M_PeonyV2_' + name
                     for name in ('Fabric', 'FlowFabric', 'FlowPanel', 'FlowStrap', 'SourceGlow')]
                    + ['/Game/CSS/eins0fx/EveSB/PeonyV2/Stockings/Materials/M_PeonyV2_Stocking']
                    if v2 else ['/Game/CSS/eins0fx/EveSB/Peony/Materials/' + name
                                for name in ('M_PeonyFabric', 'M_PeonyHardware', 'M_PeonyPanels', 'M_PeonyStockings')])
    for source_path in source_paths:
        name = source_path.rsplit('/', 1)[1]
        source = unreal.load_asset(source_path)
        assert isinstance(source, unreal.Material)
        assert not unreal.CSSMaterialLibrary.get_expressions(source)
        before = parameters(source)
        assert not any(n.startswith('CSS_Astral') for names in before.values() for n in names)
        target_path = prefix + name
        assert not assets.does_asset_exist(target_path)
        material = assets.duplicate_asset(source_path, target_path)
        assert isinstance(material, unreal.Material)
        preserved = {p: (edit.get_material_property_input_node(material, getattr(mp, p)),
                         edit.get_material_property_input_node_output_name(material, getattr(mp, p)))
                     for p in ('MP_BASE_COLOR', 'MP_NORMAL', 'MP_ROUGHNESS', 'MP_METALLIC',
                               'MP_SPECULAR', 'MP_AMBIENT_OCCLUSION', 'MP_WORLD_POSITION_OFFSET')}
        original_emission = edit.get_material_property_input_node(material, mp.MP_EMISSIVE_COLOR)
        emission_pin = edit.get_material_property_input_node_output_name(material, mp.MP_EMISSIVE_COLOR)
        blend = material.get_editor_property('blend_mode')
        coverage, coverage_pin = None, ''
        if blend == unreal.BlendMode.BLEND_TRANSLUCENT:
            coverage = edit.get_material_property_input_node(material, mp.MP_OPACITY)
            coverage_pin = edit.get_material_property_input_node_output_name(material, mp.MP_OPACITY)
            assert coverage
        elif blend == unreal.BlendMode.BLEND_MASKED:
            original_mask = edit.get_material_property_input_node(material, mp.MP_OPACITY_MASK)
            mask_pin = edit.get_material_property_input_node_output_name(material, mp.MP_OPACITY_MASK)
            assert original_mask
            coverage = node(material, 'Step', const_y=material.get_editor_property('opacity_mask_clip_value'))
            wire(original_mask, coverage, 'X', mask_pin)
        else:
            assert blend == unreal.BlendMode.BLEND_OPAQUE
        color, ghost_alpha = build_native_ghost(material, neutral['Color'])
        alpha = node(material, 'Multiply', const_a=1.)
        if coverage:
            wire(coverage, alpha, 'A', coverage_pin)
        wire(ghost_alpha, alpha, 'B')
        custom_inputs = []
        for input_name in ('Base', 'Emission'):
            entry = unreal.CustomInput()
            entry.set_editor_property('input_name', input_name)
            custom_inputs.append(entry)
        detail = node(material, 'Custom', inputs=custom_inputs,
            output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
            code='float3 e = max(Emission, 0.0);\n'
                 'float peak = max(e.r, max(e.g, e.b));\n'
                 'return 0.5 * (1.0 + saturate(Base) + peak + e);')
        base, base_pin = preserved['MP_BASE_COLOR']
        assert base
        wire(base, detail, 'Base', base_pin)
        if original_emission is None:
            original_emission = node(material, 'Constant3Vector', constant=unreal.LinearColor(0, 0, 0, 1))
            emission_pin = ''
        wire(original_emission, detail, 'Emission', emission_pin)
        emission = node(material, 'Multiply')
        wire(detail, emission, 'A')
        wire(color, emission, 'B')
        assert edit.connect_material_property(emission, '', mp.MP_EMISSIVE_COLOR)
        assert edit.connect_material_property(alpha, '', mp.MP_OPACITY)
        material.set_editor_properties(dict(shading_model=unreal.MaterialShadingModel.MSM_UNLIT,
            blend_mode=unreal.BlendMode.BLEND_TRANSLUCENT, dither_opacity_mask=False,
            # Ghost depth fade reads scene depth, incompatible with velocity/depth output.
            output_translucent_velocity=False,
            translucency_pass=unreal.MaterialTranslucencyPass.MTP_BEFORE_DOF,
            automatically_set_usage_in_editor=False, used_with_skeletal_mesh=True,
            used_with_morph_targets=True, used_with_clothing=True))
        textures = {}
        expressions = unreal.CSSMaterialLibrary.get_expressions(material)
        assert expressions
        for expression in expressions:
            if not isinstance(expression, unreal.MaterialExpressionTextureBase):
                continue
            assert isinstance(expression, unreal.MaterialExpressionTextureSampleParameter)
            parameter = str(expression.get_editor_property('parameter_name'))
            sampler = expression.get_editor_property('sampler_type')
            texture = expression.get_editor_property('texture')
            assert isinstance(texture, unreal.Texture2D)
            if sampler == unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL:
                replacement = neutral['Normal']
            elif sampler == unreal.MaterialSamplerType.SAMPLERTYPE_COLOR:
                replacement = neutral['Color']
            elif sampler == unreal.MaterialSamplerType.SAMPLERTYPE_MASKS:
                replacement = neutral['Masks']
            else:
                assert sampler == unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
                replacement = neutral['Linear']
            binding = dict(parameter=parameter, candidate_default=texture.get_path_name(),
                           placeholder=replacement.get_path_name(), sampler=str(sampler),
                           binding='fixed_game_texture' if parameter == 'CSS_AstralNoise' else 'source_parameter')
            if parameter == 'CSS_AstralNoise':
                binding['game_texture'] = '/Game/Sparta/FX/Textures/Noises/BnW/T_noise_0017.T_noise_0017'
            assert parameter not in textures or textures[parameter] == binding
            textures[parameter] = binding
            expression.set_editor_property('texture', replacement)
        assert 'CSS_AstralNoise' in textures
        for prop, (expression, pin) in preserved.items():
            assert edit.get_material_property_input_node(material, getattr(mp, prop)) == expression
            assert edit.get_material_property_input_node_output_name(material, getattr(mp, prop)) == pin
        edit.recompile_material(material)
        after = parameters(material)
        for kind in before:
            assert all(after[kind][n] == v for n, v in before[kind].items())
        assert after['scalar']['CSS_AstralOpacity'] == 0
        assert after['scalar']['CSS_AstralUseFixedTime'] == 0
        save(material)
        two_sided = material.get_editor_property('two_sided')
        variant_name = 'MI_' + name + ('_SingleSided' if two_sided else '_TwoSided')
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(variant_name, prefix.rstrip('/'),
            unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        assert instance
        edit.set_material_instance_parent(instance, material)
        overrides = instance.get_editor_property('base_property_overrides')
        overrides.set_editor_properties(dict(override_two_sided=True, two_sided=not two_sided))
        instance.set_editor_property('base_property_overrides', overrides)
        edit.update_material_instance(instance)
        save(instance)
        rows.append(dict(name=name, source_parent=source_path, material=material.get_path_name(),
                         source_blend=str(blend), textures=list(textures.values()),
                         original_parameters=before, authored_parameters=after,
                         coverage=coverage.get_class().get_name() if coverage else 'ConstantOne'))
        variants.append(dict(parent=material.get_path_name(), parent_two_sided=two_sided,
                             variant=instance.get_path_name(), variant_two_sided=not two_sided))
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([prefix], force_rescan=True)
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
        include_hard_package_references=True, include_searchable_names=False,
        include_soft_management_references=False, include_hard_management_references=False)
    for row in rows:
        row['dependencies'] = sorted(str(d) for d in registry.get_dependencies(row['material'].split('.')[0], options))
        assert all(d.startswith(('/Engine/', '/Script/Engine', '/Game/CSS/eins0fx/Shared/')) for d in row['dependencies'])
finally:
    for path, checksum in protected.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest() == checksum, path

document = dict(materials=rows, variants=variants, protected=protected,
    additional_shared_assets=[neutral['Masks'].get_path_name()],
    scope='Editor ghost companion graphs only. Compilation, rendering, Windows cook and runtime catalog integration remain unverified.')
if v2:
    packages = {row['material'].split('.')[0] for row in rows}
    packages.update(row['variant'].split('.')[0] for row in variants)
    packages.update(texture.get_path_name().split('.')[0] for texture in neutral.values())
    packages.add('/Game/CSS/eins0fx/Shared/Materials/MF_TrimFlow')
    document.update(packages={package: hashlib.sha256((stage / 'editor/Content' /
        (package.removeprefix('/Game/') + '.uasset')).read_bytes()).hexdigest()
        for package in sorted(packages)}, sources=protected,
        engine=unreal.SystemLibrary.get_engine_version())
output.write_text(json.dumps(document, indent=2) + '\n')
