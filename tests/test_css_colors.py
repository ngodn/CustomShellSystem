import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from css_colors import validate,embed,verify_resources

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
