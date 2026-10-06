import bisect
import copy
import unittest

from prepare_blender_face import linearize, prepare


def fixture():
    strip = dict(type='CLIP',mute=False,blend_type='REPLACE',influence=1,scale=1,repeat=1,
                 extrapolation='HOLD',animated_time=False,animated_influence=False,
                 scene_range=[1,10],action_range=[1,10],action='jaw',action_slot='Face')
    inspection = dict(sha256='source',source='face.blend',autoexec_enabled=False,
        meshes=[dict(name='Face',shape_values={'jaw_open':0},shape_animation=dict(
            action=None,drivers=[],use_nla=True,nla=[dict(mute=False,solo=False,strips=[strip])]))],
        scenes=[dict(fps=25,fps_base=1)])
    curve = dict(path='key_blocks["jaw_open"].value',action='jaw',slot='Face',index=0,
                 mute=False,modifiers=[],extrapolation='CONSTANT',keys=[
                    dict(co=[1,0],right=[4,0],interpolation='BEZIER'),
                    dict(co=[10,1],left=[7,1],interpolation='BEZIER')])
    audit = dict(sha256='source',source='face.blend',autoexec_enabled=False,mesh='Face',
        scene_frames=[1,10],scene_rate=25,duration=.36,active_curves=[curve],names=['jaw_open'],
        samples=[dict(seconds=t*.36,values=[3*t*t-2*t*t*t]) for t in (0,.25,.5,.75,1)])
    return inspection,audit


class BlenderFacePreparationTests(unittest.TestCase):
    def test_error_bound_holds_between_keys_and_preserves_signed_weights(self):
        keys,bound = linearize([[1,-.25],[4,-.25],[7,1.25],[10,1.25]],.36)
        self.assertLessEqual(bound,1e-5)
        self.assertEqual(keys[0],[0,-.25])
        self.assertEqual(keys[-1],[.36,1.25])
        for step in range(10001):
            t=step/10000
            seconds=t*.36
            i=max(0,min(len(keys)-2,bisect.bisect_right(keys,seconds,key=lambda key:key[0])-1))
            left,right=keys[i:i+2]
            actual=left[1]+(right[1]-left[1])*(seconds-left[0])/(right[0]-left[0])
            self.assertLessEqual(abs(actual-(-.25+1.5*(3*t*t-2*t*t*t))),bound+1e-12)

    def test_reject_nonuniform_handles_and_invalid_bounds(self):
        for points,duration,tolerance in [([[0,0],[.2,0],[.7,1],[1,1]],1,1e-5),
                ([[0,0],[1,0],[2,1],[3,1]],0,1e-5),
                ([[0,0],[1,0],[2,5],[3,1]],1,1e-5),
                ([[0,0],[1,0],[2,1],[3,1]],1,float('nan'))]:
            with self.subTest(points=points),self.assertRaises(ValueError):
                linearize(points,duration,tolerance)

    def test_prepare_keeps_timing_and_inputs(self):
        inspection,audit=fixture()
        before=copy.deepcopy((inspection,audit))
        result=prepare(inspection,audit,['jaw_open'],'css.eve.performance.example200')
        self.assertEqual(result['performance']['duration'],.36)
        self.assertLess(result['scene_comparison_error'],1e-12)
        self.assertEqual((inspection,audit),before)

    def test_reject_unproven_binding_shape_and_samples(self):
        for edit in [lambda i,a:a.update(sha256='different'),
                lambda i,a:a['active_curves'][0].update(modifiers=['NOISE']),
                lambda i,a:i['meshes'][0]['shape_animation']['nla'][0]['strips'][0].update(scale=2),
                lambda i,a:a.update(samples=[]),
                lambda i,a:a['samples'][1].update(values=[float('nan')]),
                lambda i,a:a['samples'][1].update(values=[.7])]:
            inspection,audit=fixture()
            edit(inspection,audit)
            with self.assertRaises(ValueError):
                prepare(inspection,audit,['jaw_open'],'css.eve.performance.example200')
        with self.assertRaises(ValueError):
            prepare(*fixture(),[],'css.eve.performance.example200')


if __name__ == '__main__':
    unittest.main()
