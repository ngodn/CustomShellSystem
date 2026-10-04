"""Render native ghost response, time variation and fade on a controlled sphere.

This tests the reconstructed UE graph, not native-versus-copy image equivalence.
"""
import hashlib
import json
from pathlib import Path
import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "native-ghost-renders"
OUT.mkdir(exist_ok=False)
report = json.loads((ROOT / "native-ghost-material.json").read_text())
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
PREFIX = "/Game/CSS/UnholyGenessa/AstralNative1/"


def check_sources():
    for path, digest in report["protected"].items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
parent = LIB.load_asset(report["material"])
assert parent
backdrop_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_RenderBackdrop", PREFIX, unreal.Material, unreal.MaterialFactoryNew())
assert backdrop_material
backdrop_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
color = EDIT.create_material_expression(backdrop_material, unreal.MaterialExpressionConstant3Vector)
color.set_editor_property("constant", unreal.LinearColor(.025, .025, .025, 1.))
assert EDIT.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
EDIT.recompile_material(backdrop_material)
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials([parent, backdrop_material]))
assert compiled["passed"], compiled
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
camera_location = unreal.Vector(170, 270, 100)
camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, camera_location)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera_location, unreal.Vector()), False)
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
capture.set_editor_property("capture_every_frame", False)
capture.set_editor_property("capture_on_movement", False)
capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
capture.set_editor_property("fov_angle", 35.)
settings = capture.get_editor_property("post_process_settings")
for limit in ("min", "max"):
    settings.set_editor_property("override_auto_exposure_" + limit + "_brightness", True)
    settings.set_editor_property("auto_exposure_" + limit + "_brightness", 1.)
capture.set_editor_property("post_process_settings", settings)
target = unreal.RenderingLibrary.create_render_target2d(world, 512, 512,
    unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(.025, .025, .025, 1.))
capture.set_editor_property("texture_target", target)

actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
component = actor.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(LIB.load_asset("/Engine/BasicShapes/Sphere"))
material = component.create_dynamic_material_instance(0, parent)
material.set_scalar_parameter_value("CSS_AstralUseFixedTime", 1.)
# A solid background keeps zero/removed tests from submitting an empty scene.
backdrop = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, -200, 0))
backdrop.set_actor_scale3d(unreal.Vector(10, 0.1, 10))
background_component = backdrop.get_component_by_class(unreal.StaticMeshComponent)
background_component.set_static_mesh(LIB.load_asset("/Engine/BasicShapes/Cube"))
background_component.set_material(0, backdrop_material)
capture.show_only_actor_components(actor)
capture.show_only_actor_components(backdrop)

rows = []
for form, corrupted in (("faithful", 0.), ("stray", 1.)):
    material.set_scalar_parameter_value("CSS_AstralCorrupted", corrupted)
    for state, opacity, time in (("full_t0", 1., 0.), ("full_t1", 1., 1.),
                                 ("half", .5, 0.), ("zero", 0., 0.), ("removed", 0., 0.)):
        unreal.log(f"CSS_ASTRAL_NATIVE_CAPTURE_BEGIN form={form} state={state}")
        material.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
        material.set_scalar_parameter_value("CSS_AstralFixedTime", time)
        component.set_visibility(state != "removed", False)
        unreal.UGMaterialLibrary.flush_component_updates(component)
        for _ in range(8):
            capture.capture_scene()
            unreal.UGMaterialLibrary.flush_rendering()
        name = form + "-" + state + ".png"
        unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name)
        image = OUT / name
        assert image.is_file() and image.stat().st_size
        rows.append({"form": form, "state": state, "opacity": opacity, "time": time,
                     "image": name, "sha256": hashlib.sha256(image.read_bytes()).hexdigest()})
        unreal.log(f"CSS_ASTRAL_NATIVE_CAPTURE_END form={form} state={state}")
check_sources()
(OUT / "captures.json").write_text(json.dumps({
    "images": rows, "compilation": compiled, "source_files_unchanged": True,
    "scope": "Vulkan sphere fixture. Not a native-versus-copy comparison, outfit integration, or DX12 validation."
}, indent=2) + "\n")
unreal.log("CSS_ASTRAL_NATIVE_CAPTURED")
