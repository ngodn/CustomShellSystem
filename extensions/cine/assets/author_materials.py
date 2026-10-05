"""Create CINE's breakdown materials under /Game/CSSX/CINE. UE 5.6.1 editor Python.

Run through author.sh (UnrealEditor-Cmd -run=pythonscript). Refuses to overwrite: start from an
empty Content folder. Everything depends only on /Engine and on MPC_Cine, so the cooked pak
carries no game asset.

One wipe drives every material. MPC_Cine holds WipeT0/WipeDur/WipeFrom/WipeTo (+ soft edge and
line width). Each material computes the wipe position from the game clock itself, so a wipe
animates on the GPU with no per-frame call from the mod: CINE writes the four numbers once.
With the defaults (From = To = 1) everything is fully revealed.
"""
import json
from pathlib import Path
import unreal

ROOT = "/Game/CSSX/CINE"
MATERIALS = ROOT + "/Materials"
REPORT = Path(unreal.Paths.project_dir()).resolve() / "cine-materials.json"
EDIT = unreal.MaterialEditingLibrary
LIB = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty

if not unreal.SystemLibrary.get_engine_version().startswith("5.6.1-"):
    raise RuntimeError("Expected UE 5.6.1")
if REPORT.exists() or LIB.does_directory_exist(ROOT):
    raise FileExistsError("CINE asset output already exists")


def node(material, cls, x, y, **properties):
    expression = EDIT.create_material_expression(material, cls, x, y)
    if expression is None:
        raise RuntimeError(f"Cannot create {cls.__name__}")
    for key, value in properties.items():
        expression.set_editor_property(key, value)
    return expression


def link(source, output, target, pin):
    if not EDIT.connect_material_expressions(source, output, target, pin):
        raise RuntimeError(f"Cannot connect {source.get_name()}:{output} -> {target.get_name()}:{pin}")


def out(source, output, material_property):
    if not EDIT.connect_material_property(source, output, material_property):
        raise RuntimeError(f"Cannot connect {source.get_name()} to {material_property}")


class Graph:
    """Small helpers so a graph reads like the maths it computes."""

    def __init__(self, material):
        self.m = material
        self.x = -2400
        self.y = 0

    def n(self, cls, **properties):
        self.y += 120
        if self.y > 1800:
            self.y = 0
            self.x += 260
        return node(self.m, cls, self.x, self.y, **properties)

    def const(self, value):
        return self.n(unreal.MaterialExpressionConstant, r=float(value))

    def scalar(self, name, default):
        return self.n(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=float(default))

    def vector(self, name, r, g, b, a=1.0):
        return self.n(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                      default_value=unreal.LinearColor(r, g, b, a))

    def two(self, cls, a, b, pin_a="A", pin_b="B"):
        e = self.n(cls)
        link(*self._src(a), e, pin_a)
        link(*self._src(b), e, pin_b)
        return e

    def one(self, cls, source, pin="", **properties):
        e = self.n(cls, **properties)
        link(*self._src(source), e, pin)
        return e

    @staticmethod
    def _src(value):
        return value if isinstance(value, tuple) else (value, "")

    def add(self, a, b): return self.two(unreal.MaterialExpressionAdd, a, b)
    def sub(self, a, b): return self.two(unreal.MaterialExpressionSubtract, a, b)
    def mul(self, a, b): return self.two(unreal.MaterialExpressionMultiply, a, b)
    def div(self, a, b): return self.two(unreal.MaterialExpressionDivide, a, b)
    def max(self, a, b): return self.two(unreal.MaterialExpressionMax, a, b)
    def saturate(self, a): return self.one(unreal.MaterialExpressionSaturate, a)
    def one_minus(self, a): return self.one(unreal.MaterialExpressionOneMinus, a)
    def abs(self, a): return self.one(unreal.MaterialExpressionAbs, a)
    def rgb(self, a): return self.one(unreal.MaterialExpressionComponentMask, a, r=True, g=True, b=True, a=False)
    def red(self, a): return self.one(unreal.MaterialExpressionComponentMask, a, r=True, g=False, b=False, a=False)

    def lerp(self, a, b, alpha):
        e = self.n(unreal.MaterialExpressionLinearInterpolate)
        link(*self._src(a), e, "A")
        link(*self._src(b), e, "B")
        link(*self._src(alpha), e, "Alpha")
        return e

    def collection(self, mpc, name):
        return self.n(unreal.MaterialExpressionCollectionParameter, collection=mpc, parameter_name=name)

    def wipe(self, mpc):
        """(revealed, line): 1 where the wipe has passed this pixel, and a thin band on its edge."""
        time = self.n(unreal.MaterialExpressionTime)
        progress = self.saturate(self.div(self.sub(time, self.collection(mpc, "WipeT0")),
                                          self.max(self.collection(mpc, "WipeDur"), self.const(0.001))))
        # smoothstep: p * p * (3 - 2p)
        eased = self.mul(self.mul(progress, progress), self.sub(self.const(3.0), self.mul(progress, self.const(2.0))))
        position = self.lerp(self.collection(mpc, "WipeFrom"), self.collection(mpc, "WipeTo"), eased)
        soft = self.max(self.collection(mpc, "WipeSoft"), self.const(0.0001))
        # Stretch by the soft edge so 0 hides everything and 1 reveals everything.
        edge = self.mul(position, self.add(soft, self.const(1.0)))
        screen = self.n(unreal.MaterialExpressionScreenPosition)
        u = self.red((screen, "ViewportUV"))
        distance = self.sub(edge, u)
        revealed = self.saturate(self.div(distance, soft))
        width = self.max(self.collection(mpc, "WipeLine"), self.const(0.00001))
        band = self.saturate(self.one_minus(self.div(self.abs(distance), width)))
        # No line while nothing or everything is revealed.
        moving = self.mul(self.saturate(self.mul(position, self.const(1000.0))),
                          self.saturate(self.mul(self.one_minus(position), self.const(1000.0))))
        return revealed, self.mul(band, moving)


