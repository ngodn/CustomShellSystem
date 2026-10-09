"""Author the reusable travelling pulse used by garment glow. UE 5.6.1 Python.

The caller supplies rest-space UVs, time and controls. Masks, colors and garment
textures belong to each outfit. This creates a new function, never edits one.
"""
import unreal

PACKAGE='/Game/CSS/eins0fx/Shared/Materials/MF_TrimFlow'


def create():
    edit=unreal.MaterialEditingLibrary
    lib=unreal.EditorAssetLibrary
    if lib.does_asset_exist(PACKAGE):
        raise ValueError('Use a fresh project for the shared flow candidate')
    function=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'MF_TrimFlow',PACKAGE.rsplit('/',1)[0],unreal.MaterialFunction,unreal.MaterialFunctionFactoryNew())
    if not function:
        raise RuntimeError('Could not create shared trim-flow function')

    def node(kind,**props):
        value=edit.create_material_expression_in_function(function,getattr(unreal,'MaterialExpression'+kind))
        if not value:
            raise RuntimeError('Could not create '+kind)
        for name,prop in props.items():
            value.set_editor_property(name,prop)
        return value

    def wire(a,b,pin):
        if not edit.connect_material_expressions(a,'',b,pin):
            raise RuntimeError('Could not connect flow input '+pin)

    def op(kind,a,b=None,**props):
        value=node(kind,**props)
        wire(a,value,'A' if kind in ('Add','Multiply','Subtract') else '')
        if b is not None:
            wire(b,value,'B')
        return value

    inputs={}
    for order,(name,kind) in enumerate([('UV','FUNCTION_INPUT_VECTOR2'),('Clock','FUNCTION_INPUT_SCALAR'),
            ('FlowSpeed','FUNCTION_INPUT_SCALAR'),('FlowPhase','FUNCTION_INPUT_SCALAR')]):
        inputs[name]=node('FunctionInput',input_name=name,input_type=getattr(unreal.FunctionInputType,kind),
                          sort_priority=order)
    u=node('ComponentMask',r=True,g=False,b=False,a=False)
    v=node('ComponentMask',r=False,g=True,b=False,a=False)
    wire(inputs['UV'],u,'');wire(inputs['UV'],v,'')
    warp=op('Sine',op('Add',op('Multiply',u,const_b=3.7),op('Multiply',v,const_b=1.3)),period=1.)
    spatial=op('Add',op('Add',op('Multiply',v,const_b=4.),op('Multiply',u,const_b=1.2)),
               op('Multiply',warp,const_b=.24))
    clock=op('Add',op('Multiply',inputs['Clock'],inputs['FlowSpeed']),inputs['FlowPhase'])
    wave=op('Sine',op('Subtract',spatial,clock),period=1.)
    positive=op('Add',op('Multiply',wave,const_b=.5),const_b=.5)
    pulse=node('Power',const_exponent=6.)
    wire(positive,pulse,'Base')
    output=node('FunctionOutput',output_name='Pulse',sort_priority=0)
    wire(pulse,output,'')
    edit.layout_material_function_expressions(function)
    edit.update_material_function(function)
    if not lib.save_loaded_asset(function,False):
        raise RuntimeError('Could not save shared flow function')
    return function
