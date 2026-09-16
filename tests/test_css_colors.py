import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from css_colors import validate,embed,verify_resources,lint_convention

class ColorTests(unittest.TestCase):
    def setUp(self):
        self.colors=dict(schema=1,controls=[dict(id='cloth',name='Clothing',default=[1,1,1,1])],surfaces=[dict(id='body',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'})],palettes=[dict(id='red',name='Crimson',values={'cloth':[.6,.1,.2,1]})])

    def test_mask_embedding_and_corruption(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);metadata=root/'metadata';metadata.mkdir()
            Image.new('RGBA',(1024,1024),(255,255,255,0)).save(root/'dye-cloth.png')
            recipe=root/'outfit.colors.json';recipe.write_text(json.dumps(dict(id='test',colors=self.colors)))
            manifest=dict(id='test',catalog=dict(outfits=[dict(id='test')]))
            embed(recipe,manifest,metadata);verify_resources(manifest,metadata)
            self.assertEqual(manifest['catalog']['outfits'][0]['colors'],self.colors)
            image=metadata/'dye-cloth.png';data=bytearray(image.read_bytes());data[-1]^=1;image.write_bytes(data)
            with self.assertRaisesRegex(ValueError,'checksum'):verify_resources(manifest,metadata)

    def test_invalid_bindings_and_parts(self):
        variants=[]
        bad=copy.deepcopy(self.colors);bad['surfaces'][0]['layers']['cloth']='../dye-cloth.png';variants.append(bad)
        bad=copy.deepcopy(self.colors);bad['surfaces'][0]['slots']=[128];variants.append(bad)
        bad=copy.deepcopy(self.colors);bad['controls'][0]['default'][3]=2;variants.append(bad)
        bad=copy.deepcopy(self.colors);bad['palettes'][0]['values']['cloth'][3]=.5;variants.append(bad)
        bad=copy.deepcopy(self.colors);bad['surfaces'][0]['layers']['unknown']='dye-other.png';variants.append(bad)
        bad=copy.deepcopy(self.colors);bad['controls'][0]['default'][0]=float('nan');variants.append(bad)
        bad=copy.deepcopy(self.colors);bad['surfaces'].append(dict(id='duplicate',parameter='BaseColorMap  non VT',slots=[0],layers={'cloth':'dye-cloth.png'}));variants.append(bad)
        for bad in variants:
            with self.subTest(colors=bad),self.assertRaises(ValueError):validate(bad)

    def test_variant_resources_share_only_identical_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);metadata=root/'metadata';metadata.mkdir()
            Image.new('RGBA',(1024,1024),(255,255,255,0)).save(root/'dye-cloth.png')
            recipe=root/'colors.json';recipe.write_text(json.dumps(dict(id='test',colors=self.colors)))
            variants=[{'id':'one'},{'id':'two'}]
            manifest=dict(id='test',catalog=dict(outfits=[dict(id='test',variants=variants)]))
            for variant in variants:embed(recipe,manifest,metadata,variant)
            verify_resources(manifest,metadata)
            self.assertEqual(len(manifest['resources']),1)
            self.assertNotIn('colors',manifest['catalog']['outfits'][0])
            Image.new('RGBA',(1024,1024),(0,0,0,255)).save(root/'dye-cloth.png')
            with self.assertRaisesRegex(ValueError,'collision'):embed(recipe,manifest,metadata,variants[1])

    def test_convention_fields_are_optional_but_checked(self):
        control=self.colors['controls'][0]
        for good in [dict(group='outfit'),dict(group='body'),dict(role='garment'),dict(hue_locked=True),
                     dict(kind='color'),dict(group='outfit',role='metal',hue_locked=True)]:
            colors=copy.deepcopy(self.colors);colors['controls'][0].update(good)
            with self.subTest(control=good):validate(colors)
        for bad in [dict(group='ornaments'),dict(group=''),dict(role='a b'),dict(role='x'*33),
                    dict(hue_locked='yes'),dict(kind='scalar'),dict(kind='intensity',type='color')]:
            colors=copy.deepcopy(self.colors);colors['controls'][0].update(bad)
            with self.subTest(control=bad),self.assertRaises(ValueError):validate(colors)
        self.assertEqual(control,self.colors['controls'][0])

    def test_intensity_is_a_scalar_everywhere_type_would_be(self):
        # `kind: intensity` is the convention's spelling of `type: scalar`, so a
        # control declaring it must be refused as a dye layer and take one value.
        colors=copy.deepcopy(self.colors)
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
        self.assertTrue(lint_convention(self.colors))       # no role, no group, one palette
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
