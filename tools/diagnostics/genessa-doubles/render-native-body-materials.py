"""Exercise native alpha cutouts and ghost fades on a controlled sphere."""
import hashlib
import json
from pathlib import Path
import struct
import zlib
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "native-body-renders-1"
OUT.mkdir(exist_ok=False)
report = json.loads((ROOT / "native-body-materials.json").read_text())
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
PREFIX = "/Game/CSS/UnholyGenessa/AstralBodyFixture1/"


def check_sources():
    for path, digest in report["protected"].items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


def texture(name, alpha):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    png = OUT / (name + ".png")
    pixels = b"".join(b"\0" + bytes((255, 255, 255, alpha)) * 16 for _ in range(16))
    png.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 16, 16, 8, 6, 0, 0, 0))
                   + chunk(b"IDAT", zlib.compress(pixels)) + chunk(b"IEND", b""))
    task = unreal.AssetImportTask()
    for key, value in {"filename": str(png), "destination_path": PREFIX,
                       "destination_name": name, "automated": True,
                       "replace_existing": False, "save": False}.items():
        task.set_editor_property(key, value)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = task.get_editor_property("imported_object_paths")
    if len(paths) != 1:
        raise RuntimeError("Expected one fixture texture")
    return LIB.load_asset(paths[0])


check_sources()
materials = {row["kind"]: LIB.load_asset(row["path"]) for row in report["materials"]}
if not all(materials.values()):
    raise RuntimeError("Missing body parents")
textures = {value: texture(f"T_Alpha{value}", value) for value in (0, 255)}
backdrop_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_Backdrop", PREFIX, unreal.Material, unreal.MaterialFactoryNew())
backdrop_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
black = EDIT.create_material_expression(backdrop_material, unreal.MaterialExpressionConstant)
if not EDIT.connect_material_property(black, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect backdrop")
EDIT.recompile_material(backdrop_material)
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials([*materials.values(), backdrop_material]))
if not compiled["passed"]:
    raise RuntimeError(compiled)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
where = unreal.Vector(170, 270, 100)
camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, where)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(where, unreal.Vector()), False)
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
for key, value in {"capture_every_frame": False, "capture_on_movement": False,
                   "capture_source": unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR,
                   "primitive_render_mode": unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST,
                   "fov_angle": 35.}.items():
    capture.set_editor_property(key, value)
settings = capture.get_editor_property("post_process_settings")
for key, value in {"override_auto_exposure_method": True,
    "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL,
    "override_auto_exposure_apply_physical_camera_exposure": True,
    "auto_exposure_apply_physical_camera_exposure": False,
    "override_auto_exposure_bias": True, "auto_exposure_bias": 0.}.items():
    settings.set_editor_property(key, value)
capture.set_editor_property("post_process_settings", settings)
target = unreal.RenderingLibrary.create_render_target2d(world, 256, 256,
    unreal.TextureRenderTargetFormat.RTF_RGBA16F, unreal.LinearColor(0, 0, 0, 1))
capture.set_editor_property("texture_target", target)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
component = actor.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(LIB.load_asset("/Engine/BasicShapes/Sphere"))
backdrop = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, -200, 0))
backdrop.set_actor_scale3d(unreal.Vector(10, .1, 10))
background = backdrop.get_component_by_class(unreal.StaticMeshComponent)
background.set_static_mesh(LIB.load_asset("/Engine/BasicShapes/Cube"))
background.set_material(0, backdrop_material)
capture.show_only_actor_components(actor)
capture.show_only_actor_components(backdrop)
rows = []
states = ("opaque", "full", "half", "zero", "removed", "below_clip", "at_clip",
          "above_clip", "red_channel", "zero_channel", "alpha_zero", "opaque_alpha_zero", "time")
for form, corrupted in (("faithful", 0.), ("stray", 1.)):
    for state in states:
        kind = "opaque" if state in ("opaque", "opaque_alpha_zero") else "masked"
        dynamic = component.create_dynamic_material_instance(0, materials[kind])
        dynamic.set_scalar_parameter_value("CSS_AstralUseFixedTime", 1.)
        dynamic.set_scalar_parameter_value("CSS_AstralFixedTime", 1. if state == "time" else 0.)
        dynamic.set_scalar_parameter_value("CSS_AstralCorrupted", corrupted)
        dynamic.set_scalar_parameter_value("CSS_AstralOpacity", .5 if state == "half" else 0. if state == "zero" else 1.)
        dynamic.set_scalar_parameter_value("Opacity Strength", {
            "below_clip": .3332, "at_clip": .3333, "above_clip": .3334}.get(state, 1.))
        dynamic.set_scalar_parameter_value("CSS_AstralClipValue", .3333)
        channel = (1., 0., 0., 0.) if state == "red_channel" else (0., 0., 0., 0. if state == "zero_channel" else 1.)
        dynamic.set_vector_parameter_value("OpacityMask_Channel", unreal.LinearColor(*channel))
        dynamic.set_texture_parameter_value("BaseColorMap  non VT", textures[0 if "alpha_zero" in state else 255])
        component.set_visibility(state != "removed", False)
        unreal.UGMaterialLibrary.flush_component_updates(component)
        for _ in range(8):
            capture.capture_scene()
            unreal.UGMaterialLibrary.flush_rendering()
        name = f"{form}-{state}.exr"
        unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name)
        image = OUT / name
        if not image.is_file() or not image.stat().st_size:
            raise RuntimeError(f"Missing capture: {image}")
        rows.append({"form": form, "state": state, "image": name,
                     "sha256": hashlib.sha256(image.read_bytes()).hexdigest()})
check_sources()
(OUT / "captures.json").write_text(json.dumps({"images": rows, "compilation": compiled,
    "source_files_unchanged": True, "resolution": 256,
    "scope": "Vulkan sphere with known white RGB and zero/full texture alpha. Native mask boundary, selector behavior and fade checks only. Not a full outfit, native image comparison or Windows validation."}, indent=2) + "\n")
unreal.log("CSS_NATIVE_BODY_CAPTURED")
