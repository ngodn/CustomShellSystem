"""Fit one bounded CW face basis against every accepted performance's source poses."""
import argparse
import hashlib
import json
from pathlib import Path
import re

import numpy as np

from cw_facial_basis import compact_basis, filtered_shapes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compression-error-cm', type=float, default=.002)
    parser.add_argument('--anchor', help='Keep one accepted performance basis and its coefficients unchanged')
    parser.add_argument('--catalog', type=Path, help='With an anchor, retain the original controls actually used by this catalog')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    if bool(args.anchor) != bool(args.catalog):
        raise ValueError('Anchoring requires the original catalog for expression and performance preservation')
    if not np.isfinite(args.compression_error_cm) or not 0 < args.compression_error_cm <= .003:
        raise ValueError('Compression must leave room within the unchanged 0.015 cm source limit')
    protected, expected = {}, {}

    def protect(path):
        path = path.resolve(strict=True)
        with path.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        name = str(path)
        if expected.get(name, checksum) != checksum or protected.setdefault(name, checksum) != checksum:
            raise ValueError('Verified input changed: '+name)
        return path

    def read(path):
        return json.loads(protect(path).read_text())

    work = args.work.resolve(strict=True)
    layout_path = work/'cw-retained-basis-v1/report.json'
    layout = read(layout_path)
    bake = read(work/'cw-expression-bake-v1/report.json')
    transfer = read(work/'cw-performance-fitted-v1/report.json')
    if not layout.get('complete') or not bake.get('sources_unchanged') or not transfer.get('complete'):
        raise ValueError('Require the verified CW source layout and fitted transfer')
    spec = read(args.inputs)
    if not isinstance(spec, list) or not 1 <= len(spec) <= 6:
        raise ValueError('Use a bounded set of verified CW performances')
    entries, labels, clips = [], set(), set()
    names = layout['existing_names']
    if len(names) != 20:
        raise ValueError('The retained expression, jaw and blink controls changed')
    for row in spec:
        if (set(row) != {'name', 'basis', 'source', 'holdouts'} or
                not re.fullmatch(r'[a-z0-9]+', row['name']) or row['name'] in labels):
            raise ValueError('Invalid or duplicate performance input')
        labels.add(row['name'])
        basis, source, holdouts = (Path(row[k]).resolve(strict=True) for k in ('basis', 'source', 'holdouts'))
        report = read(basis/'report.json')
        if not report.get('passed') or report['limit_cm'] != .015 or report['clip'] in clips:
            raise ValueError('Require distinct accepted source fits')
        clips.add(report['clip'])
        for path, checksum in report['protected'].items():
            if expected.setdefault(path, checksum) != checksum:
                raise ValueError('Accepted fits disagree on their source: '+path)
        plan = read(source/'plan.json')
        if plan['clip'] != report['clip'] or not plan.get('prepared') or not 1 <= len(plan['samples']) <= 16384:
            raise ValueError('Source plan differs from the accepted fit')
        with np.load(protect(basis/'basis.npz')) as data:
            active, shapes = data['active'].copy(), data['shapes'][:len(names)].copy()
            if data['names'][:len(names)].tolist() != names or not np.array_equal(data['times'], [r['seconds'] for r in plan['samples']]):
                raise ValueError('Retained control identities or sample times changed')
            anchored = ({key: data[key].copy() for key in ('names', 'shapes', 'coefficients')}
                        if row['name'] == args.anchor else None)
        entries.append(dict(name=row['name'], source=source, holdouts=holdouts, report=report,
                            plan=plan, active=active, old_shapes=shapes, basis=basis, anchored=anchored))
    active = np.unique(np.concatenate([entry['active'] for entry in entries]))
    if not 1 <= len(active) <= 20000 or active.min() < 0:
        raise ValueError('Invalid active facial topology')

    def points(path):
        path = path.resolve(strict=True)
        if str(path) not in expected:
            raise ValueError('Source geometry was not part of an accepted fit: '+str(path))
        with np.load(protect(path)) as data:
            return np.concatenate([data[name] for name in layout['parts']])

    neutral = None
    previous = None
    for entry in entries:
        report = entry['report']
        entry['neutral'] = Path(report.get('neutral_reference') or entry['source']/'samples/neutral.npz')
        entry['holdout_neutral'] = Path(report.get('holdout_neutral_reference') or entry['holdouts']/'samples/neutral.npz')
        current = points(entry['neutral'])
        if not np.array_equal(current, points(entry['holdout_neutral'])) or (neutral is not None and not np.array_equal(neutral, current)):
            raise ValueError('Original neutral geometry differs between performances')
        neutral = current
        old = np.zeros((len(names), len(active), 3))
        old[:, np.searchsorted(active, entry['active'])] = entry['old_shapes']
        if previous is not None and not np.array_equal(previous, old):
            raise ValueError('Existing expression or jaw geometry differs between fits')
        previous = old
    if active.max() >= len(neutral):
        raise ValueError('Active point outside source topology')
    anchor = None
    original_count = len(names)
    anchor_shapes = []
    if args.anchor:
        anchor = next((entry for entry in entries if entry['name'] == args.anchor), None)
        if anchor is None:
            raise ValueError('Anchor is not one of the verified inputs')
        catalog = read(args.catalog)
        face = next(f for f in catalog['catalog']['animation_library']['faces'] if f['id'] == 'css.commanderwhite.face')
        used = {name for expression in face['expressions'] for name in expression['weights']}
        used.update(curve['morph'] for performance in face['performances'] for curve in performance['curves'])
        if not used or not used <= set(names):
            raise ValueError('Original catalog uses controls outside the preserved source basis')
        selected = [i for i, name in enumerate(names) if name in used]
        omitted = [i for i in range(len(names)) if i not in selected]
        if np.any(anchor['anchored']['coefficients'][:, omitted] != 0):
            raise ValueError('Anchored performance uses an omitted original control')
        original_count = len(selected)
        selected.extend(range(len(names), len(anchor['anchored']['names'])))
        names = anchor['anchored']['names'][selected].tolist()
        previous = np.zeros((len(names), len(active), 3))
        previous[:, np.searchsorted(active, anchor['active'])] = anchor['anchored']['shapes'][selected]
        anchor['coefficients'] = anchor['anchored']['coefficients'][:, selected]
        anchor_payload = read(anchor['basis']/'facial-shapes.json')
        if protected[str((anchor['basis']/'facial-shapes.json').resolve())] != anchor['report']['sha256']:
            raise ValueError('Anchored fitted payload changed')
        payload_names = [row['name'] for row in anchor_payload['morph_targets']]
        if len(set(payload_names)) != len(payload_names) or set(payload_names) != set(names[original_count:]):
            raise ValueError('Anchored payload has duplicate or missing control identities')
        remap = np.searchsorted(active, anchor['active'])
        for row in anchor_payload['morph_targets']:
            if row['name'] not in names[original_count:]:
                raise ValueError('Anchored payload contains an unexpected shape')
            restored = np.zeros((len(active), 3), dtype=np.float32)
            for point, x, y, z in row['deltas']:
                restored[remap[point]] = [x, -y, z]
            restored[np.linalg.norm(restored, axis=1) <= .015] = 0
            if not np.array_equal(restored, previous[names.index(row['name'])]):
                raise ValueError('Anchored basis differs from its fitted payload')
            anchor_shapes.append(dict(name=row['name'], deltas=[
                [int(remap[point]), x, y, z] for point, x, y, z in row['deltas']]))
        if len(anchor_shapes) != len(names)-original_count:
            raise ValueError('Anchored payload omits a retained shape')
    ids = []
    for part in layout['parts']:
        offset = bake['offsets'][part]
        if offset >= bake['offsets']['Commander White Top Mesh']:
            offset += bake['topology_insertion_points']
        ids.extend(range(offset, offset+layout['part_counts'][part]))
    if len(ids) != len(neutral) or len(set(ids)) != len(ids):
        raise ValueError('Source-to-fitted point mapping changed')
    if anchor is not None and anchor_payload['point_indices'] != np.asarray(ids)[anchor['active']].tolist():
        raise ValueError('Anchored fitted topology differs')
    payload = read(work/'cw-performance-fitted-v1/facial-shapes.json')
    if protected[str((work/'cw-performance-fitted-v1/facial-shapes.json').resolve())] != transfer['sha256']:
        raise ValueError('Verified fitted payload changed')
    verified = dict(zip(payload['point_indices'], payload['body_points'], strict=True))
    correspondence = max(float(np.linalg.norm(neutral[p]*[1, -1, 1]-verified[ids[p]])) for p in active)
    if correspondence > .0002:
        raise ValueError('Original neutral differs from the fitted assembly')
    outside = np.ones(len(neutral), dtype=bool)
    outside[active] = False
    count = sum(len(entry['plan']['samples']) for entry in entries)
    if count > 65536:
        raise ValueError('Combined source set exceeds the offline bound')
    samples = np.empty((count, len(active), 3))
    offset = 0
    for entry in entries:
        rows = entry['plan']['samples']
        entry['slice'] = slice(offset, offset+len(rows))
        for i, row in enumerate(rows):
            actual = points(entry['source']/'samples'/(row['label']+'.npz'))-neutral
            if outside.any() and np.abs(actual[outside]).max() > 1e-9:
                raise ValueError('Source pose moves points outside the accepted active set')
            samples[offset+i] = actual[active]
        offset += len(rows)
        print('CW_COMBINED_SOURCE', entry['name'], len(rows), flush=True)
    flat = previous.reshape(len(names), -1)
    old_weights, _, rank, _ = np.linalg.lstsq(flat.T, samples.reshape(count, -1).T, rcond=.001)
    old_weights = old_weights.T
    train = np.ones(count, dtype=bool)
    if anchor is not None:
        old_weights[anchor['slice']] = anchor['coefficients']
        train[anchor['slice']] = False
    if np.abs(old_weights).max() > 4:
        raise ValueError('Retained control projection exceeds runtime bounds')
    residual = samples[train]-(old_weights[train]@flat).reshape(-1, len(active), 3)
    print('CW_COMBINED_FIT', count, len(active), 'retained_rank', rank, flush=True)
    basis, weights, error = compact_basis(residual, args.compression_error_cm, 128-len(names), method='greedy')
    del residual
    _, gains = filtered_shapes(basis, .015, 32)
    stored = (basis*gains[:, None, None]).astype(np.float32)
    if np.linalg.norm(stored, axis=2).max() > 20:
        raise ValueError('New shapes exceed the unchanged native delta bound')
    filtered = np.where((np.linalg.norm(stored, axis=2) > .015)[:, :, None], stored, 0)
    shapes = np.concatenate((previous, filtered))
    added_weights = np.zeros((count, len(basis)))
    added_weights[train] = weights/gains
    coefficients = np.concatenate((old_weights, added_weights), axis=1).astype(np.float32).astype(float)
    if np.abs(coefficients).max() > 4:
        raise ValueError('Combined coefficients exceed runtime bounds')
    flat = shapes.reshape(len(shapes), -1)
    results = []
    for entry in entries:
        selected = entry['slice']
        values = coefficients[selected]
        source_samples = samples[selected]
        training = 0.
        for start in range(0, len(values), 128):
            prediction = (values[start:start+128]@flat).reshape(-1, len(active), 3)
            training = max(training, float(np.linalg.norm(prediction-source_samples[start:start+128], axis=2).max()))
        times = np.asarray([row['seconds'] for row in entry['plan']['samples']])
        rows = entry['plan']['holdouts']
        query = np.asarray([row['seconds'] for row in rows])
        intermediate = np.stack([np.interp(query, times, values[:, i]) for i in range(len(shapes))], axis=1)
        cases = []
        for start in range(0, len(rows), 128):
            prediction = (intermediate[start:start+128]@flat).reshape(-1, len(active), 3)
            for row, actual in zip(rows[start:start+128], prediction, strict=True):
                target = points(entry['holdouts']/'samples'/(row['label']+'.npz'))-neutral
                maximum = float(np.linalg.norm(target[active]-actual, axis=1).max())
                if outside.any():
                    maximum = max(maximum, float(np.linalg.norm(target[outside], axis=1).max()))
                cases.append(dict(seconds=row['seconds'], error_cm=maximum))
        maximum = max(row['error_cm'] for row in cases)
        results.append(dict(name=entry['name'], clip=entry['plan']['clip'], training_error_cm=training,
                            max_holdout_error_cm=maximum, holdouts=cases, passed=max(training, maximum) <= .015))
        print('CW_COMBINED_CHECK', entry['name'], training, maximum, flush=True)
    new_names = [f'CSSF_CW_Shared_{i:03}' for i in range(len(basis))]
    fitted_shapes = list(anchor_shapes)
    for name, shape in zip(new_names, stored*[1, -1, 1], strict=True):
        selected = np.flatnonzero(np.any(shape != 0, axis=1))
        fitted_shapes.append(dict(name=name, deltas=[[int(i), *shape[i].tolist()] for i in selected]))
    fitted = dict(schema=2, point_indices=np.asarray(ids)[active].tolist(),
                  body_points=(neutral[active]*[1, -1, 1]).tolist(), morph_targets=fitted_shapes)
    encoded = json.dumps(fitted, separators=(',', ':'), allow_nan=False)+'\n'
    if len(encoded.encode()) > 64*1024*1024:
        raise ValueError('Combined fitted payload exceeds the unchanged native input bound')
    for path in list(protected):
        protect(Path(path))
    args.output.mkdir(parents=True)
    output = args.output/'facial-shapes.json'
    output.write_text(encoded)
    checksum = hashlib.sha256(output.read_bytes()).hexdigest()
    for entry, result in zip(entries, results, strict=True):
        folder = args.output/entry['name']
        folder.mkdir()
        (folder/'facial-shapes.json').hardlink_to(output)
        np.savez_compressed(folder/'basis.npz', names=np.asarray(names+new_names), active=active,
                            times=[r['seconds'] for r in entry['plan']['samples']],
                            coefficients=coefficients[entry['slice']], shapes=shapes)
        report = dict(result, protected=protected, added_shapes=len(fitted_shapes), catalog_shapes=len(shapes),
                      source_samples=len(entry['plan']['samples']), basis_error_cm=error,
                      compression_error_cm=args.compression_error_cm,
                      fitted_correspondence_error_cm=correspondence,
                      neutral_reference=str(entry['neutral'].resolve()),
                      holdout_neutral_reference=str(entry['holdout_neutral'].resolve()),
                      limit_cm=.015, sha256=checksum, installed=False,
                      scope='Full combined source geometry and temporal holdouts. Runtime curves, cooking and live acceptance pending.')
        (folder/'report.json').write_text(json.dumps(report, indent=2, allow_nan=False)+'\n')
    passed = all(row['passed'] for row in results)
    (args.output/'report.json').write_text(json.dumps(dict(
        passed=passed, protected=protected, source_samples=count, retained_shapes=original_count,
        anchor=args.anchor, anchor_shapes=len(anchor_shapes), new_basis_shapes=len(basis),
        added_shapes=len(fitted_shapes), total_shapes=len(shapes), compression_error_cm=args.compression_error_cm,
        basis_error_cm=error, fitted_correspondence_error_cm=correspondence, sha256=checksum,
        results=[{k: v for k, v in row.items() if k != 'holdouts'} for row in results], installed=False),
        indent=2, allow_nan=False)+'\n')
    print('CW_COMBINED_COMPLETE', passed, len(shapes), flush=True)
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
