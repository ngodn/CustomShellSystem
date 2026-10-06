"""Keep existing animation choices and face bindings intact during a facial update."""
import copy
import unittest
from package_eve_facial_update import refresh_catalog


class Refresh(unittest.TestCase):
    def setUp(self):
        self.old = dict(id='css.eve.face',morphs=['jaw'],expressions=[dict(id='neutral',weights={'jaw':0})],
            performances=[dict(clip='old',duration=1,curves=[dict(morph='jaw',keys=[[0,0],[1,1]])])],
            adapters=[dict(source_mesh=f'/Game/Eve/SK_{i}.SK_{i}',target_mesh=f'/Game/Face/Old{i}.Old{i}',
                skeleton='eve',source_outfit='eve',source_toc_sha256='a'*64,source_bulk_bytes=12)
                for i in range(6)])
        ids=['old','css.eve.dance.dumbdumb','css.eve.dance.irisout','css.eve.dance.jangwonyoungspin']
        self.document=dict(catalog=dict(animation_library=dict(entries=[dict(id=i) for i in ids],
            adapters=[dict(id='genessa'),dict(id='commanderwhite')],
            faces=[copy.deepcopy(self.old),dict(id='css.commanderwhite.face',morphs=['cw'])])),version='untouched')
        self.new=copy.deepcopy(self.old)
        self.new['morphs'].append('mouth')
        self.new['performances'] += [dict(clip=i,duration=2,curves=[]) for i in ids[1:]]
        for i,a in enumerate(self.new['adapters']):a['target_mesh']=f'/Game/Face/New{i}.New{i}'

    def test_only_eve_rig_changes_and_input_is_unchanged(self):
        saved=copy.deepcopy(self.document)
        result,old=refresh_catalog(self.document,self.new)
        self.assertEqual(self.document,saved)
        self.assertEqual(old,self.old)
        self.assertEqual(result['catalog']['animation_library']['faces'][1],
                         saved['catalog']['animation_library']['faces'][1])
        result['catalog']['animation_library']['faces'][0]=self.old
        self.assertEqual(result,saved)

    def test_saved_expressions_cannot_change(self):
        self.new['expressions'][0]['weights']['jaw']=.5
        with self.assertRaisesRegex(ValueError,'expressions changed'):refresh_catalog(self.document,self.new)

    def test_existing_curve_key_cannot_change(self):
        self.new['performances'][0]['curves'][0]['keys'][1][1]=.8
        with self.assertRaisesRegex(ValueError,'performances changed'):refresh_catalog(self.document,self.new)

    def test_unknown_body_clip_is_rejected(self):
        self.document['catalog']['animation_library']['entries'].pop()
        with self.assertRaisesRegex(ValueError,'no body clip'):refresh_catalog(self.document,self.new)

    def test_binding_revision_cannot_change(self):
        self.new['adapters'][0]['source_toc_sha256']='b'*64
        with self.assertRaisesRegex(ValueError,'binding changed'):refresh_catalog(self.document,self.new)

    def test_distinct_variants_cannot_share_replacement_mesh(self):
        self.new['adapters'][1]['target_mesh']=self.new['adapters'][0]['target_mesh']
        with self.assertRaisesRegex(ValueError,'Repeated replacement'):refresh_catalog(self.document,self.new)

    def test_existing_morph_cannot_disappear(self):
        self.new['morphs'].remove('jaw')
        with self.assertRaisesRegex(ValueError,'shapes or expressions changed'):refresh_catalog(self.document,self.new)


if __name__=='__main__':
    unittest.main()