def new_material(name):
    material = TOOLS.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Cannot create {name}")
    return material


def mesh_usage(material):
    # A cooked game swaps in the default material when a usage flag is missing.
    for flag in ("used_with_skeletal_mesh", "used_with_morph_targets", "used_with_clothing",
                 "used_with_instanced_static_meshes", "used_with_spline_meshes",
                 "used_with_geometry_collections"):
        material.set_editor_property(flag, True)


def finish(material):
    EDIT.layout_material_expressions(material)
    EDIT.recompile_material(material)
    if not LIB.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Cannot save {material.get_path_name()}")
    return material.get_path_name()


# ---------------------------------------------------------------- parameter collection
mpc = TOOLS.create_asset("MPC_Cine", ROOT, unreal.MaterialParameterCollection,
                         unreal.MaterialParameterCollectionFactoryNew())
if not isinstance(mpc, unreal.MaterialParameterCollection):
    raise RuntimeError("Cannot create MPC_Cine")
scalars = []
for name, default in (("WipeT0", 0.0), ("WipeDur", 1.0), ("WipeFrom", 1.0), ("WipeTo", 1.0),
                      ("WipeSoft", 0.004), ("WipeLine", 0.0015)):
    parameter = unreal.CollectionScalarParameter()
    parameter.set_editor_property("parameter_name", name)
    parameter.set_editor_property("default_value", default)
    scalars.append(parameter)
mpc.set_editor_property("scalar_parameters", scalars)
# Each parameter gets its own GUID from its constructor (the field is protected in Python);
# check.py confirms the ids are distinct in the cooked asset.
if not LIB.save_loaded_asset(mpc, only_if_is_dirty=False):
    raise RuntimeError("Cannot save MPC_Cine")

created = {"collection": mpc.get_path_name(), "materials": {}}

# ---------------------------------------------------------------- wireframe (overlay or slot)
m = new_material("M_Cine_Wire")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("two_sided", True)
m.set_editor_property("wireframe", True)
mesh_usage(m)
g = Graph(m)
revealed, _ = g.wipe(mpc)
color = g.vector("Color", 0.05, 0.9, 1.0)
out(g.mul(g.rgb(color), g.scalar("Glow", 1.0)), "", MP.MP_EMISSIVE_COLOR)
out(revealed, "", MP.MP_OPACITY_MASK)
# Lift the lines off the surface they sit on, or they fight with it for depth.
out(g.mul(g.n(unreal.MaterialExpressionVertexNormalWS), g.scalar("Push", 0.06)), "", MP.MP_WORLD_POSITION_OFFSET)
created["materials"]["wire"] = finish(m)

# ---------------------------------------------------------------- clay (overlay or slot)
m = new_material("M_Cine_Clay")
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("two_sided", True)
mesh_usage(m)
g = Graph(m)
revealed, _ = g.wipe(mpc)
out(g.rgb(g.vector("BaseColor", 0.42, 0.42, 0.42)), "", MP.MP_BASE_COLOR)
out(g.scalar("Roughness", 0.65), "", MP.MP_ROUGHNESS)
out(g.scalar("Specular", 0.35), "", MP.MP_SPECULAR)
out(g.const(0.0), "", MP.MP_METALLIC)
out(revealed, "", MP.MP_OPACITY_MASK)
out(g.mul(g.n(unreal.MaterialExpressionVertexNormalWS), g.scalar("Push", 0.02)), "", MP.MP_WORLD_POSITION_OFFSET)
created["materials"]["clay"] = finish(m)

