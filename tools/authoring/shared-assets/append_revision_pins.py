#!/usr/bin/env python3
"""Build a Shared Assets metadata PAK that adds outfit revision pins.

Usage:
  append_revision_pins.py --shared <installed trio dir> --handoff <handoff.json> [--handoff ...]
                          --out <dir> [--version 1.0.2] [--allow-new]

A handoff file is what a package lane writes after a recook
(`work/uuepbs/step6/shared-adapter-handoff.json`): `expected_adapters`, a list of
{"pointer": "/catalog/animation_library/adapters/0", "adapter": {...row...}}.
The pointer names the array the row belongs to; its index is ignored. With --prune, rows of one outfit whose revision is not listed are removed first
(superseded development builds; the core accepts at most 32 rows per list). Rows are
appended, never replaced: the core keys performance adapters by (outfit, revision
hash, source class) and facial adapters by (outfit, revision hash, source mesh), so
the previous revision keeps resolving and rollback is a trio swap only.

Only the metadata PAK changes. The .utoc and .ucas are copied unchanged into
<out>/trio next to the new .pak with their hashes, ready for the install step.
"""
import argparse
import copy
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
CSS = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(CSS / 'tools'))
from css_facial_resources import encode  # noqa: E402

REPAK = Path(os.environ.get('CSS_REPAK', CSS / 'build/repak/release/repak'))
SHARED = 'CSS_SharedAssets_P'
RELATIVE = Path('MortalShell2/Content/CSS/Packages/css.locomotion/manifest.json')
KEYS = {'adapters': 'source_class', 'faces': 'source_mesh'}


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def run(out, name, args):
    proc = subprocess.run([str(a) for a in args], capture_output=True, text=True, env={**os.environ, 'RAYON_NUM_THREADS': '1'})
    (out / f'{name}.log').write_text(proc.stdout + proc.stderr)
    if proc.returncode:
        sys.exit(f'{name} failed ({proc.returncode}); see {out / (name + ".log")}')
    return proc.stdout


def inventory(folder):
    return {str(p.relative_to(folder)): sha(p) for p in sorted(folder.rglob('*')) if p.is_file()}


