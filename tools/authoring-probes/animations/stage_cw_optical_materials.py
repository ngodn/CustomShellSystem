"""Create morph-compatible optical layers for the shared Commander White face."""
import hashlib
import json
import os
from pathlib import Path
import sys

import unreal

ROOT = Path(__file__).resolve().parents[3]
WORK = Path(os.environ['CSS_ANIM_WORK']).resolve()
CONTENT = Path(unreal.Paths.project_content_dir()).resolve()
PREFIX = '/Game/CSS/Locomotion/Faces/Optical1'
SOURCE = '/Game/CSS/Locomotion/Faces/SK_Face_CWCompact/SK_Face_CWE8'
TARGET = '/Game/CSS/Locomotion/Faces/SK_Face_CWCompact/SK_Face_CWE9'
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
sys.path.insert(0, str(ROOT/'tools/diagnostics/genessa-doubles'))
from native_ghost_graph import node, wire, custom


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk(package):
    return CONTENT/(package.removeprefix('/Game/')+'.uasset')


if not unreal.SystemLibrary.get_engine_version().startswith('5.6.1-'):
    raise ValueError('Expected UE 5.6.1')
if LIB.does_directory_exist(PREFIX) or LIB.does_asset_exist(TARGET):
    raise FileExistsError('Optical candidate already exists')
source = unreal.load_asset(SOURCE)
if not isinstance(source, unreal.SkeletalMesh):
    raise ValueError('Missing verified shared face')
protected = {str(disk(SOURCE)): sha(disk(SOURCE))}
slots = list(source.get_editor_property('materials'))
if len(slots) != 70:
    raise ValueError('Expected 70 released material slots')
eye_slots = {8: 'EyeMoisture-2', 10: 'Cornea-2', 45: 'Tear-1'}
for index, name in eye_slots.items():
    if str(slots[index].material_slot_name) != name:
        raise ValueError('Unexpected optical slot')
    path = disk(slots[index].material_interface.get_path_name().split('.')[0])
    protected[str(path)] = sha(path)

