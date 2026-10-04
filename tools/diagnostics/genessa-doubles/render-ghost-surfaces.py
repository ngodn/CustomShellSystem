"""Render source detail and native ghost, with separate terms for comparison.

Uses HDR readback, cube faces, and the authored garment for vertex-masked cloth.
These fixtures do not establish full-character sorting, DX12 behavior or
performance with multiple active summons.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
run = os.environ.get("CSS_ASTRAL_SURFACE_RUN", "1")
if not re.fullmatch(r"[a-zA-Z0-9_]{1,32}", run):
    raise ValueError("Invalid surface run label")
capture_run = os.environ.get("CSS_ASTRAL_SURFACE_CAPTURE_RUN", run)
if not re.fullmatch(r"[a-zA-Z0-9_]{1,32}", capture_run):
    raise ValueError("Invalid surface capture label")
OUT = ROOT / ("ghost-surface-renders-" + capture_run)
if capture_run == run:
    if not OUT.is_dir() or any(OUT.iterdir()):
        raise RuntimeError("Expected an empty directory from surface preparation")
else:
    OUT.mkdir(exist_ok=False)
float_capture = True
LIB = unreal.EditorAssetLibrary
PREFIX = "/Game/CSS/UnholyGenessa/AstralGhostSurface_" + run + "/"
fixture = json.loads((ROOT / ("ghost-surface-fixtures-" + run + ".json")).read_text())
if not fixture["source_files_unchanged"]:
    raise ValueError("Fixture preparation failed source checks")
protected = fixture["protected"]
protected.update(json.loads((ROOT / "garment-fixture-sources.json").read_text())["protected"])

def check_sources():
    for path, digest in protected.items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")

def load_fixture(path):
    if not path.startswith(PREFIX):
        raise ValueError(f"Unexpected fixture path: {path}")
    result = LIB.load_asset(path)
    if result is None:
        raise RuntimeError(f"Missing fixture: {path}")
    return result

check_sources()
interfaces = [load_fixture(path) for path in fixture["interfaces"]]
backdrop_material = load_fixture(fixture["backdrop"])
cards = [(row["source"], {role: load_fixture(path) for role, path in row["family"].items()},
          row["vectors"], row["scalars"]) for row in fixture["cards"]]
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials(interfaces))
if not compiled["passed"]:
    raise RuntimeError(compiled)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0"):
    unreal.SystemLibrary.execute_console_command(world, command)
for pitch, yaw, intensity in ((-70, -60, 8.), (-35, 120, 4.)):
    light = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 250),
        unreal.Rotator(pitch=pitch, yaw=yaw, roll=0))
    light.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(intensity)

camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 0, 250))
camera.set_actor_rotation(unreal.Rotator(pitch=-90), False)
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
capture.set_editor_property("capture_every_frame", False)
capture.set_editor_property("capture_on_movement", False)
capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR
                            if float_capture else unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
capture.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
capture.set_editor_property("fov_angle", 35.)
settings = capture.get_editor_property("post_process_settings")
settings.set_editor_property("override_auto_exposure_method", True)
settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
settings.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", 0.)
capture.set_editor_property("post_process_settings", settings)
resolution = 128 if float_capture else 512
target = unreal.RenderingLibrary.create_render_target2d(world, resolution, resolution,
    unreal.TextureRenderTargetFormat.RTF_RGBA16F if float_capture else unreal.TextureRenderTargetFormat.RTF_RGBA8,
    unreal.LinearColor(0., 0., 0., 1.))
capture.set_editor_property("texture_target", target)
cube = LIB.load_asset("/Engine/BasicShapes/Cube")
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector())
component = actor.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(cube)
garment = LIB.load_asset("/Game/CSS/UnholyGenessa/SK_EveW3")
if not isinstance(garment, unreal.SkeletalMesh):
    raise TypeError("Expected the staged Unholy Genessa garment mesh")
garment_slots = list(garment.get_editor_property("materials"))
fabric_slots = []
for index, slot in enumerate(garment_slots):
    overlay = slot.get_editor_property("overlay_material_interface")
    # The staged wing/gown uses the authored fabric overlays. Match their
    # parent, not numeric slot positions that change with mesh revisions.
    if overlay:
        parent = overlay
        for _ in range(16):
            if isinstance(parent, unreal.Material):
                break
            if not isinstance(parent, unreal.MaterialInstanceConstant):
                raise TypeError("Unexpected staged garment overlay")
            parent = parent.get_editor_property("parent")
        if parent.get_path_name().split(".")[0] == "/Game/CSS/UnholyGenessa/Fabric02/M_Fabric":
            fabric_slots.append(index)
    slot.set_editor_property("overlay_material_interface", None)
if not fabric_slots:
    raise RuntimeError("The fixture mesh has no authored Fabric02 cloth sections")
# This is an in-memory fixture edit. Source packages are read-only and checked
# by hash; no save is performed on the garment mesh.
garment.set_editor_property("materials", garment_slots)
garment_actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector())
garment_component = garment_actor.get_component_by_class(unreal.SkeletalMeshComponent)
garment_component.set_skeletal_mesh_asset(garment)
garment_component.suspend_clothing_simulation()
garment_component.set_editor_property("disable_cloth_simulation", True)
garment_component.set_component_tick_enabled(False)
garment_component.set_forced_lod(1)
for index in range(len(garment_slots)):
    garment_component.show_material_section(index, -1, index in fabric_slots, 0)
garment_component.set_visibility(False, False)
backdrop = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -100))
backdrop.set_actor_scale3d(unreal.Vector(10, 10, .1))
backdrop_component = backdrop.get_component_by_class(unreal.StaticMeshComponent)
backdrop_component.set_static_mesh(cube)
backdrop_component.set_material(0, backdrop_material)
capture.show_only_actor_components(actor)
capture.show_only_actor_components(garment_actor)
capture.show_only_actor_components(backdrop)
rows = []
for index, (path, family, vectors, scalars) in enumerate(cards):
    is_fabric = path == "/Game/CSS/UnholyGenessa/Fabric02/M_Fabric"
    component.set_visibility(not is_fabric, False)
    garment_component.set_visibility(is_fabric, False)
    active = garment_component if is_fabric else component
    if is_fabric:
        where = unreal.Vector(150, 570, 130)
        camera.set_actor_location_and_rotation(where, unreal.MathLibrary.find_look_at_rotation(
            where, unreal.Vector(0, 0, 105)), False, True)
        backdrop.set_actor_location(unreal.Vector(0, -150, 150), False, True)
        backdrop.set_actor_scale3d(unreal.Vector(10, .1, 10))
    else:
        camera.set_actor_location_and_rotation(unreal.Vector(0, 0, 250),
            unreal.Rotator(pitch=-90), False, True)
        backdrop.set_actor_location(unreal.Vector(0, 0, -100), False, True)
        backdrop.set_actor_scale3d(unreal.Vector(10, 10, .1))
    for form, corrupted in (("faithful", 0.), ("stray", 1.)):
        states = [(role, 1., 0.) for role in ("coverage", "ghost_only", "surface_only", "combined")]
        if vectors:
            states += [("palette_red", 1., 0.), ("palette_blue", 1., 0.)]
        if "GlowStrength" in scalars:
            states += [("glow", 1., 0.), ("surface_glow", 1., 0.)]
        if "FabricVisible" in scalars:
            states += [("fabric_hidden", 1., 0.), ("fabric_half", 1., 0.)]
        states += [("time", 1., 1.), ("half", .5, 0.), ("zero", 0., 0.), ("removed", 0., 0.)]
        for state, opacity, time in states:
            unreal.log(f"CSS_GHOST_SURFACE_BEGIN index={index} form={form} state={state}")
            parent = family["surface_only"] if state == "surface_glow" else family.get(state, family["combined"])
            dynamic = active.create_dynamic_material_instance(fabric_slots[0] if is_fabric else 0, parent)
            if is_fabric:
                for slot in fabric_slots[1:]:
                    active.set_material(slot, dynamic)
            dynamic.set_scalar_parameter_value("CSS_AstralOpacity", opacity)
            dynamic.set_scalar_parameter_value("CSS_AstralCorrupted", corrupted)
            dynamic.set_scalar_parameter_value("CSS_AstralUseFixedTime", 1.)
            dynamic.set_scalar_parameter_value("CSS_AstralFixedTime", time)
            # Freeze authored flow independently of the native ghost clock.
            for name, value in (("FlowSpeed", 0.), ("FlowPhase", .25), ("GlowStrength", 0.)):
                if name in scalars:
                    dynamic.set_scalar_parameter_value(name, value)
            if state.startswith("palette_"):
                tint = unreal.LinearColor(.8, .025, .025, 1.) if state == "palette_red" else unreal.LinearColor(.025, .025, .8, 1.)
                for name in vectors:
                    dynamic.set_vector_parameter_value(name, tint)
            if state in ("glow", "surface_glow"):
                dynamic.set_scalar_parameter_value("GlowStrength", 4.4)
                dynamic.set_vector_parameter_value("GlowColor", unreal.LinearColor(.4, .8, .2, 1.))
            if state == "fabric_hidden":
                dynamic.set_scalar_parameter_value("FabricVisible", 0.)
            if state == "fabric_half":
                dynamic.set_scalar_parameter_value("FabricOpacityScale", .5)
            active.set_visibility(state != "removed", False)
            unreal.UGMaterialLibrary.flush_component_updates(active)
            for _ in range(8):
                capture.capture_scene()
                unreal.UGMaterialLibrary.flush_rendering()
            name = f"{index:02}-{form}-{state}" + (".exr" if float_capture else ".png")
            # UE 5.6.1 Vulkan's ReadLinearColorPixels rounds through FColor.
            # EXR from RTF_RGBA16F instead uses ReadFloat16Pixels without that conversion.
            unreal.RenderingLibrary.export_render_target(world, target, str(OUT), name)
            image = OUT / name
            if not image.is_file() or image.stat().st_size == 0:
                raise RuntimeError(f"Empty capture: {image}")
            rows.append({"source": path, "form": form, "state": state,
                "opacity": opacity, "time": time, "image": name, "palette_parameters": vectors, "has_glow": "GlowStrength" in scalars,
                "geometry": "authored_garment" if is_fabric else "cube",
                "sha256": hashlib.sha256(image.read_bytes()).hexdigest()})
            unreal.log(f"CSS_GHOST_SURFACE_END index={index} form={form} state={state}")
check_sources()
(OUT / "captures.json").write_text(json.dumps({"images": rows, "compilation": compiled,
    "composition": fixture.get("composition", "lit_add"),
    "source_files_unchanged": True,
    "float_capture": float_capture, "resolution": resolution,
    "serial_rhi_requested": os.environ.get("CSS_ASTRAL_SERIAL_RHI", "0") == "1",
    "fabric_slots": fabric_slots,
    "scope": "Vulkan source-detail and native ghost composition on cube faces and authored cloth sections. Native-filter mode is unlit. Not full character, DX12, runtime binding or performance evidence."
}, indent=2) + "\n")
unreal.log("CSS_GHOST_SURFACE_CAPTURED")
