"""Prevent linked RigVM inputs from being reported as fixed bone targets."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('hand_targets',
    Path(__file__).resolve().parents[1] / 'tools/diagnostics/inspect_hand_targets.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class HandTargetsTests(unittest.TestCase):
    def setUp(self):
        self.asset = '/Game/Test/Hand'
        self.node = self.asset + '.Hand:RigVMModel.Write'
        self.objects = {self.node: {'class': '/Script/RigVMDeveloper.RigVMUnitNode',
            'properties': {'ResolvedFunctionName': ['FRigUnit_SetTransform::Execute']}}}
        for field, value in {'Item.Type': 'Bone', 'Item.Name': 'finger', 'Space': 'LocalSpace',
                             'bInitial': 'False', 'bPropagateToChildren': 'True', 'Weight': '1'}.items():
            self.objects[self.node + '.' + field] = {'class': '/Script/RigVMDeveloper.RigVMPin',
                                                     'properties': {'DefaultValue': [value]}}
        self.skeleton = {'ReferenceSkeleton': {'FinalRefBoneInfo': [
            {'Name': 'root', 'ParentIndex': -1}, {'Name': 'finger', 'ParentIndex': 0},
            {'Name': 'tip', 'ParentIndex': 1}, {'Name': 'other_hand', 'ParentIndex': 0}]}}

    def run_audit(self):
        return audit.inspect({self.asset: {'properties': self.objects}}, self.asset, self.skeleton)

    def test_descendants_exclude_siblings_and_ancestors(self):
        self.assertEqual(self.run_audit()['potentially_affected_bones'], ['finger', 'tip'])

    def test_linked_parent_or_target_is_unresolved(self):
        for target in ('Write.Item', 'Write.Item.Name', 'Write.bPropagateToChildren', 'Write.Weight'):
            with self.subTest(target=target):
                self.objects['Link'] = {'class': '/Script/RigVMDeveloper.RigVMLink',
                    'properties': {'SourcePinPath': ['Dynamic.Value'], 'TargetPinPath': [target]}}
                result = self.run_audit()
                self.assertEqual(result['literal_target_counts'], {})
                self.assertEqual(result['unresolved_writers'], ['RigVMModel.Write'])

    def test_unrelated_input_does_not_hide_literal_target(self):
        self.objects['Link'] = {'class': '/Script/RigVMDeveloper.RigVMLink',
            'properties': {'SourcePinPath': ['Dynamic.Value'], 'TargetPinPath': ['Write.Value']}}
        self.assertEqual(self.run_audit()['literal_target_counts'], {'finger': 1})

    def test_missing_bone_rejected(self):
        self.objects[self.node + '.Item.Name']['properties']['DefaultValue'] = ['absent']
        with self.assertRaisesRegex(ValueError, 'absent from skeleton'):
            self.run_audit()

    def test_invalid_parent_rejected(self):
        self.skeleton['ReferenceSkeleton']['FinalRefBoneInfo'][1]['ParentIndex'] = 1
        with self.assertRaisesRegex(ValueError, 'parent index'):
            self.run_audit()

    def test_nonpropagating_target(self):
        self.objects[self.node + '.bPropagateToChildren']['properties']['DefaultValue'] = ['False']
        self.assertEqual(self.run_audit()['potentially_affected_bones'], ['finger'])


if __name__ == '__main__':
    unittest.main()
