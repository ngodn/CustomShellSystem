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
