import sys
import tempfile
import unittest
import json
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import css_convert as css


class NamingTests(unittest.TestCase):
    def test_default(self):
        self.assertEqual(css.output_name('HIT2 DE Scyther','XTGMods'),'CSS_HIT2_DE_Scyther_XTGMods_P')

    def test_templates(self):
        for pattern in ('CSS_$NAME_$AUTHORorMODDER','CSS_${NAME}_${AUTHOR}','CSS_${NAME}_${MODDER}'):
            self.assertEqual(css.output_name('A B','Modder',pattern),'CSS_A_B_Modder_P')
        self.assertEqual(css.output_name('A','B','${AUTHOR}_${NAME}_P'),'B_A_P')
        self.assertEqual(css.output_name('A','B','${AUTHOR}_${NAME}'),'B_A_P')

    def test_unsafe_or_unknown_names(self):
        for pattern in ('../$NAME','$NOPE','$NAME.pak','CON','C:/$NAME',''):
            with self.subTest(pattern=pattern),self.assertRaises(ValueError): css.output_name('A','B',pattern)
        with self.assertRaises(ValueError): css.output_name('A',' ')


class DiscoveryTests(unittest.TestCase):
    def test_any_companion_and_directory(self):
        with tempfile.TemporaryDirectory() as root:
            directory=Path(root)/'nested/packs'; directory.mkdir(parents=True)
            paths=[directory/('Some Mod_P'+suffix) for suffix in ('.pak','.utoc','.ucas')]
            for path in paths: path.touch()
            for source in (*paths,Path(root)):
                self.assertEqual(css.discover([source]),[paths[1]])
            self.assertEqual(css.discover(paths),[paths[1]])

    def test_missing_companion(self):
        with tempfile.TemporaryDirectory() as root:
            file=Path(root)/'mod.utoc'; file.touch()
            with self.assertRaisesRegex(ValueError,'Missing companion'): css.discover([file])

    def test_standalone_pak(self):
        with tempfile.TemporaryDirectory() as root:
            file=Path(root)/'legacy.pak'; file.touch()
            self.assertEqual(css.discover([file]),[file])

    def test_empty_directory(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaisesRegex(ValueError,'No pack'): css.discover([Path(root)])


class RelocationTests(unittest.TestCase):
    old='/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/Body'

    def test_isolation_and_length(self):
        a=css.relocation('author.one',[self.old]); b=css.relocation('author.two',[self.old])
        self.assertNotEqual(a,b)
        self.assertEqual(len(a[self.old]),len(self.old))
        self.assertTrue(a[self.old].startswith('/Game/CSS/'))
        self.assertEqual(a,css.relocation('author.one',[self.old]))

    def test_object_and_unicode_references(self):
        mapping=css.relocation('author.one',[self.old])
        data=(self.old+'.Body:Subobject\0').encode()+(self.old+'\0').encode('utf-16-le')
        changed,count=css.replace_references(data,mapping)
        self.assertEqual(count,2)
        self.assertEqual(len(changed),len(data))
        restored,_=css.replace_references(changed,{v:k for k,v in mapping.items()})
        self.assertEqual(restored,data)

    def test_numbered_package_names_and_case_preserving_inverse(self):
        old=self.old+'_1001';mapping=css.relocation('test.numbered',[old,self.old+'_1002'])
        references=css.reference_mapping(mapping)
        self.assertEqual(references[self.old]+'_1001',mapping[old])
        data=(old+'\0'+self.old.lower()+'\0').encode()
        edits=[];changed,count=css.replace_references(data,references,edits)
        self.assertEqual(count,2)
        restored=bytearray(changed)
        for offset,before in reversed(edits):restored[offset:offset+len(before)]=before
        self.assertEqual(restored,data)
        self.assertEqual(len(changed),len(data))

    def test_unrelated_assets_are_preserved(self):
        data=(self.old+'_Unrelated\0/Game/Engine/Unchanged\0').encode()
        self.assertEqual(css.replace_references(data,css.relocation('a.b',[self.old])),(data,0))

    def test_short_paths_rejected(self):
        with self.assertRaises(ValueError): css.relocation('a.b',['/Game/Mesh'])

    def test_live_hit2_export_classes_and_roundtrip(self):
        root=Path(__file__).resolve().parents[1]/'work/hit2-inspect/legacy'
        if not root.exists(): self.skipTest('Local HIT2 fixture is not distributed')
        files=list(root.rglob('*.uasset'))
        self.assertEqual(len(files),13)
        info=[css.asset_info(p.read_bytes()) for p in files]
        mapping=css.relocation('xtgmods.hit2_de_scyther',[i['package'] for i in info])
        meshes=[e for i in info for e in i['exports'] if e['class']=='SkeletalMesh' and e['asset']]
        self.assertEqual([e['name'] for e in meshes],['SK_Shell_KnightLady_V04'])
        for file,before in zip(files,info):
            changed,_=css.replace_references(file.read_bytes(),mapping)
            after=css.asset_info(changed)
            self.assertEqual(before['exports'],after['exports'])
            self.assertEqual(after['package'],mapping[before['package']])


class VariantTests(unittest.TestCase):
    package='/Game/Sparta/Characters/Shells/Example/Art/Mesh/Body'
    def fixtures(self):
        return [{'package':self.package+suffix,'exports':[{'name':'Body'+suffix,'class':'SkeletalMesh','asset':True,'outer':0}]}
                for suffix in ('','_Corrupted')]

    def test_grouped_variants_keep_ids_and_export_names(self):
        values=css.select_variants(self.fixtures(),None,['regular='+self.package,'corrupted='+self.package+'_Corrupted.Body_Corrupted'],'Example')
        self.assertEqual([v['id'] for v in values],['regular','corrupted'])
        self.assertEqual(values[1]['mesh'],self.package+'_Corrupted.Body_Corrupted')

    def test_ambiguous_and_duplicate_variants_rejected(self):
        for mesh,definitions in [(None,[]),(self.package,['a='+self.package]),(None,['a='+self.package]*2),(None,['a=/Game/Missing'])]:
            with self.subTest(mesh=mesh,definitions=definitions),self.assertRaises(ValueError):
                css.select_variants(self.fixtures(),mesh,definitions,'Example')

    def test_material_recipe_must_resolve_included_materials(self):
        info=[{'package':self.package,'exports':[{'name':'MI_Body','class':'MaterialInstanceConstant','outer':0}]}]
        mapping=css.relocation('test.example',[self.package])
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'materials.json'
            path.write_text(json.dumps({'0':self.package+'.MI_Body'}))
            self.assertEqual(css.material_recipe(path,info,mapping),{'0':mapping[self.package]+'.MI_Body'})
            for bad in [{'128':self.package+'.MI_Body'},{'01':self.package+'.MI_Body'},{'0':'/Game/Absent.Material'},[]]:
                path.write_text(json.dumps(bad))
                with self.subTest(bad=bad),self.assertRaises(ValueError):css.material_recipe(path,info,mapping)

    def test_overlapping_source_groups_are_kept_separate(self):
        with tempfile.TemporaryDirectory() as root:
            root=Path(root)
            for name in ('heels','flat'):(root/name).mkdir()
            path=root/'variants.json'
            recipe=[{'id':n,'name':n.title(),'inputs':[n]} for n in ('heels','flat')]
            path.write_text(json.dumps(recipe))
            groups=css.variant_sources(path)
            self.assertEqual([g['inputs'][0] for g in groups],[root/'heels',root/'flat'])
            recipe[1]['id']='heels';path.write_text(json.dumps(recipe))
            with self.assertRaisesRegex(ValueError,'unique'):css.variant_sources(path)


class TextureSharingTests(unittest.TestCase):
    def test_only_identical_root_textures_are_shared(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            paths=['/Game/CSS/aaaaaaaa/texture','/Game/CSS/bbbbbbbb/texture',
                   '/Game/CSS/cccccccc/texture','/Game/CSS/dddddddd/meshref']
            reports=[]
            for index,path in enumerate(paths):
                base=root/'MortalShell2/Content'/path.removeprefix('/Game/')
                base.parent.mkdir(parents=True,exist_ok=True)
                header=(paths[1]+'\0').encode() if index==3 else b'header'
                files={}
                for suffix,data in (('.uasset',header),('.ubulk',b'payload')):
                    file=Path(str(base)+suffix);file.write_bytes(data)
                    files[suffix]={'source_sha256':str(0 if index<2 else index)+suffix}
                reports.append({'assets':[{'css':path,'files':files,'exports':[
                    {'class':'SkeletalMesh' if index==3 else 'Texture2D','outer':0}]}]})
            assets,shared=css.share_variant_textures(root,reports)
            self.assertEqual(shared['aliases'],{paths[1]:paths[0]})
            self.assertEqual(len(assets),3)
            self.assertGreater(shared['legacy_bytes_saved'],0)
            deleted=root/'MortalShell2/Content'/paths[1].removeprefix('/Game/')
            self.assertFalse(Path(str(deleted)+'.uasset').exists())
            mesh=root/'MortalShell2/Content'/paths[3].removeprefix('/Game/')
            self.assertEqual(Path(str(mesh)+'.uasset').read_bytes(),(paths[0]+'\0').encode())
            self.assertEqual(Path(str(mesh)+'.ubulk').read_bytes(),b'payload')


if __name__=='__main__': unittest.main()