def to_bytes(value):
    data = encode(value)
    return data if isinstance(data, bytes) else data.encode('utf-8')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--shared', required=True, type=Path)
    ap.add_argument('--handoff', action='append', default=[], type=Path)
    ap.add_argument('--prune', action='append', default=[], metavar='OUTFIT=HASH[,HASH...]',
                    help='drop every row of OUTFIT whose revision hash does not start with one of HASH (12+ hex chars)')
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--version', default='1.0.2')
    ap.add_argument('--allow-new', action='store_true', help='allow a row whose outfit/source has no earlier revision in the array')
    a = ap.parse_args()

    out = a.out
    if out.exists():
        sys.exit(f'{out} exists; pick a new output folder')
    out.mkdir(parents=True)
    shared = a.shared.resolve(strict=True)
    files = {s: shared / (SHARED + s) for s in ('.pak', '.utoc', '.ucas')}
    for p in files.values():
        p.resolve(strict=True)
    before = {s: dict(bytes=p.stat().st_size, sha256=sha(p)) for s, p in files.items()}

    info = run(out, 'pak-info', [REPAK, 'info', files['.pak']])
    mount = next(l.split(': ', 1)[1] for l in info.splitlines() if l.startswith('mount point: '))
    if 'version: V8B' not in info:
        sys.exit('shared PAK is not V8B')
    run(out, 'pak-unpack', [REPAK, 'unpack', files['.pak'], '--output', out / 'metadata'])
    path = out / 'metadata' / RELATIVE
    original_bytes = path.read_bytes()
    original = json.loads(original_bytes)
    original_sha = hashlib.sha256(original_bytes).hexdigest()
    if (original.get('format'), original.get('kind'), original.get('id')) != ('CSS.Package', 'animation_library', 'css.locomotion'):
        sys.exit('not the css.locomotion metadata manifest')
    for s in ('.utoc', '.ucas'):
        want = dict(file=SHARED + s, **before[s])
        if original['containers'][s] != want:
            sys.exit(f'{s} does not match the manifest containers block: {original["containers"][s]} vs {want}')

    candidate = copy.deepcopy(original)
    library = candidate['catalog']['animation_library']
    appended, skipped, pruned = [], [], []
    for rule in a.prune:
        outfit, _, hashes = rule.partition('=')
        keep = [h for h in hashes.split(',') if h]
        if not outfit or not keep or any(len(h) < 12 or any(c not in '0123456789abcdef' for c in h) for h in keep):
            sys.exit(f'bad --prune {rule}')
        def kept(row):
            return row['source_outfit'] != outfit or any(row['source_toc_sha256'].startswith(h) for h in keep)
        arrays = [('adapters', library['adapters'])] + [(f'faces/{i} ({r["id"]})', r['adapters']) for i, r in enumerate(library.get('faces', []))]
        for where, array in arrays:
            drop = [r for r in array if not kept(r)]
            pruned += [dict(array=where, source_outfit=r['source_outfit'], source_toc_sha256=r['source_toc_sha256']) for r in drop]
            array[:] = [r for r in array if kept(r)]
        for h in keep:
            if not any(r['source_outfit'] == outfit and r['source_toc_sha256'].startswith(h) for _, arr in arrays for r in arr) and not a.handoff:
                print(f'note: no remaining row for {outfit} {h}')
    for hp in a.handoff:
        hand = json.loads(hp.read_text())
        recorded = hand.get('source_manifest_sha256')
        if recorded and recorded != original_sha:
            print(f'note: {hp} was written against manifest {recorded[:12]}, installed is {original_sha[:12]}; rows are matched by content, continuing')
        for entry in hand['expected_adapters']:
            parts = entry['pointer'].strip('/').split('/')
            row = entry['adapter']
            if parts[:2] != ['catalog', 'animation_library']:
                sys.exit(f'unexpected pointer {entry["pointer"]}')
            if parts[2] == 'adapters':
                array, key, where = library['adapters'], KEYS['adapters'], 'adapters'
            elif parts[2] == 'faces':
                rig = library['faces'][int(parts[3])]
                array, key, where = rig['adapters'], KEYS['faces'], f'faces/{parts[3]} ({rig["id"]})'
            else:
                sys.exit(f'unexpected pointer {entry["pointer"]}')
            ident = (row['source_outfit'], row['source_toc_sha256'], row[key])
            same = [r for r in array if (r['source_outfit'], r['source_toc_sha256'], r[key]) == ident]
            if same:
                if same[0] != row:
                    sys.exit(f'{where}: a row with the same outfit/revision/source exists with different fields: {same[0]}')
                skipped.append((where, ident))
                continue
            earlier = [r for r in array if (r['source_outfit'], r[key]) == (row['source_outfit'], row[key])]
            if not earlier and not a.allow_new:
                sys.exit(f'{where}: no earlier revision of {row["source_outfit"]} {row[key]}; pass --allow-new for a new adapter')
            for e in earlier:
                for f in row:
                    if f not in ('source_toc_sha256', 'source_bulk_bytes') and e[f] != row[f]:
                        sys.exit(f'{where}: {f} differs from the earlier revision row: {e[f]} vs {row[f]}')
            array.append(copy.deepcopy(row))
            appended.append((where, ident, row['source_bulk_bytes']))

    if len(library['adapters']) > 32:
        sys.exit('more than 32 performance adapters')
    for rig in library.get('faces', []):
        if len(rig['adapters']) > 32:
            sys.exit(f'more than 32 facial adapters on {rig["id"]}')
    candidate['version'] = a.version
    library['version'] = a.version
    if not appended and not pruned:
        sys.exit('nothing to change; every row is already pinned and nothing was pruned')

    data = to_bytes(candidate)
    if len(data) > 256 * 1024:
        sys.exit(f'manifest would be {len(data)} bytes, over the 256 KiB runtime limit')
    inv_before = inventory(out / 'metadata')
    path.write_bytes(data)
    inv_after = inventory(out / 'metadata')
    changed = [p for p in inv_before if inv_before[p] != inv_after[p]]
    if set(inv_before) != set(inv_after) or changed != [str(RELATIVE)]:
        sys.exit(f'unexpected metadata change: {changed}')

    trio = out / 'trio'
    trio.mkdir()
    pak = trio / (SHARED + '.pak')
    run(out, 'pak-pack', [REPAK, 'pack', out / 'metadata', pak, '--version', 'V8B', '--mount-point', mount])
    run(out, 'pak-readback', [REPAK, 'unpack', pak, '--output', out / 'readback'])
    if inventory(out / 'readback') != inv_after:
        sys.exit('packed metadata differs from the folder')
    if json.loads((out / 'readback' / RELATIVE).read_bytes()) != candidate:
        sys.exit('manifest readback differs')
    for s in ('.utoc', '.ucas'):
        shutil.copy2(files[s], trio / (SHARED + s))
    after = {p.name: dict(bytes=p.stat().st_size, sha256=sha(p)) for p in sorted(trio.iterdir())}
    for s in ('.utoc', '.ucas'):
        if after[SHARED + s]['sha256'] != before[s]['sha256']:
            sys.exit(f'{s} copy differs')
    report = dict(version=a.version, source=str(shared), source_files={SHARED + s: v for s, v in before.items()},
                  source_manifest_sha256=original_sha, manifest_bytes=len(data), mount_point=mount,
                  handoffs=[str(p) for p in a.handoff],
                  appended=[dict(array=w, source_outfit=i[0], source_toc_sha256=i[1], source=i[2], source_bulk_bytes=b) for w, i, b in appended],
                  already_pinned=[dict(array=w, source_outfit=i[0], source_toc_sha256=i[1], source=i[2]) for w, i in skipped],
                  pruned=pruned,
                  adapters=len(library['adapters']), facial=[(r['id'], len(r['adapters'])) for r in library.get('faces', [])],
                  trio=after)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    shutil.rmtree(out / 'readback')
    print(json.dumps(dict(appended=len(appended), pruned=len(pruned), already_pinned=len(skipped), adapters=len(library['adapters']),
                          manifest_bytes=len(data), pak=after[SHARED + '.pak']), indent=1))


if __name__ == '__main__':
    main()
