"""Reject incomplete or stale evidence before adding an outfit adapter to a pack."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from adapter_additions import digest, load_addition


class AdapterAdditionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.package = '/Game/CSS/Locomotion/Adapters/ABP_Test'
        self.graph = self.file('editor/Content/CSS/Locomotion/Adapters/ABP_Test.uasset', b'graph')
        self.meshes = ['/Game/CSS/Test/SK_A.SK_A', '/Game/CSS/Test/SK_B.SK_B']
        self.mesh_files = [self.file(f'editor/Content/CSS/Test/SK_{name}.uasset', name.encode()) for name in ('A', 'B')]
        self.clip = self.file('editor/Content/CSS/Locomotion/AN_Test.uasset', b'clip')
        self.payloads = [self.file('cook/CSSAuthoring/Content/CSS/Locomotion/Adapters/ABP_Test' + suffix,
                                  suffix.encode()) for suffix in ('.uasset', '.uexp')]
        self.receipt = dict(protected={str(self.graph): digest(self.graph)},
                            cooked={'MortalShell2/Content/CSS/Locomotion/Adapters/' + p.name: digest(p)
                                    for p in self.payloads})
        self.receipt_path = self.write('receipt.json', self.receipt)
        self.binding = dict(passed=True, contract='explicit-weight-v1',
            evidence={str(self.receipt_path): digest(self.receipt_path)},
            adapters=[dict(target_class=self.package + '.ABP_Test_C')])
        cases = [dict(case=name, body_passed=True, fingers_passed=True, frames=120)
                 for name in ('inactive', 'active_start', 'active_seek', 'held_end', 'stopped',
                              'null_clip_guard', 'restored', 'zero_weight', 'full_clip')]
        self.evaluation = dict(complete=True,
            protected={str(p): digest(p) for p in [self.graph, self.clip, *self.mesh_files]},
            evaluations=[dict(layer=self.package + '.ABP_Test', mesh=mesh, id=clip,
                              finger_bones_checked=30, cases=copy.deepcopy(cases))
                         for mesh in self.meshes for clip in ('clip-a', 'clip-b')])
        self.spec = dict(bindings=str(self.write('bindings.json', self.binding)),
                         evaluation=str(self.write('evaluation.json', self.evaluation)),
                         package_receipt=str(self.receipt_path),
                         cook_exit=str(self.write('exit.json', dict(exit_code=0))),
                         cook_root=str(self.root / 'cook'), meshes=self.meshes)
        self.spec_path = self.write('addition.json', self.spec)

    def file(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def write(self, name, value):
        return self.file(name, json.dumps(value).encode())

    def load(self):
        return load_addition(self.spec_path, {'clip-a', 'clip-b'})

    def test_complete_evidence_returns_both_payloads_and_all_protected_inputs(self):
        adapters, packages, evidence = self.load()
        self.assertEqual(len(adapters), 1)
        self.assertEqual(set(packages[self.package]), {'.uasset', '.uexp'})
        for path in [self.graph, self.clip, *self.mesh_files, *self.payloads]:
            self.assertEqual(evidence[str(path)], digest(path))

    def test_rejects_changed_graph_mesh_clip_or_cooked_export(self):
        for path in [self.graph, self.clip, *self.mesh_files, *self.payloads]:
            before = path.read_bytes()
            path.write_bytes(b'changed')
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.load()
            path.write_bytes(before)

    def test_requires_each_mesh_and_each_moving_clip(self):
        for remove in (lambda row: row['mesh'] == self.meshes[1], lambda row: row['id'] == 'clip-b'):
            broken = copy.deepcopy(self.evaluation)
            broken['evaluations'] = [row for row in broken['evaluations'] if not remove(row)]
            self.write('evaluation.json', broken)
            with self.assertRaisesRegex(ValueError, 'missing a mesh or moving clip'):
                self.load()

    def test_rejects_missing_restore_case_failed_fingers_and_short_full_clip(self):
        for mutation in ('missing', 'fingers', 'short', 'duplicate'):
            broken = copy.deepcopy(self.evaluation)
            cases = broken['evaluations'][0]['cases']
            if mutation == 'missing':
                cases[:] = [case for case in cases if case['case'] != 'restored']
            elif mutation == 'fingers':
                cases[0]['fingers_passed'] = False
            elif mutation == 'short':
                cases[-1]['frames'] = 1
            else:
                cases.append(copy.deepcopy(cases[0]))
            self.write('evaluation.json', broken)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.load()

    def test_bind_mount_alias_requires_matching_package_path_and_bytes(self):
        alias = self.root / 'mounted/Content/CSS/Locomotion/Adapters/ABP_Test.uasset'
        evaluation = copy.deepcopy(self.evaluation)
        evaluation['protected'][str(alias)] = evaluation['protected'].pop(str(self.graph))
        self.write('evaluation.json', evaluation)
        self.load()
        evaluation['protected'][str(alias)] = '0' * 64
        self.write('evaluation.json', evaluation)
        with self.assertRaises(ValueError):
            self.load()

    def test_rejects_unsuccessful_cook_or_wrong_receipt_hash(self):
        self.write('exit.json', dict(exit_code=1))
        with self.assertRaises(ValueError):
            self.load()
        self.write('exit.json', dict(exit_code=0))
        self.binding['evidence'][str(self.receipt_path)] = '0' * 64
        self.write('bindings.json', self.binding)
        with self.assertRaises(ValueError):
            self.load()

    def test_rejects_unverified_targets_and_duplicate_mesh_clip_rows(self):
        for mutation in ('object', 'package', 'duplicate'):
            evaluation = copy.deepcopy(self.evaluation)
            if mutation == 'object':
                evaluation['evaluations'][0]['layer'] = self.package + '.Different'
            elif mutation == 'package':
                evaluation['evaluations'][0]['layer'] = '/Game/Other.ABP_Other'
            else:
                evaluation['evaluations'].append(copy.deepcopy(evaluation['evaluations'][0]))
            self.write('evaluation.json', evaluation)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.load()


if __name__ == '__main__':
    unittest.main()
