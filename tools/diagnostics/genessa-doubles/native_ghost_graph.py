"""Shared UE graph construction for the private Astral material experiments."""
from pathlib import Path
import unreal

EDIT = unreal.MaterialEditingLibrary


def node(material, kind, **properties):
    result = EDIT.create_material_expression(material, getattr(unreal, "MaterialExpression" + kind))
    if result is None:
        raise RuntimeError(f"Could not create {kind}")
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def wire(source, target, pin, output=""):
    if not EDIT.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f"Could not connect {source} to {target}.{pin}")


def custom(material, code, connections):
    inputs = []
    for name in connections:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    result = node(material, "Custom", code=code,
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4, inputs=inputs)
    for name, expression in connections.items():
        wire(expression, result, name)
    return result


def normalized_position(material, position, bounds):
    difference = node(material, "Subtract")
    wire(position, difference, "A")
    wire(bounds, difference, "B", "Min")
    normalized = node(material, "Divide")
    wire(difference, normalized, "A")
    wire(bounds, normalized, "B", "Extents")
    return normalized


def build_native_ghost(material, noise_texture):
    kernel_path = Path(__file__).with_name("native-ghost-response.hlsl")
    world_position = node(material, "WorldPosition")
    local_position = node(material, "TransformPosition",
        transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
        transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    wire(world_position, local_position, "")
    local01 = normalized_position(material, local_position, node(material, "ObjectLocalBounds"))
    pre_skinned = node(material, "PreSkinnedPosition")
    interpolator = node(material, "VertexInterpolator")
    wire(pre_skinned, interpolator, "VS")
    pre01 = normalized_position(material, interpolator, node(material, "PreSkinnedLocalBounds"))
    time = node(material, "Time", ignore_pause=False)
    fixed_time = node(material, "ScalarParameter", parameter_name="CSS_AstralFixedTime", default_value=0.)
    time_override = node(material, "ScalarParameter", parameter_name="CSS_AstralUseFixedTime", default_value=0.)
    selected_time = node(material, "LinearInterpolate")
    wire(time, selected_time, "A")
    wire(fixed_time, selected_time, "B")
    wire(time_override, selected_time, "Alpha")

    inputs = {
        "Local01": local01,
        "Pre01": pre01,
        "NormalWS": node(material, "VertexNormalWS"),
        "CameraVectorWS": node(material, "CameraVectorWS"),
        "TimeSeconds": selected_time,
        "Opacity": node(material, "ScalarParameter", parameter_name="CSS_AstralOpacity", default_value=0.),
        "Corrupted": node(material, "ScalarParameter", parameter_name="CSS_AstralCorrupted", default_value=0.),
        "PixelDepth": node(material, "PixelDepth"),
        "DepthFade": node(material, "DepthFade", opacity_default=1., fade_distance_default=5.),
        "Exposure": node(material, "EyeAdaptation"),
        "NoiseTex": node(material, "TextureObjectParameter", parameter_name="CSS_AstralNoise", texture=noise_texture),
    }
    code = "struct CSSAstralKernel {\n" + kernel_path.read_text() + "\n};\n" + """
    CSSAstralKernel kernel;
    float noise = kernel.CSSAstralNoise(NoiseTex, NoiseTexSampler, Local01, NormalWS, TimeSeconds);
    float radius = saturate(length(Pre01 - float3(0.5f, 0.325f, 0.7f)));
    return kernel.CSSAstralResponse(noise, dot(NormalWS, CameraVectorWS), Local01.y,
        radius, Opacity, PixelDepth, DepthFade, Exposure, Corrupted > 0.5f);
    """
    response = custom(material, code, inputs)
    color = node(material, "ComponentMask", r=True, g=True, b=True, a=False)
    alpha = node(material, "ComponentMask", r=False, g=False, b=False, a=True)
    wire(response, color, "")
    wire(response, alpha, "")
    return color, alpha