# ---------------------------------------------------------------- see-through colour (rig shapes)
m = new_material("M_Cine_XRay")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("two_sided", True)
m.set_editor_property("disable_depth_test", True)
mesh_usage(m)
g = Graph(m)
revealed, _ = g.wipe(mpc)
color = g.vector("Color", 1.0, 0.85, 0.1)
out(g.mul(g.rgb(color), g.scalar("Glow", 1.0)), "", MP.MP_EMISSIVE_COLOR)
out(g.mul(g.scalar("Opacity", 0.85), revealed), "", MP.MP_OPACITY)
created["materials"]["xray"] = finish(m)

# ---------------------------------------------------------------- solid colour, depth tested (rig shapes behind the body)
m = new_material("M_Cine_Solid")
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
m.set_editor_property("two_sided", True)
mesh_usage(m)
g = Graph(m)
revealed, _ = g.wipe(mpc)
color = g.vector("Color", 1.0, 0.85, 0.1)
out(g.mul(g.rgb(color), g.scalar("Glow", 1.0)), "", MP.MP_EMISSIVE_COLOR)
out(revealed, "", MP.MP_OPACITY_MASK)
created["materials"]["solid"] = finish(m)


# ---------------------------------------------------------------- buffer passes (post-process)
def buffer_material(name, location, lighting):
    material = new_material(name)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    material.set_editor_property("blendable_location", location)
    g = Graph(material)
    revealed, line = g.wipe(mpc)
    ID = unreal.SceneTextureId

    def texture(identifier):
        return (g.n(unreal.MaterialExpressionSceneTexture, scene_texture_id=identifier), "Color")

    scene = g.rgb(texture(ID.PPI_POST_PROCESS_INPUT0))
    base = g.rgb(texture(ID.PPI_BASE_COLOR))
    normal = g.add(g.mul(g.rgb(texture(ID.PPI_WORLD_NORMAL)), g.const(0.5)), g.const(0.5))
    depth = g.one_minus(g.saturate(g.div(g.red(texture(ID.PPI_SCENE_DEPTH)), g.max(g.scalar("DepthRange", 3000.0), g.const(1.0)))))
    layers = [("W_BaseColor", base), ("W_Normal", normal), ("W_Depth", depth),
              ("W_Roughness", g.red(texture(ID.PPI_ROUGHNESS))), ("W_Metallic", g.red(texture(ID.PPI_METALLIC))),
              ("W_Specular", g.red(texture(ID.PPI_SPECULAR))), ("W_AO", g.red(texture(ID.PPI_AMBIENT_OCCLUSION)))]
    if lighting:
        # Scene colour over base colour: roughly the light that fell on the surface.
        lit = g.mul(g.div(scene, g.max(base, g.const(0.04))), g.scalar("LightingScale", 0.18))
        layers.append(("W_Lighting", lit))
    mix = None
    total = None
    for parameter, value in layers:
        weight = g.scalar(parameter, 0.0)
        term = g.mul(value, weight)
        mix = term if mix is None else g.add(mix, term)
        total = weight if total is None else g.add(total, weight)
    # Gain and Gamma place the pass on the output curve; set live per location.
    graded = g.two(unreal.MaterialExpressionPower, g.max(g.mul(mix, g.scalar("Gain", 1.0)), g.const(0.0)),
                   g.scalar("Gamma", 1.0), "Base", "Exp")
    shown = g.lerp(scene, graded, g.mul(g.saturate(total), revealed))
    edge = g.mul(g.rgb(g.vector("LineColor", 1.0, 1.0, 1.0)), g.mul(line, g.scalar("LineGlow", 1.0)))
    out(g.add(shown, edge), "", MP.MP_EMISSIVE_COLOR)
    return finish(material)


LOC = unreal.BlendableLocation
created["materials"]["buffer_post"] = buffer_material("PP_Cine_Buffer_Post", LOC.BL_SCENE_COLOR_AFTER_TONEMAPPING, False)
created["materials"]["buffer_pre"] = buffer_material("PP_Cine_Buffer_Pre", LOC.BL_SCENE_COLOR_BEFORE_DOF, True)

# ---------------------------------------------------------------- no game or project dependency
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([ROOT], force_rescan=True)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
for key, path in created["materials"].items():
    dependencies = sorted(str(x) for x in registry.get_dependencies(path.split(".")[0], options))
    unexpected = [x for x in dependencies if not x.startswith(("/Engine/", "/Script/", ROOT + "/"))]
    if unexpected:
        raise RuntimeError(f"{key} depends on {unexpected}")
created["engine"] = unreal.SystemLibrary.get_engine_version()
with REPORT.open("x") as stream:
    stream.write(json.dumps(created, indent=2) + "\n")
print(f"Created MPC_Cine and {len(created['materials'])} CINE materials")
