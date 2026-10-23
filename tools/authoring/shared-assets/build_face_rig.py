#!/usr/bin/env python3
"""Add (or replace) a facial rig in Shared Assets, derived from an existing rig.

Usage:
  build_face_rig.py --shared <CSS_SharedAssets_P dir> --from-rig css.eve.face --rig css.profaneproxima.face
                    --facial-handoff <facial-handoff.json> --out <dir> [--drop look-down] [--version 1.0.4]

The facial handoff is what the outfit lane writes after building its facial meshes:
  {"outfit": "eins0fx.thornyproxima", "source_toc_sha256": "<64 hex>", "source_bulk_bytes": N,
   "variants": [{"source_mesh": "/Game/...", "target_mesh": "/Game/...", "skeleton": "/Game/...",
                 "morphs": ["jaw_open", ...]}, ...]}

The new rig lists the morphs every facial mesh carries (CSS refuses an expression that writes a
morph the worn mesh lacks), keeps the source rig's expressions and facial performances limited
to those morphs, renames expression ids into the new rig, and drops expressions left without
motion (plus any named with --drop). Only the metadata PAK changes; utoc and ucas are copied
unchanged into <out>/trio.
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
from css_facial_resources import encode, externalize, resolve  # noqa: E402

REPAK = Path(os.environ.get('CSS_REPAK', CSS / 'build/repak/release/repak'))
SHARED = 'CSS_SharedAssets_P'
RELATIVE = Path('MortalShell2/Content/CSS/Packages/css.locomotion')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(out, name, args):
    proc = subprocess.run([str(a) for a in args], capture_output=True, text=True, env={**os.environ, 'RAYON_NUM_THREADS': '1'})
    (out / f'{name}.log').write_text(proc.stdout + proc.stderr)
    if proc.returncode:
        sys.exit(f'{name} failed ({proc.returncode}); see {out / (name + ".log")}')
    return proc.stdout


def inventory(folder):
    return {str(p.relative_to(folder)): sha(p) for p in sorted(folder.rglob('*')) if p.is_file()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--shared', required=True, type=Path)
    ap.add_argument('--from-rig', required=True)
    ap.add_argument('--rig', required=True)
    ap.add_argument('--facial-handoff', required=True, type=Path)
    ap.add_argument('--revision-handoff', type=Path,
                    help='shared-adapter-handoff.json of the final outfit trio; supplies source_toc_sha256 and source_bulk_bytes')
    ap.add_argument('--drop', action='append', default=[], help='expression name suffix to drop, e.g. look-down')
    ap.add_argument('--replace', action='store_true', help='replace the rig if it already exists')
    ap.add_argument('--version')
    ap.add_argument('--out', required=True, type=Path)
    a = ap.parse_args()

    out = a.out
    if out.exists():
        sys.exit(f'{out} exists; pick a new output folder')
    out.mkdir(parents=True)
    shared = a.shared.resolve(strict=True)
    files = {s: shared / (SHARED + s) for s in ('.pak', '.utoc', '.ucas')}
    hand = json.loads(a.facial_handoff.read_text())
    # Accept the outfit lane's field names too (facial_mesh / facial_morphs) and bare package
    # paths; the core wants full object paths, /Game/Path/Name.Name.
    def object_path(path):
        return path if '.' in path.rsplit('/', 1)[-1] else path + '.' + path.rsplit('/', 1)[-1]
    for v in hand['variants']:
        v.setdefault('target_mesh', v.get('facial_mesh'))
        v.setdefault('morphs', v.get('facial_morphs'))
        if not isinstance(v['morphs'], list):
            sys.exit(f'variant {v.get("variant")} lists no morph names')
        for key in ('source_mesh', 'target_mesh', 'skeleton'):
            v[key] = object_path(v[key])
    if a.revision_handoff:
        rev = json.loads(a.revision_handoff.read_text())['expected_adapters'][0]['adapter']
        if rev['source_outfit'] != hand['outfit']:
            sys.exit('revision handoff is for another outfit')
        hand['source_toc_sha256'], hand['source_bulk_bytes'] = rev['source_toc_sha256'], rev['source_bulk_bytes']
    if 'source_toc_sha256' not in hand:
        sys.exit('no outfit revision: pass --revision-handoff')

    info = run(out, 'pak-info', [REPAK, 'info', files['.pak']])
    mount = next(l.split(': ', 1)[1] for l in info.splitlines() if l.startswith('mount point: '))
    run(out, 'pak-unpack', [REPAK, 'unpack', files['.pak'], '--output', out / 'metadata'])
    folder = out / 'metadata' / RELATIVE
    manifest = json.loads((folder / 'manifest.json').read_bytes())
    resources = {d['file']: (folder / d['file']).read_bytes() for d in manifest.get('facial_resources', [])}
    inline = resolve(manifest, resources)
    faces = inline['catalog']['animation_library']['faces']

    source = next((f for f in faces if f['id'] == a.from_rig), None)
    if not source:
        sys.exit(f'no rig {a.from_rig}')
    existing = [f for f in faces if f['id'] == a.rig]
    if existing and not a.replace:
        sys.exit(f'rig {a.rig} exists; pass --replace')

    variants = hand['variants']
    if not variants:
        sys.exit('facial handoff has no variants')
    carried = [set(v['morphs']) for v in variants]
    common = set.intersection(*carried)
    for v, morphs in zip(variants, carried):
        if morphs != common:
            print(f'note: {v["target_mesh"]} carries {len(morphs - common)} morphs the other variants lack; they are left out')
    unknown = common - set(source['morphs'])
    if unknown:
        sys.exit(f'morphs not in {a.from_rig}: {sorted(unknown)}')
    morphs = [m for m in source['morphs'] if m in common]   # keep the source rig's order
    if not 1 <= len(morphs) <= 128:
        sys.exit(f'{len(morphs)} morphs; a rig needs 1 to 128')

    prefix_from, prefix_to = a.from_rig + '.', a.rig + '.'
    expressions, dropped = [], []
    for e in source['expressions']:
        suffix = e['id'][len(prefix_from):] if e['id'].startswith(prefix_from) else e['id']
        weights = {k: v for k, v in e['weights'].items() if k in common and v}
        if suffix in a.drop or (not weights and suffix != 'neutral'):
            dropped.append(suffix)
            continue
        expressions.append(dict(id=prefix_to + suffix, name=e['name'], weights=weights))
    performances, dropped_clips = [], []
    for p in source.get('performances', []):
        curves = [c for c in p['curves'] if c['morph'] in common]
        if not curves:
            dropped_clips.append(p['clip'])
            continue
        performances.append(dict(clip=p['clip'], duration=p['duration'], curves=curves))
    adapters = [dict(source_outfit=hand['outfit'], source_mesh=v['source_mesh'], target_mesh=v['target_mesh'],
                     skeleton=v['skeleton'], source_toc_sha256=hand['source_toc_sha256'],
                     source_bulk_bytes=hand['source_bulk_bytes']) for v in variants]
    if len(hand['source_toc_sha256']) != 64 or len({(x['source_outfit'], x['source_mesh']) for x in adapters}) != len(adapters):
        sys.exit('bad revision or duplicate source mesh in the handoff')

    rig = dict(id=a.rig, morphs=morphs, expressions=expressions, adapters=adapters)
    if performances:
        rig['performances'] = performances
    faces[:] = [f for f in faces if f['id'] != a.rig] + [rig]
    if len(faces) > 16:
        sys.exit('more than 16 facial rigs')
    if a.version:
        inline['version'] = a.version
        inline['catalog']['animation_library']['version'] = a.version

    new_manifest, new_resources = externalize(inline)
    before = inventory(out / 'metadata')
    for d in manifest.get('facial_resources', []):
        if d['file'] not in new_resources:
            (folder / d['file']).unlink()
    for name, data in new_resources.items():
        (folder / name).write_bytes(data)
    (folder / 'manifest.json').write_bytes(encode(new_manifest))
    after = inventory(out / 'metadata')
    changed = sorted(p for p in set(before) | set(after) if before.get(p) != after.get(p))
    expected = {str(RELATIVE / 'manifest.json'), str(RELATIVE / (a.rig + '.face.json'))}
    if not set(changed) <= expected:
        sys.exit(f'unexpected metadata change: {changed}')

    trio = out / 'trio'
    trio.mkdir()
    pak = trio / (SHARED + '.pak')
    run(out, 'pak-pack', [REPAK, 'pack', out / 'metadata', pak, '--version', 'V8B', '--mount-point', mount])
    run(out, 'pak-readback', [REPAK, 'unpack', pak, '--output', out / 'readback'])
    if inventory(out / 'readback') != after:
        sys.exit('packed metadata differs from the folder')
    back = out / 'readback' / RELATIVE
    back_manifest = json.loads((back / 'manifest.json').read_bytes())
    back_resources = {d['file']: (back / d['file']).read_bytes() for d in back_manifest.get('facial_resources', [])}
    if resolve(back_manifest, back_resources) != inline:
        sys.exit('readback does not resolve to the built catalog')
    for s in ('.utoc', '.ucas'):
        shutil.copy2(files[s], trio / (SHARED + s))
    shutil.rmtree(out / 'readback')
    report = dict(rig=a.rig, from_rig=a.from_rig, morphs=len(morphs), expressions=[e['id'] for e in expressions],
                  dropped_expressions=dropped, performances=[p['clip'] for p in performances],
                  dropped_performances=dropped_clips, adapters=adapters, changed=changed,
                  manifest_bytes=len(encode(new_manifest)),
                  trio={p.name: dict(bytes=p.stat().st_size, sha256=sha(p)) for p in sorted(trio.iterdir())})
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: report[k] for k in ('rig', 'morphs', 'dropped_expressions', 'dropped_performances', 'manifest_bytes')}, indent=1))
    print('expressions', len(expressions), 'performances', len(performances), 'adapters', len(adapters))


if __name__ == '__main__':
    main()
