"""Measure actual GPU output and rear-panel exposure in the isolated cloth study."""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--mesh-audit', type=Path, required=True)
    parser.add_argument('--morph-audit', type=Path, required=True)
    parser.add_argument('--basis', action='store_true', help='Read the independent-basis study without overwriting the first trial evidence.')
    args = parser.parse_args()
    suffix = '-basis-readback' if args.basis else '-readback'
    receipt = json.loads((args.stage / f'result{suffix}.json').read_text())
    assert len(receipt['captures']) == 24
    audit = json.loads(args.mesh_audit.read_text())
    hashes = {}

    def load(path):
        content = path.read_bytes()
        hashes[str(path)] = hashlib.sha256(content).hexdigest()
        return json.loads(content)

    rows = []
    for variant in ('Eve', 'Genessa'):
        package = f'/Game/CSS/eins0fx/UnholyGenessa/Faces/SK_{variant}W3_Face'
        lod, = audit[package]['mesh']['render_lods']
        source = load(args.morph_audit / f'SK_{variant}W3_Face.json')
        skirt, = [g for g in source['groups'] if g['slot'] == 'UG_Skirt']
        offsets = []
        base = 0
        for section in lod['sections']:
            if (section['cloth_asset_index'] == skirt['cloth_asset'] and
                    section['vertices'] == len(skirt['vertices'])):
                offsets.append(base)
            base += section['vertices']
        start, = offsets
        assert base == lod['vertices']
        section_slice = slice(start, start + len(skirt['vertices']))
        deltas = np.zeros((len(skirt['vertices']), 3))
        morph, = [m for m in skirt['morphs'] if m['name'] == 'PBMGlutesSize']
        entries = np.asarray(morph['deltas'])
        deltas[entries[:, 0].astype(int)] = entries[:, 1:4]
        cloth_weights = 1 - np.asarray(skirt['cloth_mappings'][0])[:, 15] / 65535
        expected = deltas if args.basis else deltas * cloth_weights[:, None]
        positions = {}
        full_positions = {}
        for size in (0, 1):
            for mode in ('stock', 'trial'):
                captures = [r for r in receipt['captures'] if r['variant'] == variant
                            and r['glute'] == size and r['mode'] == mode]
                assert len(captures) == 2
                path = Path(captures[0]['gpu_readback'])
                data = load(path)
                assert data['passed'] and data['vertices'] == base
                values = np.asarray(data['position_normal_tangent'])
                assert values.shape == (base, 9) and np.isfinite(values).all()
                positions[size, mode] = values[section_slice, :3]
                full_positions[size, mode] = values[:, :3]
        actual = positions[1, 'trial'] - positions[1, 'stock']
        neutral = positions[0, 'trial'] - positions[0, 'stock']
        pixels = {}
        for size in (0, 1):
            for mode in ('legacy', 'stock', 'trial'):
                path = args.stage / f'captures{suffix}' / f'{variant}-{size}-{mode}-back.png'
                hashes[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
                rgb = np.asarray(Image.open(path).convert('RGB')).astype(np.int32)
                assert rgb.shape == (512, 512, 3)
                # Fixed rear view: this rectangle covers the reported rear-panel region.
                region = rgb[140:280, 140:370]
                exposed = (region[:, :, 0] > 25) & (region[:, :, 0] > 2 * region[:, :, 1])
                pixels[f'{size}-{mode}'] = int(exposed.sum())
        noncloth = np.zeros(base, dtype=bool)
        offset = 0
        for section in lod['sections']:
            if section['cloth_asset_index'] < 0:
                noncloth[offset:offset + section['vertices']] = True
            offset += section['vertices']
        noncloth_change = full_positions[1, 'trial'][noncloth] - full_positions[1, 'stock'][noncloth]
        outside_skirt = np.ones(base, dtype=bool)
        outside_skirt[section_slice] = False
        outside_change = full_positions[1, 'trial'][outside_skirt] - full_positions[1, 'stock'][outside_skirt]
        rows.append(dict(variant=variant, skirt_start=start, skirt_vertices=len(deltas),
                         expected_retained_offset_max_cm=float(np.linalg.norm(expected, axis=1).max()),
                         actual_trial_minus_stock_max_cm=float(np.linalg.norm(actual, axis=1).max()),
                         neutral_trial_minus_stock_max_cm=float(np.linalg.norm(neutral, axis=1).max()),
                         missing_offset_max_cm=float(np.linalg.norm(actual - expected, axis=1).max()),
                         noncloth_vertices=int(noncloth.sum()),
                         noncloth_trial_minus_stock_max_cm=float(np.linalg.norm(noncloth_change, axis=1).max()),
                         outside_skirt_trial_minus_stock_max_cm=float(np.linalg.norm(outside_change, axis=1).max()),
                         exposed_body_pixels=pixels))
    hashes[str(args.mesh_audit)] = hashlib.sha256(args.mesh_audit.read_bytes()).hexdigest()
    result = dict(variants=rows, inputs=hashes,
                  scope='GPU reference-pose output and diagnostic rear-view pixels. '
                        'Counts include intended openings; compare matched cases. '
                        'Not gameplay, collision or performance acceptance.')
    output = args.stage / ('gpu-basis-comparison.json' if args.basis else 'gpu-comparison.json')
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(rows, indent=2))


if __name__ == '__main__':
    main()
