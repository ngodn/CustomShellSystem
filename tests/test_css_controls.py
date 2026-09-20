import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from css_controls import validate,embed,verify_resources,lint_convention

class ControlTests(unittest.TestCase):
    def test_independent_body_regions(self):
        body=dict(id='chest-motion',name='Chest motion',kind='rig',solver='angular_body',regions=['brust001','brust002'],
                  frequency=dict(min=.5,max=6,default=2),damping_ratio=dict(min=.1,max=2,default=.7),
                  motion_amount=dict(min=0,max=1,default=1))
        hair=dict(id='hair-motion',name='Hair motion',kind='rig',stiffness=dict(min=100,max=250,default=150),
                  damping=dict(min=12,max=24,default=18),gravity=dict(min=-.2,max=.2,default=0))
        recipe=dict(schema=1,controls=[body,hair],palettes=[dict(id='off',name='Off',values={'chest-motion':[3,.9,.5,0]})])
        validate(recipe)
        other=copy.deepcopy(body);other.update(id='belly-motion',regions=['belly'])
        recipe['controls'].append(other);validate(recipe)
        for key,value in [('regions',[]),('regions',['head']),('regions',['brust001','brust001']),
                          ('solver','unknown'),('stiffness',{}),('gravity',{}),
                          ('frequency',dict(min=0,max=6,default=2)),('enabled',1)]:
            bad=copy.deepcopy(recipe);bad['controls'][0][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate(bad)
        bad=copy.deepcopy(recipe);bad['controls'][2]['regions']=['brust001']
        with self.assertRaises(ValueError):validate(bad)
        bad=copy.deepcopy(recipe);bad['palettes'][0]['values']['chest-motion']=[2,.7,1,.5]
        with self.assertRaises(ValueError):validate(bad)
        bad=copy.deepcopy(recipe);bad['controls'].append(dict(id='old-chest',name='Old chest',kind='spring',nodes=['brust001'],
            frequency=dict(min=1,max=3,default=2),damping_ratio=dict(min=.1,max=1,default=.7)))
        with self.assertRaises(ValueError):validate(bad)

    def test_rig_inputs_and_palette_contract(self):
        recipe=dict(schema=1,controls=[dict(id='hair-motion',name='Hair motion',kind='rig',
            stiffness=dict(min=100,max=250,default=150),damping=dict(min=12,max=24,default=18),
            gravity=dict(min=-.2,max=.2,default=0),enabled=True)])
        validate(recipe)
        recipe['palettes']=[dict(id='off',name='Off',values={'hair-motion':[150,18,0,0]})]
        validate(recipe)
        for field,value in [('enabled',1),('nodes',[]),('bindings',[]),('angular_spring',{}),
                            ('default',[150,18,0,1]),('stiffness',dict(min=0,max=250,default=150)),
                            ('damping',dict(min=12,max=121,default=18))]:
            bad=copy.deepcopy(recipe);bad['controls'][0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):validate(bad)
        for value in ([99,18,0,1],[150,25,0,1],[150,18,1,1],[150,18,0,.5],[150,18,0,True],
                      [float('nan'),18,0,1]):
            bad=copy.deepcopy(recipe);bad['palettes'][0]['values']['hair-motion']=value
            with self.subTest(value=value),self.assertRaises(ValueError):validate(bad)
        bad=copy.deepcopy(recipe);bad['controls'].append(dict(bad['controls'][0],id='duplicate'))
        with self.assertRaises(ValueError):validate(bad)

    def test_dynamics_solver_ranges_and_palettes(self):
        recipe=dict(schema=1,controls=[dict(id='hair',name='Hair dynamics',kind='dynamics',
            nodes=['CSS_Hair_Ponytail_01'],angular_spring=dict(min=0,max=1000,default=80),
            damping=dict(min=.7,max=1,default=.8),gravity=dict(min=-5,max=5,default=.1))],
            palettes=[dict(id='float',name='Floating',values={'hair':[120,.9,-.5,1]})])
        self.assertEqual(validate(recipe),set())
        for value in ([1000,1,-5,1],[0,.7,5,1]):
            good=copy.deepcopy(recipe);good['palettes'][0]['values']['hair']=value
            validate(good)
        for value in ([1001,.8,0,1],[80,.69,0,1],[80,.8,-6,1],[80,.8,0,.5],[True,.8,0,1]):
            bad=copy.deepcopy(recipe);bad['palettes'][0]['values']['hair']=value
            with self.subTest(value=value),self.assertRaises(ValueError):validate(bad)
        for key in ('frequency','damping_ratio','default','max_displacement','bindings'):
            bad=copy.deepcopy(recipe);bad['controls'][0][key]=[]
            with self.subTest(key=key),self.assertRaises(ValueError):validate(bad)
        for key in ('angular_spring','damping','gravity'):
            for value in (True,None,'0',float('inf')):
                bad=copy.deepcopy(recipe);bad['controls'][0][key]['default']=value
                with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate(bad)
            bad=copy.deepcopy(recipe);del bad['controls'][0][key]
            with self.assertRaises(ValueError):validate(bad)
        duplicate=copy.deepcopy(recipe);second=copy.deepcopy(recipe['controls'][0]);second['id']='other'
        duplicate['controls'].append(second)
        with self.assertRaises(ValueError):validate(duplicate)

    def test_secondary_motion_metadata_matches_native_limits(self):
        recipe=dict(schema=1,controls=[dict(id='motion',name='Motion',kind='spring',
            nodes=['hair_root'],frequency=dict(min=.5,max=3,default=1.5),
            damping_ratio=dict(min=.2,max=1,default=.5))])
        validate(recipe)
        for key,low,high in [('world_damping',0,1),('limit_angle',0,180),
                             ('collision_radius',0,100),('gravity_scale',-5,5)]:
            for number in (low,0,high):
                good=copy.deepcopy(recipe);good['controls'][0][key]=number
                validate(good)
            for value in (low-1,high+1,True,None,'0',[],float('nan'),float('inf')):
                bad=copy.deepcopy(recipe);bad['controls'][0][key]=value
                with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate(bad)
            stray=dict(schema=1,controls=[dict(id='gloss',name='Gloss',kind='scalar',
                default=[1,0,0,1],bindings=[dict(slot=0,parameter='Gloss')],**{key:0})])
            with self.assertRaises(ValueError):validate(stray)
        for axis in ('none','x','y','z'):
            good=copy.deepcopy(recipe);good['controls'][0]['planar_constraint']=axis
            validate(good)
        for axis in ('w',None,True,1,[]):
            bad=copy.deepcopy(recipe);bad['controls'][0]['planar_constraint']=axis
            with self.subTest(axis=axis),self.assertRaises(ValueError):validate(bad)

    def test_glow_and_opacity_package_kinds_match_runtime(self):
        recipe=dict(schema=1,controls=[
            dict(id='glow',name='Glow',kind='glow',default=[5,0,0,1],
                 bindings=[dict(slot=1,parameter='Emission')]),
            dict(id='opacity',name='Opacity',kind='opacity',default=[.5,0,0,1],
                 bindings=[dict(slot=2,parameter='Opacity')])])
        validate(recipe)
        recipe['palettes']=[dict(id='bright',name='Bright',values={'glow':[12,0,0,1]})]
        validate(recipe)
        for control,field,value in [(0,'pulse_hz',float('nan')),(0,'pulse_hz',-1),
                                    (0,'combat_reactive','yes'),(1,'default',[2,0,0,1])]:
            bad=copy.deepcopy(recipe);bad['controls'][control][field]=value
            with self.subTest(field=field,value=value),self.assertRaises(ValueError):validate(bad)
        for index in (0,1):
            bad=copy.deepcopy(recipe);bad['controls'][index]['bindings']=[]
            with self.assertRaises(ValueError):validate(bad)

    def setUp(self):
        self.recipe=dict(schema=1,controls=[dict(id='cloth',name='Clothing',default=[1,1,1,1])],surfaces=[dict(id='body',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'})],palettes=[dict(id='red',name='Crimson',values={'cloth':[.6,.1,.2,1]})])

    def test_mask_embedding_and_corruption(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);metadata=root/'metadata';metadata.mkdir()
            Image.new('RGBA',(1024,1024),(255,255,255,0)).save(root/'dye-cloth.png')
            path=root/'outfit.customize.json';path.write_text(json.dumps(dict(id='test',customize=self.recipe)))
            manifest=dict(id='test',catalog=dict(outfits=[dict(id='test')]))
            embed(path,manifest,metadata);verify_resources(manifest,metadata)
            self.assertEqual(manifest['catalog']['outfits'][0]['customize'],self.recipe)
            # 1.0 renamed the block. A recipe still written as "colors" embeds the same way.
            legacy=root/'legacy.colors.json';legacy.write_text(json.dumps(dict(id='test',colors=self.recipe)))
            old_manifest=dict(id='test',catalog=dict(outfits=[dict(id='test')]))
            embed(legacy,old_manifest,metadata)
            self.assertEqual(old_manifest['catalog']['outfits'][0]['customize'],self.recipe)
            both=root/'both.customize.json'
            both.write_text(json.dumps(dict(id='test',customize=self.recipe,colors=self.recipe)))
            with self.assertRaisesRegex(ValueError,'once'):embed(both,dict(id='test',catalog=dict(outfits=[dict(id='test')])),metadata)
            image=metadata/'dye-cloth.png';data=bytearray(image.read_bytes());data[-1]^=1;image.write_bytes(data)
            with self.assertRaisesRegex(ValueError,'checksum'):verify_resources(manifest,metadata)

    def test_invalid_bindings_and_parts(self):
        variants=[]
        bad=copy.deepcopy(self.recipe);bad['surfaces'][0]['layers']['cloth']='../dye-cloth.png';variants.append(bad)
        bad=copy.deepcopy(self.recipe);bad['surfaces'][0]['slots']=[128];variants.append(bad)
        bad=copy.deepcopy(self.recipe);bad['controls'][0]['default'][3]=2;variants.append(bad)
        bad=copy.deepcopy(self.recipe);bad['palettes'][0]['values']['cloth'][3]=.5;variants.append(bad)
        bad=copy.deepcopy(self.recipe);bad['surfaces'][0]['layers']['unknown']='dye-other.png';variants.append(bad)
        bad=copy.deepcopy(self.recipe);bad['controls'][0]['default'][0]=float('nan');variants.append(bad)
        bad=copy.deepcopy(self.recipe);bad['surfaces'].append(dict(id='duplicate',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'}));variants.append(bad)
        for bad in variants:
            with self.subTest(recipe=bad),self.assertRaises(ValueError):validate(bad)

    def test_variant_resources_share_only_identical_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);metadata=root/'metadata';metadata.mkdir()
            Image.new('RGBA',(1024,1024),(255,255,255,0)).save(root/'dye-cloth.png')
            recipe=root/'customize.json';recipe.write_text(json.dumps(dict(id='test',customize=self.recipe)))
            variants=[{'id':'one'},{'id':'two'}]
            manifest=dict(id='test',catalog=dict(outfits=[dict(id='test',variants=variants)]))
            for variant in variants:embed(recipe,manifest,metadata,variant)
            verify_resources(manifest,metadata)
            self.assertEqual(len(manifest['resources']),1)
            self.assertNotIn('customize',manifest['catalog']['outfits'][0])
            Image.new('RGBA',(1024,1024),(0,0,0,255)).save(root/'dye-cloth.png')
            with self.assertRaisesRegex(ValueError,'collision'):embed(recipe,manifest,metadata,variants[1])

    def test_convention_fields_are_optional_but_checked(self):
        control=self.recipe['controls'][0]
        for good in [dict(group='outfit'),dict(group='body'),dict(role='garment'),dict(hue_locked=True),
                     dict(kind='color'),dict(group='outfit',role='metal',hue_locked=True)]:
            colors=copy.deepcopy(self.recipe);colors['controls'][0].update(good)
            with self.subTest(control=good):validate(colors)
        for bad in [dict(group='ornaments'),dict(group=''),dict(role='a b'),dict(role='x'*33),
                    dict(hue_locked='yes'),dict(kind='scalar'),dict(kind='intensity',type='color')]:
            colors=copy.deepcopy(self.recipe);colors['controls'][0].update(bad)
            with self.subTest(control=bad),self.assertRaises(ValueError):validate(colors)
        self.assertEqual(control,self.recipe['controls'][0])

    def test_intensity_is_a_scalar_everywhere_type_would_be(self):
        # `kind: intensity` is the convention's spelling of `type: scalar`, so a
        # control declaring it must be refused as a dye layer and take one value.
        colors=copy.deepcopy(self.recipe)
        colors['controls'][0].update(kind='intensity',max=5,default=[1.5,0,0,1])
        with self.assertRaises(ValueError):validate(colors)
        colors['surfaces']=[];colors['controls'][0]['bindings']=[dict(slot=0,parameter='Intensity')]
        colors['palettes']=[dict(id='red',name='Crimson',values={'cloth':[4,0,0,1]})]
        validate(colors)
        colors['palettes'][0]['values']['cloth']=[6,0,0,1]
        with self.assertRaises(ValueError):validate(colors)

    def test_lint_convention_reports_what_validate_allows(self):
        # validate() stays permissive so published packages keep loading; the lint
        # is what a package's own builder runs, and it has to catch all of this.
        self.assertTrue(lint_convention(self.recipe))       # no role, no group, one palette
        colors=dict(schema=1,controls=[
            dict(id='cloth',name='Garment',group='outfit',role='garment',default=[1,1,1,1]),
            dict(id='skin',name='Skin',group='body',role='skin',default=[1,1,1,1])],
            surfaces=[dict(id='body',parameter='BaseColorMap  non VT',slots=[0],
                           layers={'cloth':'dye-cloth.png','skin':'dye-skin.png'})],
            palettes=[dict(id='red',name='Crimson',values={'cloth':[.6,.1,.2,1]}),
                      dict(id='blue',name='Midnight',values={'cloth':[.1,.1,.3,1]})])
        validate(colors);self.assertEqual(lint_convention(colors),[])
        # A palette may leave the body alone, but never the outfit.
        short=copy.deepcopy(colors);short['palettes'][0]['values']={}
        self.assertIn('does not set cloth',' '.join(lint_convention(short)))
        # A role has to agree with the group and hue locking it implies.
        wrong=copy.deepcopy(colors);wrong['controls'][1]['group']='outfit'
        self.assertIn('belongs to the body group',' '.join(lint_convention(wrong)))
        loose=copy.deepcopy(colors);loose['controls'][1]['hue_locked']=False
        self.assertIn('hue_locked=True',' '.join(lint_convention(loose)))
        unknown=copy.deepcopy(colors);unknown['controls'][0]['role']='frock'
        self.assertIn('shared vocabulary',' '.join(lint_convention(unknown)))

    def test_toggle_occlusion(self):
        recipe=dict(schema=1,controls=[dict(id='boots',name='Boots',kind='toggle',
            default=[1,0,0,1],sections=[20],occludes_sections=[21,22])])
        validate(recipe)
        for field in ('sections','occludes_sections'):
            for invalid in ([],[-1],[128],[1.5],[True],[4294967296],'21'):
                with self.subTest(field=field,value=invalid):
                    bad=copy.deepcopy(recipe);bad['controls'][0][field]=invalid
                    with self.assertRaises(ValueError):validate(bad)
        for invalid in ([21,21],[20]):
            bad=copy.deepcopy(recipe);bad['controls'][0]['occludes_sections']=invalid
            with self.assertRaisesRegex(ValueError,'distinct'):validate(bad)
        bad=copy.deepcopy(recipe);bad['controls'][0]['kind']='scalar'
        del bad['controls'][0]['sections']
        with self.assertRaisesRegex(ValueError,'toggle'):validate(bad)

    def test_control_kinds(self):
        """1.0: colour is one kind among several, and every kind but colour is a number."""
        from css_controls import kind_of
        base=dict(schema=1,controls=[
            dict(id='cloth',name='Garment',role='garment',default=[1,1,1,1]),
            dict(id='gloss',name='Sheen',kind='scalar',role='gloss',default=[.4,0,0,1],min=0,max=1,
                 bindings=[dict(slot=0,parameter='Roughness')]),
            dict(id='hood',name='Hood',kind='toggle',role='piece',default=[1,0,0,1],sections=[2,3])],
            surfaces=[dict(id='body',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'})],
            palettes=[dict(id='red',name='Crimson',values={'cloth':[.6,.1,.2,1]})])
        validate(base)
        self.assertEqual([kind_of(c) for c in base['controls']],['color','scalar','toggle'])

        # A package written before the split said type=scalar and meant a strength.
        self.assertEqual(kind_of(dict(type='scalar')),'intensity')
        self.assertEqual(kind_of({}),'color')

        # A toggle drives sections directly, so it needs them and needs no binding.
        missing=copy.deepcopy(base);del missing['controls'][2]['sections']
        with self.assertRaises(Exception):validate(missing)
        # Nothing else may claim them.
        stray=copy.deepcopy(base);stray['controls'][1]['sections']=[1]
        with self.assertRaisesRegex(ValueError,'toggle'):validate(stray)
        # On or off, never half hidden.
        half=copy.deepcopy(base);half['controls'][2]['default']=[.5,0,0,1]
        with self.assertRaisesRegex(ValueError,'on or off'):validate(half)
        # An unknown kind is refused rather than quietly treated as a colour.
        odd=copy.deepcopy(base);odd['controls'][1]['kind']='texture'
        with self.assertRaisesRegex(ValueError,'kind'):validate(odd)
        # kind and type must agree when a recipe carries both.
        both=copy.deepcopy(base);both['controls'][1]['type']='color'
        with self.assertRaisesRegex(ValueError,'contradicts'):validate(both)

        # A choice picks one of the textures the package ships.
        choice=copy.deepcopy(base)
        choice['controls'].append(dict(id='pattern',name='Pattern',kind='choice',role='pattern',
            default=[1,0,0,1],
            options=[dict(name='Plain',texture='/Game/CSS/x/T_Plain.T_Plain'),
                     dict(name='Lace',texture='/Game/CSS/x/T_Lace.T_Lace')],
            bindings=[dict(slot=0,parameter='BaseColorMap  non VT')]))
        validate(choice)
        self.assertEqual(kind_of(choice['controls'][3]),'choice')
        # The default has to name one of them.
        over=copy.deepcopy(choice);over['controls'][3]['default']=[2,0,0,1]
        with self.assertRaisesRegex(ValueError,'name one of its options'):validate(over)
        # A path without its object name is not an asset.
        short=copy.deepcopy(choice);short['controls'][3]['options'][0]['texture']='/Game/CSS/x/T_Plain'
        with self.assertRaisesRegex(ValueError,'object name'):validate(short)
        # One option is not a choice, and it needs somewhere to write the texture.
        lonely=copy.deepcopy(choice);lonely['controls'][3]['options']=lonely['controls'][3]['options'][:1]
        with self.assertRaisesRegex(ValueError,'two and sixteen'):validate(lonely)
        unbound=copy.deepcopy(choice);del unbound['controls'][3]['bindings']
        with self.assertRaisesRegex(ValueError,'texture parameter'):validate(unbound)
        # Nothing else may carry options.
        stray_options=copy.deepcopy(choice);stray_options['controls'][1]['options']=[]
        with self.assertRaisesRegex(ValueError,'choice'):validate(stray_options)

    def test_spring_controls(self):
        """1.0: a spring tunes live secondary motion, in numbers a person can reason about."""
        from css_controls import kind_of,spring_tuning
        base=dict(schema=1,controls=[
            dict(id='cloth',name='Garment',role='garment',group='outfit',default=[1,1,1,1]),
            dict(id='bust',name='Bust',kind='spring',group='body',role='figure',
                 nodes=['brust001','brust002'],
                 frequency=dict(min=1.2,max=2.6,default=1.6),
                 damping_ratio=dict(min=.4,max=.95,default=.65))],
            surfaces=[dict(id='body',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'})],
            palettes=[dict(id='red',name='Crimson',values={'cloth':[.6,.1,.2,1]})])
        validate(base)
        self.assertEqual(kind_of(base['controls'][1]),'spring')

        # The conversion has to agree with spring_tuning() in native/src/colors.cpp, and
        # with what the Seductress V2 blueprint actually ships: 1.5915 Hz at a damping
        # ratio of 0.65 is stiffness 100 and damping 13.
        stiffness,damping=spring_tuning(1.5915494309189535,.65)
        self.assertAlmostEqual(stiffness,100,places=4)
        self.assertAlmostEqual(damping,13,places=4)

        # Bones, both ranges, and nothing that would say the same number twice.
        for key in ('nodes','frequency','damping_ratio'):
            gone=copy.deepcopy(base);del gone['controls'][1][key]
            with self.assertRaises(Exception):validate(gone)
        twice=copy.deepcopy(base);twice['controls'][1]['nodes']=['brust001','brust001']
        with self.assertRaisesRegex(ValueError,'same bone twice'):validate(twice)
        hyphen=copy.deepcopy(base);hyphen['controls'][1]['nodes']=['brust-001']
        with self.assertRaisesRegex(ValueError,'bone name'):validate(hyphen)
        spelled=copy.deepcopy(base);spelled['controls'][1]['default']=[1.6,.65,0,1]
        with self.assertRaisesRegex(ValueError,'takes its default'):validate(spelled)
        limits=copy.deepcopy(base);limits['controls'][1]['max']=4
        with self.assertRaisesRegex(ValueError,'takes its limits'):validate(limits)
        bound=copy.deepcopy(base)
        bound['controls'][1]['bindings']=[dict(slot=0,parameter='Roughness')]
        with self.assertRaisesRegex(ValueError,'no material parameter'):validate(bound)
        # Nothing else tunes a skeleton.
        stray=copy.deepcopy(base);stray['controls'][0]['nodes']=['belly']
        with self.assertRaisesRegex(ValueError,'spring control tunes'):validate(stray)
        # A range has to be a range, and the default has to sit inside it.
        upside_down=copy.deepcopy(base);upside_down['controls'][1]['frequency']['min']=3
        with self.assertRaisesRegex(ValueError,'frequency range'):validate(upside_down)
        outside=copy.deepcopy(base);outside['controls'][1]['frequency']['default']=2.9
        with self.assertRaisesRegex(ValueError,'frequency range'):validate(outside)
        # Past the engine's damping cutoff the slider stops meaning what it says.
        violent=copy.deepcopy(base)
        violent['controls'][1]['frequency']['max']=8
        violent['controls'][1]['damping_ratio']['max']=2
        with self.assertRaisesRegex(ValueError,'integrate as written'):validate(violent)

        # A palette may set a spring, and each channel is checked against its own range.
        look=copy.deepcopy(base)
        look['palettes'][0]['values']['bust']=[2.0,.5,0,1]
        validate(look)
        loud=copy.deepcopy(look);loud['palettes'][0]['values']['bust']=[4.0,.5,0,1]
        with self.assertRaises(ValueError):validate(loud)
        sticky=copy.deepcopy(look);sticky['palettes'][0]['values']['bust']=[2.0,1.4,0,1]
        with self.assertRaises(ValueError):validate(sticky)

    def test_shape_controls(self):
        """1.0: a shape drives a morph target the package cooked into its own mesh."""
        from css_controls import kind_of
        base=dict(schema=1,controls=[
            dict(id='cloth',name='Garment',role='garment',group='outfit',default=[1,1,1,1]),
            dict(id='hips',name='Hips',kind='shape',group='body',role='figure',
                 morph='Hips',min=0,max=1,step=.05,default=[0,0,0,1])],
            surfaces=[dict(id='body',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'})],
            palettes=[dict(id='red',name='Crimson',values={'cloth':[.6,.1,.2,1]})])
        validate(base)
        self.assertEqual(kind_of(base['controls'][1]),'shape')

        gone=copy.deepcopy(base);del gone['controls'][1]['morph']
        with self.assertRaisesRegex(ValueError,'morph target name'):validate(gone)
        # CSSImportMesh refuses any name the engine would have renamed, so this does too.
        for bad in ('','Hips and thighs','Hips-2','x'*65):
            odd=copy.deepcopy(base);odd['controls'][1]['morph']=bad
            with self.assertRaisesRegex(ValueError,'morph target name'):validate(odd)
        bound=copy.deepcopy(base)
        bound['controls'][1]['bindings']=[dict(slot=0,parameter='Roughness')]
        with self.assertRaisesRegex(ValueError,'no material parameter'):validate(bound)
        # Nothing else drives a morph.
        stray=copy.deepcopy(base);stray['controls'][0]['morph']='Hips'
        with self.assertRaisesRegex(ValueError,'shape control drives'):validate(stray)
        # The weight is an ordinary slider and keeps its author's range.
        look=copy.deepcopy(base);look['palettes'][0]['values']['hips']=[.5,0,0,1]
        validate(look)
        past=copy.deepcopy(look);past['palettes'][0]['values']['hips']=[1.5,0,0,1]
        with self.assertRaises(ValueError):validate(past)

        # A shape naming a morph the mesh does not carry takes the outfit off at runtime,
        # so it is caught against the mesh JSON at build time instead.
        from css_controls import check_shapes,shape_names
        mesh=dict(morph_targets=[dict(name='Hips',deltas=[[0,1,0,0]]),dict(name='Waist',deltas=[[0,0,1,0]])])
        self.assertEqual(shape_names(mesh),{'Hips','Waist'})
        check_shapes(base,shape_names(mesh))
        typo=copy.deepcopy(base);typo['controls'][1]['morph']='Hip'
        with self.assertRaisesRegex(ValueError,'does not have'):check_shapes(typo,shape_names(mesh))
        with self.assertRaisesRegex(ValueError,'does not have'):check_shapes(base,set())

    def test_reviewed_recipes(self):
        root=Path(__file__).resolve().parents[1]/'work/color-recipes'
        recipes=list(root.glob('*/*.colors.json'))
        if not recipes:self.skipTest('Private generated recipes are not distributed')
        self.assertEqual(len(recipes),3)
        for path in recipes:
            recipe=json.loads(path.read_text());files=validate(recipe['colors'])
            self.assertTrue(all((path.parent/name).is_file() for name in files))
            self.assertEqual({p['id'] for p in recipe['colors']['palettes']},{'crimson','midnight'})
            for palette in recipe['colors']['palettes']:
                self.assertTrue(set(palette['values']).isdisjoint({'skin','face','eye-glow','eye-intensity'}))

if __name__=='__main__':unittest.main()
