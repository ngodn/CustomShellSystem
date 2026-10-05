"""Source ancestry must survive preparation without inventing transforms."""
import unittest

from prepare_eve import select_bones


class SourceHierarchyTests(unittest.TestCase):
    def test_companion_base_is_retained_in_parent_order(self):
        bones = [dict(name=n, parent=p) for n, p in
                 [('Root', -1), ('Base', 0), ('Body', 1), ('Finger', 2), ('Hair', 2)]]
        self.assertEqual(select_bones(bones, {'Root', 'Body', 'Finger'}, True), [0, 1, 2, 3])
        with self.assertRaises(ValueError):
            select_bones(bones, {'Root', 'Body', 'Finger'})

    def test_existing_complete_selection_is_unchanged(self):
        bones = [dict(name='Root', parent=-1), dict(name='Body', parent=0)]
        for include in (False, True):
            self.assertEqual(select_bones(bones, {'Root', 'Body'}, include), [0, 1])

    def test_missing_named_joint_is_not_replaced_by_another_bone(self):
        with self.assertRaises(ValueError):
            select_bones([dict(name='Root', parent=-1)], {'Root', 'Finger'}, True)

    def test_rejects_duplicate_names_cycles_and_invalid_parents(self):
        for bones in ([dict(name='Root', parent=-1), dict(name='Root', parent=0)],
                      [dict(name='Root', parent=0)], [dict(name='Root', parent=1)],
                      [dict(name='Root', parent=-2)], [dict(name='Root', parent=False)]):
            with self.subTest(bones=bones), self.assertRaises(ValueError):
                select_bones(bones, {'Root'}, True)


if __name__ == '__main__':
    unittest.main()
