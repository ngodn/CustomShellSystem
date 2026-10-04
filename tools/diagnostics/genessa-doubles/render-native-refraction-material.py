"""Check eye distortion and its disappearance on a patterned background."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from native_ghost_graph import custom, node

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "native-refraction-renders-1"
OUT.mkdir(exist_ok=False)
report = json.loads((ROOT / "native-refraction-material.json").read_text())


def check_sources():
    for path, digest in report["protected"].items():
        if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source changed: {path}")


check_sources()
parent = unreal.EditorAssetLibrary.load_asset(report["material"])
backdrop_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_Checker", "/Game/CSS/UnholyGenessa/AstralRefractionFixture1/",
    unreal.Material, unreal.MaterialFactoryNew())
if not parent or not backdrop_material:
    raise RuntimeError("Missing refraction fixture material")
backdrop_material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
checker = custom(backdrop_material,
    "float c=fmod(floor(UV.x*64.0)+floor(UV.y*64.0),2.0); float v=lerp(0.04,0.25,c); return float4(v,v,v,1);",
    {"UV": node(backdrop_material,"TextureCoordinate",coordinate_index=0)})
if not unreal.MaterialEditingLibrary.connect_material_property(checker,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect checker")
unreal.MaterialEditingLibrary.recompile_material(backdrop_material)
compiled = json.loads(unreal.UGMaterialLibrary.finish_materials([parent,backdrop_material]))
if not compiled["passed"]:
    raise RuntimeError(compiled)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for command in ("r.AllowOcclusionQueries 0", "r.AntiAliasingMethod 0", "r.BloomQuality 0", "r.RefractionQuality 2"):
    unreal.SystemLibrary.execute_console_command(world,command)
where = unreal.Vector(0,300,0)
camera = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D,where)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(where,unreal.Vector()),False)
capture = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
for key,value in {"capture_every_frame":False,"capture_on_movement":False,
    "capture_source":unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR,
    "primitive_render_mode":unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST,
    "fov_angle":35.}.items():
    capture.set_editor_property(key,value)
settings = capture.get_editor_property("post_process_settings")
for key,value in {"override_auto_exposure_method":True,
    "auto_exposure_method":unreal.AutoExposureMethod.AEM_MANUAL,
    "override_auto_exposure_apply_physical_camera_exposure":True,
    "auto_exposure_apply_physical_camera_exposure":False,
    "override_auto_exposure_bias":True,"auto_exposure_bias":0.}.items():
    settings.set_editor_property(key,value)
capture.set_editor_property("post_process_settings",settings)
target = unreal.RenderingLibrary.create_render_target2d(world,256,256,
    unreal.TextureRenderTargetFormat.RTF_RGBA16F,unreal.LinearColor(0,0,0,1))
capture.set_editor_property("texture_target",target)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
component = actor.get_component_by_class(unreal.StaticMeshComponent)
component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Sphere"))
backdrop = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,-120,0))
backdrop.set_actor_scale3d(unreal.Vector(10,.1,10))
background = backdrop.get_component_by_class(unreal.StaticMeshComponent)
background.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
background.set_material(0,backdrop_material)
capture.show_only_actor_components(actor)
capture.show_only_actor_components(backdrop)
rows = []
for form,corrupted in (("faithful",0.),("stray",1.)):
    for state in ("removed","zero_fade","full","half","neutral_ior","reflection_zero","visible_surface"):
        dynamic = component.create_dynamic_material_instance(0,parent)
        for name,value in {"CSS_AstralUseFixedTime":1.,"CSS_AstralFixedTime":0.,
            "CSS_AstralCorrupted":corrupted,"CSS_AstralOpacity":0. if state=="zero_fade" else .5 if state=="half" else 1.,
            "IOR":1. if state=="neutral_ior" else 1.3683993,
            "ReflectionIntensity":0. if state=="reflection_zero" else .604878,
            "Opacity":.1 if state=="visible_surface" else 0.}.items():
            dynamic.set_scalar_parameter_value(name,value)
        component.set_visibility(state!="removed",False)
        unreal.UGMaterialLibrary.flush_component_updates(component)
        for _ in range(8):
            capture.capture_scene()
            unreal.UGMaterialLibrary.flush_rendering()
        name = f"{form}-{state}.exr"
        unreal.RenderingLibrary.export_render_target(world,target,str(OUT),name)
        path = OUT/name
        if not path.is_file() or not path.stat().st_size:
            raise RuntimeError(f"Missing image: {name}")
        rows.append({"form":form,"state":state,"image":name,
                     "sha256":hashlib.sha256(path.read_bytes()).hexdigest()})
check_sources()
(OUT/"captures.json").write_text(json.dumps({"images":rows,"compilation":compiled,
    "source_files_unchanged":True,"resolution":256,
    "scope":"Vulkan sphere, patterned opaque background, CW eye IOR and opacity. Verifies visible distortion at zero opacity and full removal at zero native fade. Not the complete hairstyle/face, native rendered equivalence or DX12."},indent=2)+"\n")
unreal.log("CSS_NATIVE_REFRACTION_CAPTURED")
