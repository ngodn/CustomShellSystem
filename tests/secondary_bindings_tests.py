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
                          bone_transforms=[], rigs=[dict(node='Body', rig='/Game/Body', pose_flags={},
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


if __name__ == '__main__':
    unittest.main()