tools = unreal.AssetToolsHelpers.get_asset_tools()
material = tools.create_asset('M_Optical', PREFIX, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
material.set_editor_property('two_sided', True)
material.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE)
material.set_editor_property('refraction_method', unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
material.set_editor_property('automatically_set_usage_in_editor', False)
for name in ('used_with_skeletal_mesh', 'used_with_morph_targets', 'used_with_clothing'):
    material.set_editor_property(name, True)


def scalar(name, value):
    return node(material, 'ScalarParameter', parameter_name=name, default_value=value)


kernel_path = ROOT/'tools/diagnostics/genessa-doubles/native-refraction-response.hlsl'
protected[str(kernel_path)] = sha(kernel_path)
kernel = 'struct CSSOpticalKernel {\n'+kernel_path.read_text()+'\n};\nCSSOpticalKernel k;\n'
refraction = custom(material, kernel+'''
float ior = 1.0 + k.CSSRefractionDelta(dot(Normal,Camera),IOR);
return float4(ior,ior,ior,ior);
''', {'Normal': node(material, 'VertexNormalWS'), 'Camera': node(material, 'CameraVectorWS'),
      'IOR': scalar('IOR', 1.33)})
for expression, prop in ((scalar('Opacity', 0.0), unreal.MaterialProperty.MP_OPACITY),
                         (scalar('Roughness', 0.0), unreal.MaterialProperty.MP_ROUGHNESS),
                         (refraction, unreal.MaterialProperty.MP_REFRACTION)):
    if not EDIT.connect_material_property(expression, '', prop):
        raise ValueError('Could not connect optical graph')

# These authoring stand-ins resolve to the game's assets and are never packaged.
cube_path = '/Game/Sparta/MasterMaterials/Character_Materials/Eyes/Shared/HDRIs/spotslights3'
if LIB.does_asset_exist(cube_path):
    raise FileExistsError('Unexpected pre-existing optical cubemap stand-in')
cube = LIB.duplicate_asset('/Engine/EngineResources/DefaultTextureCube', cube_path)
if not LIB.save_loaded_asset(cube, False):
    raise ValueError('Could not save cubemap stand-in')
collection_path = '/Game/Sparta/Lighting/Blueprints/Deprecated/MPC_LightScenario'
if LIB.does_asset_exist(collection_path):
    raise FileExistsError('Unexpected pre-existing optical collection stand-in')
collection_data = json.loads((ROOT/'work/genessa-doubles/refraction-collection.json').read_text())
properties = collection_data[collection_path][0]['Properties']


folder, name = collection_path.rsplit('/', 1)
collection = tools.create_asset(name, folder, unreal.MaterialParameterCollection,
                               unreal.MaterialParameterCollectionFactoryNew())
for field, struct in (('ScalarParameters', unreal.CollectionScalarParameter),
                      ('VectorParameters', unreal.CollectionVectorParameter)):
    values = []
    for row in properties[field]:
        parameter = struct()
        parameter.set_editor_property('parameter_name', row['ParameterName'])
        value = row['DefaultValue']
        if isinstance(value, dict):
            value = unreal.LinearColor(*[value[channel] for channel in ('R', 'G', 'B', 'A')])
        parameter.set_editor_property('default_value', value)
        values.append(parameter)
    collection.set_editor_property('scalar_parameters' if field == 'ScalarParameters' else 'vector_parameters', values)
identity = unreal.CSSFacialLibrary.set_optical_collection_identity(collection, json.dumps(properties))
if not identity.startswith('PASS:'):
    raise ValueError(identity)
if not LIB.save_loaded_asset(collection, False):
    raise ValueError('Could not save collection stand-in')
boost = node(material, 'CollectionParameter', collection=collection,
             parameter_name='Reflection Boost Intensity')
reflection = custom(material, kernel+'''
float3 d = k.CSSRefractionDirection(Reflection.x, Reflection.y, Reflection.z,
    AngleX, AngleY, AngleZ, Pivot).xyz;
float3 e = ReflectionTex.Sample(ReflectionTexSampler,d).rgb * (0.25*Boost*Intensity);
return float4(e,1.0);
''', {'Reflection': node(material, 'ReflectionVectorWS'),
      'ReflectionTex': node(material, 'TextureObjectParameter', parameter_name='ReflectionMap', texture=cube),
      'AngleX': scalar('angle X', 0.0), 'AngleY': scalar('angle y', 0.0),
      'AngleZ': scalar('angle Z', 0.0), 'Pivot': scalar('pivot', 0.0),
      'Boost': boost, 'Intensity': scalar('ReflectionIntensity', 0.0)})
if not EDIT.connect_material_property(reflection, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise ValueError('Could not connect optical reflection')
if not LIB.save_loaded_asset(material, False):
    raise ValueError('Could not save optical material')
rows = []
for index in eye_slots:
    original = slots[index].material_interface
    instance = tools.create_asset('MI_Optical_'+str(index), PREFIX, unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(instance, material)
    values = list(original.get_editor_property('scalar_parameter_values'))
    instance.set_editor_property('scalar_parameter_values', values)
    if not LIB.save_loaded_asset(instance, False):
        raise ValueError('Could not save optical instance')
    rows.append({'slot': index, 'source': original.get_path_name(), 'target': instance.get_path_name()})
    slots[index].material_interface = instance
target = LIB.duplicate_asset(SOURCE, TARGET)
target.set_editor_property('materials', slots)
check = unreal.CSSFacialLibrary.verify_expression_shapes(source, target,
    str(ROOT/'work/cine-animation-port/cw-combined-basis-v2/facial-shapes.json'))
if not check.startswith('PASS:'):
    raise ValueError(check)
if not LIB.save_loaded_asset(target, False):
    raise ValueError('Could not save optical mesh candidate')
if any(sha(Path(path)) != digest for path, digest in protected.items()):
    raise ValueError('Protected source changed')
assets = [disk(TARGET), disk(PREFIX+'/M_Optical'),
          *(disk(PREFIX+'/MI_Optical_'+str(index)) for index in eye_slots)]
report = {'passed': True, 'source': SOURCE, 'target': TARGET, 'material': material.get_path_name(),
          'slots': rows,
          'protected': protected, 'assets': {str(p): sha(p) for p in assets},
          'geometry_check': check, 'installed': False,
          'excluded_authoring_stubs': [cube_path, collection_path]}
(WORK/'optical-stage.json').write_text(json.dumps(report, indent=2)+'\n')
print('CW_OPTICAL_STAGE_PASS', flush=True)
