"""Reject cooked adapter drift while allowing regenerated UE node GUIDs."""
import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('audit', Path(__file__).resolve().parents[1] /
                                            'tools/diagnostics/audit_secondary_bindings.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class Bindings(unittest.TestCase):
    def setUp(self):
        scalar = dict(Type='FloatProperty', ElementSize=4)
        self.graph = dict(controls={'CSSFrequency': scalar}, control_defaults={'CSSFrequency': 2.0},
                          bone_transforms=[], writer_orders=[['rig:Body', 'rig:Hair']],
                          rigs=[dict(node='Body', rig='/Game/Body', pose_flags={},
                          bindings={'Frequency': dict(source='CSSFrequency', source_type=scalar,
                                                      target_type=scalar)})])

    def test_same_type_wrong_input(self):
        other = copy.deepcopy(self.graph)
        other['rigs'][0]['bindings']['Frequency']['source'] = 'CSSDamping'
        self.assertTrue(audit.compare_adapter(self.graph, other))

    def test_regenerated_guid(self):
        other = copy.deepcopy(self.graph)
        self.graph['rigs'][0]['bindings']['Frequency']['source'] = '__CustomProperty_Frequency_' + 'A' * 32
        other['rigs'][0]['bindings']['Frequency']['source'] = '__CustomProperty_Frequency_' + 'B' * 32
        self.assertFalse(audit.compare_adapter(self.graph, other))

    def test_generated_wrong_pin(self):
        other = copy.deepcopy(self.graph)
        self.graph['rigs'][0]['bindings']['Frequency']['source'] = '__CustomProperty_Frequency_' + 'A' * 32
        other['rigs'][0]['bindings']['Frequency']['source'] = '__CustomProperty_Damping_' + 'B' * 32
        self.assertTrue(audit.compare_adapter(self.graph, other))

    def test_changed_default(self):
        other = copy.deepcopy(self.graph)
        other['control_defaults']['CSSFrequency'] = 20.0
        self.assertTrue(audit.compare_adapter(self.graph, other))

    def test_pose_transfer(self):
        other = copy.deepcopy(self.graph)
        other['rigs'][0]['pose_flags']['bTransferInputPose'] = False
        self.assertTrue(audit.compare_adapter(self.graph, other))

    def test_writer_order(self):
        other = copy.deepcopy(self.graph)
        other['writer_orders'][0].reverse()
        self.assertTrue(audit.compare_adapter(self.graph, other))

    def test_branch_bypasses_physics(self):
        other = copy.deepcopy(self.graph)
        other['writer_orders'].append([])
        self.assertTrue(audit.compare_adapter(self.graph, other))

    def test_array_inner_type(self):
        field = dict(Type='ArrayProperty', Inner=dict(Name='Generated_A', Type='FloatProperty', ElementSize=4))
        other = copy.deepcopy(field)
        other['Inner']['Name'] = 'Generated_B'
        self.assertEqual(audit.field_type(field), audit.field_type(other))
        other['Inner']['Type'] = 'IntProperty'
        self.assertNotEqual(audit.field_type(field), audit.field_type(other))

    def test_single_chain_repeated_endpoint(self):
        rigs = {'/Game/Body': {'defaults': {'BoneNames': ['tip', 'tip']}}}
        self.assertFalse(audit.named_bone_overlaps({'graph': self.graph}, rigs))

    def test_two_writers(self):
        self.graph['rigs'].append(dict(node='Other', rig='/Game/Hair'))
        rigs = {name: {'defaults': {'BoneNames': ['tip']}} for name in ('/Game/Body', '/Game/Hair')}
        self.assertEqual(audit.named_bone_overlaps({'graph': self.graph}, rigs),
                         [dict(graph='graph', bone='tip', nodes=['Body', 'Other'])])


    def pose_exports(self):
        names = [('AnimGraphNode_Root', 'AnimNode_Root'),
                 ('AnimGraphNode_Input', 'AnimNode_LinkedInputPose'),
                 ('AnimGraphNode_Body', 'AnimNode_ControlRig'),
                 ('AnimGraphNode_Hair', 'AnimNode_ControlRig')]
        return [dict(Type='AnimBlueprintGeneratedClass', Name='ABP_C',
                     SuperStruct=dict(ObjectName="Class'AnimInstance'"),
                     ChildProperties=[dict(Name=n, Type='StructProperty',
                                           Struct=dict(ObjectName="Class'" + t + "'")) for n, t in names]),
                dict(Type='ABP_C', Name='Default__ABP_C', Properties={
                    'AnimGraphNode_Root': {'Result': {'LinkID': 3}},
                    'AnimGraphNode_Input': {'InputPose': {'LinkID': -1}},
                    'AnimGraphNode_Body': {'Source': {'LinkID': 1}, 'ControlRigClass': '/Game/Body.Body_C'},
                    'AnimGraphNode_Hair': {'Source': {'LinkID': 2}, 'ControlRigClass': '/Game/Hair.Hair_C'}})]

    def test_walk_cooked_links(self):
        self.assertEqual(audit.pose_writer_orders(self.pose_exports()), [['rig:/Game/Body', 'rig:/Game/Hair']])

    def test_cyclic_links(self):
        exports = self.pose_exports()
        exports[1]['Properties']['AnimGraphNode_Body']['Source']['LinkID'] = 3
        with self.assertRaisesRegex(ValueError, 'Cycle'):
            audit.pose_writer_orders(exports)

    def test_disconnected_writer(self):
        exports = self.pose_exports()
        exports[1]['Properties']['AnimGraphNode_Hair']['Source']['LinkID'] = 1
        with self.assertRaisesRegex(ValueError, 'disconnected'):
            audit.pose_writer_orders(exports)

    def test_bad_link_index(self):
        exports = self.pose_exports()
        exports[1]['Properties']['AnimGraphNode_Root']['Result']['LinkID'] = 9
        with self.assertRaisesRegex(ValueError, 'outside'):
            audit.pose_writer_orders(exports)


if __name__ == '__main__':
    unittest.main()
