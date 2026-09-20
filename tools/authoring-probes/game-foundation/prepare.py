"""Prepare isolated game-reference unions, using Python 3.14 and existing exports."""
import argparse
import copy
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
WORK = ROOT / 'CustomShellSystem/work/grip-grounding-v1'
MOD = ROOT / 'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx'


def append_missing(base, additions):
    bones = copy.deepcopy(base)
    indices = {b['name'].casefold(): i for i, b in enumerate(bones)}
    assert len(indices) == len(bones)
    mapping = []
    for old_index, bone in enumerate(additions):
        name = bone['name'].casefold()
        parent = additions[bone['parent']]['name'].casefold() if bone['parent'] >= 0 else None
        if name in indices:
            existing = bones[indices[name]]
            actual_parent = bones[existing['parent']]['name'].casefold() if existing['parent'] >= 0 else None
            assert actual_parent == parent, (bone['name'], actual_parent, parent)
            inherited = True
        else:
            row = copy.deepcopy(bone)
            row['parent'] = indices[parent] if parent else -1
            indices[name] = len(bones)
            bones.append(row)
            inherited = False
        mapping.append(dict(name=bone['name'],input_index=old_index,
                            output_index=indices[name],inherited=inherited))
    assert bones[:len(base)] == base
    assert all(-1 <= b['parent'] < i for i, b in enumerate(bones))
    return bones, mapping


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    output = parser.parse_args().output.resolve()
    assert output.is_relative_to(ROOT / 'CustomShellSystem/work') and not output.exists()
    paths = [WORK / 'game-human-reference-editor-v1/GameHuman1199.mesh.json',
             WORK / 'game-human-reference-editor-v1/MoreBeaute258.mesh.json',
             MOD / 'authoring/reference/SKEL_CSS_Base.refskel.json']
    template, more, css = [json.loads(p.read_text()) for p in paths]
    game = template['bones']
    more = more['bones']
    assert (len(game), len(more), len(css)) == (1199, 258, 379)
    output.mkdir(parents=True)
    reports = {}
    for label, additions, suffix, expected in (
        ('source', more, 'GameHumanMore1275_V1', 1275),
        ('foundation', css, 'GameFoundation1396_V1', 1396),
    ):
        bones, mapping = append_missing(game, additions)
        assert len(bones) == expected
        asset = copy.deepcopy(template)
        asset.update(bones=bones,
                     mesh_package='/Game/CSSAuthoring/DiagnosticReferences/SK_' + suffix,
                     skeleton_package='/Game/CSSAuthoring/DiagnosticReferences/SKEL_' + suffix)
        (output / (label + '.mesh.json')).write_text(json.dumps(asset, indent=2) + '\n')
        reports[label] = dict(raw_bones=len(bones), mapping=mapping)
    reports['input_hashes'] = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    reports['scope'] = ('Diagnostic references only. Game prefix and extension local records retained; '
                        'no mesh rebinding, metadata migration, asset import or game deployment.')
    (output / 'preparation.json').write_text(json.dumps(reports, indent=2) + '\n')
    print(json.dumps(dict(output=str(output), source_bones=1275, foundation_bones=1396)))


if __name__ == '__main__':
    main()
