"""Check packaged companion shaders and dependency closure after an independent readback."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("readback", type=Path)
parser.add_argument("manifest", type=Path)
parser.add_argument("variants", type=Path)
parser.add_argument("report", type=Path)
args = parser.parse_args()
if args.report.exists():
    raise FileExistsError(args.report)
data = json.loads(args.readback.read_text())
manifest = json.loads(args.manifest.read_text())
variants = json.loads(args.variants.read_text())["materials"]
expected = {row["material"].split(".")[0]: row for row in manifest["materials"]}
if len(expected) != 12 or len(variants) != 12 or {row["parent"].split(".")[0] for row in variants} != set(expected):
    raise ValueError("Expected twelve audited companion parents and their culling variants")
rows = []
prefix = "/Game/CSS/SharedAssets/Astral/"


def check_references(value):
    if isinstance(value, dict):
        for child in value.values():
            check_references(child)
    elif isinstance(value, list):
        for child in value:
            check_references(child)
    elif isinstance(value, str):
        if "CSSSharedAuthoring" in value:
            raise ValueError("Editor module dependency in cooked material")
        if value.startswith("/Game/") and not value.startswith(prefix):
            raise ValueError(f"Outfit or preview package dependency: {value}")


def shader_maps(material, refraction):
    platforms = {}
    for resource in material["LoadedMaterialResources"]:
        shader_map = resource["LoadedShaderMap"]
        platform = shader_map["ShaderPlatform"]
        if platform in platforms or not shader_map["Code"]["ShaderHashes"]:
            raise ValueError("Missing or duplicate shader resource")
        meshes = shader_map["Content"]["OrderedMeshShaderMaps"]
        required = {"TGPUSkinVertexFactoryDefault", "TGPUSkinAPEXClothVertexFactoryDefault"}
        if not required.issubset({row["VertexFactoryTypeName"] for row in meshes}):
            raise ValueError(f"Missing skin/cloth shader permutations: {material['Package']}")
        for mesh in meshes:
            if mesh["VertexFactoryTypeName"] not in required:
                continue
            types = set(mesh["ShaderTypes"])
            if "TBasePassPSFNoLightMapPolicy" not in types:
                raise ValueError("Missing base-pass pixel shader")
            if refraction and "FDistortionMeshPS" not in types:
                raise ValueError("Missing eye distortion pixel shader")
        platforms[platform] = len(shader_map["Code"]["ShaderHashes"])
    if set(platforms) != {"SP_PCD3D_SM5", "SP_PCD3D_SM6"}:
        raise ValueError(f"Unexpected shader targets: {platforms}")
    return platforms


for package, spec in expected.items():
    exports = data[package]
    if len(exports) != 1 or exports[0]["Type"] != "Material":
        raise ValueError(f"Unexpected material exports: {package}")
    material = exports[0]
    check_references(material)
    properties = material["Properties"]
    if properties["BlendMode"] != "BLEND_Translucent" or properties["ShadingModel"] != "MSM_Unlit":
        raise ValueError("Companion lost ghost shading/fade mode")
    for flag in ("bUsedWithSkeletalMesh", "bUsedWithMorphTargets", "bUsedWithClothing"):
        if not properties.get(flag, False):
            raise ValueError(f"Missing mesh usage {flag}: {package}")
    cached = material["CachedExpressionData"]
    scalar_infos = cached["RuntimeEntries"]["ParameterInfoSet"]
    scalars = dict(zip((row["Name"] for row in scalar_infos), cached["ScalarValues"], strict=True))
    if scalars["CSS_AstralOpacity"] != 0 or scalars["CSS_AstralUseFixedTime"] != 0:
        raise ValueError("Unsafe default fade/time")
    texture_infos = cached["RuntimeEntries[3]"]["ParameterInfoSet"]
    textures = dict(zip((row["Name"] for row in texture_infos), cached["TextureValues"], strict=True))
    if set(textures) != {row["parameter"] for row in spec["textures"]}:
        raise ValueError("Texture binding contract changed during cooking")
    for binding in spec["textures"]:
        if textures[binding["parameter"]]["AssetPathName"] != binding["placeholder"]:
            raise ValueError("Unexpected default texture")
    platforms = shader_maps(material, spec["name"] == "M_EyeRefraction")
    rows.append({"material": package, "platform_shader_counts": platforms})

for spec in variants:
    parent = spec["parent"].split(".")[0]
    package = spec["variant"].split(".")[0]
    exports = data[package]
    if len(exports) != 1 or exports[0]["Type"] != "MaterialInstanceConstant":
        raise ValueError("Unexpected culling variant exports")
    material = exports[0]
    check_references(material)
    properties = material["Properties"]
    overrides = properties["BasePropertyOverrides"]
    if properties["Parent"]["ObjectPath"].split(".")[0] != parent:
        raise ValueError("Culling variant parent mismatch")
    if not properties.get("bHasStaticPermutationResource", False):
        raise ValueError("Culling variant has no static shaders")
    if not overrides.get("bOverride_TwoSided", False) or overrides.get("TwoSided", False) != spec["variant_two_sided"]:
        raise ValueError("Culling variant override mismatch")
    if any(value for key, value in overrides.items() if key.startswith("bOverride_") and key != "bOverride_TwoSided"):
        raise ValueError("Culling variant changes unrelated base properties")
    if data[parent][0]["Properties"].get("TwoSided", False) != spec["parent_two_sided"] or spec["parent_two_sided"] == spec["variant_two_sided"]:
        raise ValueError("Culling variants do not cover both sides")
    rows.append({"material": package, "platform_shader_counts": shader_maps(
        material, expected[parent]["name"] == "M_EyeRefraction")})
all_expected = set(expected) | {row["variant"].split(".")[0] for row in variants}
all_expected |= {path.split(".")[0] for path in manifest["neutral_textures"]}
all_expected.add(prefix + "ABP_CopyPose")
if set(data) != all_expected:
    raise ValueError("Packaged asset set differs from manifest")
args.report.write_text(json.dumps({"passed": True, "materials": rows,
    "scope": "Packaged Windows shader maps, skin/cloth permutations, defaults and reference closure. No live game execution."}, indent=2) + "\n")
print(f"Verified {len(rows)} Windows companion materials")
