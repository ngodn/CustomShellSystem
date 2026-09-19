"""Pull the Genesis 2 Female base mesh out of a VaM install via AssetRipper.

AssetRipper 2.0 runs headless with a local web API, so this drives it over HTTP
rather than shelling out per asset:

    AssetRipper.GUI.Free --headless --port 57889
    tools/harvest_g2f.py --vam-data /path/to/VaM_Data --out work/.../g2f.glb

The mesh has to be the **base** resolution figure, 21556 vertices. Every VaM
morph indexes against that number, so a mesh with any other count is the wrong
export and silently produces garbage when deltas are applied. This refuses
anything else rather than writing a file that looks fine and is not.

VaM names the figure mesh inconsistently across versions, so candidates are
matched on a pattern and then filtered by vertex count, which is the reliable
discriminator.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

G2F_VERTICES = 21556
# The genitalia geograft, needed by morphs that index past the base (BodyBase,
# NippleSize, mons, and AshAuryn's genital sets).
GRAFT_VERTICES_RANGE = (1400, 1500)

CANDIDATE = re.compile(r'genesis|g2f|female.*base|base.*female|femalebody', re.I)


class Ripper:
    def __init__(self, base: str):
        self.base = base.rstrip('/')

    def _call(self, path: str, data: dict | None = None, raw: bool = False, timeout: int = 900):
        url = f'{self.base}{path}'
        body = urllib.parse.urlencode(data).encode() if data is not None else None
        request = urllib.request.Request(url, data=body, method='POST' if data is not None else 'GET')
        if data is not None:
            request.add_header('Content-Type', 'application/x-www-form-urlencoded')
        with urllib.request.urlopen(request, timeout=timeout) as response:
            payload = response.read()
        return payload if raw else payload.decode('utf-8', errors='replace')

    def alive(self) -> bool:
        try:
            self._call('/', timeout=10)
            return True
        except Exception:
            return False

    def load_folder(self, path: Path):
        return self._call('/LoadFolder', {'Path': str(path)})

    def collections(self) -> int:
        try:
            return int(self._call('/Collections/Count', timeout=60).strip())
        except Exception:
            return -1

    def meshes(self) -> list[tuple[str, str]]:
        """Every mesh asset as (path, display name), scraped from the asset view."""
        html = self._call('/Assets/View', timeout=600)
        found = []
        for match in re.finditer(r'href="(/Assets/[^"]*Path=([^"&]+)[^"]*)"[^>]*>([^<]{1,120})<', html):
            found.append((urllib.parse.unquote(match.group(2)), match.group(3).strip()))
        return found

    def model_glb(self, asset_path: str) -> bytes:
        query = urllib.parse.urlencode({'Path': asset_path})
        return self._call(f'/Assets/Model.glb?{query}', raw=True, timeout=900)


def glb_vertex_count(blob: bytes) -> int:
    """Total POSITION accessor count across every primitive in a .glb."""
    if blob[:4] != b'glTF':
        raise ValueError('not a glb')
    # header: magic, version, length; then chunks of (length, type, data)
    offset, total = 12, 0
    json_chunk = None
    while offset < len(blob):
        length = int.from_bytes(blob[offset:offset + 4], 'little')
        kind = blob[offset + 4:offset + 8]
        if kind == b'JSON':
            json_chunk = blob[offset + 8:offset + 8 + length]
            break
        offset += 8 + length + (-length % 4)
    if json_chunk is None:
        raise ValueError('no JSON chunk in glb')
    doc = json.loads(json_chunk)
    accessors = doc.get('accessors', [])
    for mesh in doc.get('meshes', []):
        for primitive in mesh.get('primitives', []):
            index = primitive.get('attributes', {}).get('POSITION')
            if index is not None:
                total += accessors[index].get('count', 0)
    return total


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--vam-data', required=True, type=Path, help='the VaM_Data folder')
    parser.add_argument('--out', required=True, type=Path, help='where to write the .glb')
    parser.add_argument('--api', default='http://localhost:57889')
    parser.add_argument('--report', type=Path, default=None)
    parser.add_argument('--expect', type=int, default=G2F_VERTICES,
                        help=f'required vertex count (default {G2F_VERTICES})')
    parser.add_argument('--all-candidates', action='store_true',
                        help='write every candidate mesh, not just the match')
    args = parser.parse_args(argv)

    ripper = Ripper(args.api)
    if not ripper.alive():
        print(f'AssetRipper is not answering on {args.api}.\n'
              f'Start it with:  AssetRipper.GUI.Free --headless --port '
              f'{urllib.parse.urlparse(args.api).port}', file=sys.stderr)
        return 2

    if not args.vam_data.is_dir():
        print(f'no such folder: {args.vam_data}', file=sys.stderr)
        return 2

    print(f'loading {args.vam_data} ...', flush=True)
    start = time.time()
    ripper.load_folder(args.vam_data)
    print(f'  loaded in {time.time() - start:.0f}s, {ripper.collections()} collections', flush=True)

    assets = ripper.meshes()
    print(f'  {len(assets)} asset links in the view', flush=True)
    candidates = [(p, n) for p, n in assets if CANDIDATE.search(n) or CANDIDATE.search(p)]
    print(f'  {len(candidates)} name-matched candidates', flush=True)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    results, winner = [], None
    for path, name in candidates:
        try:
            blob = ripper.model_glb(path)
            count = glb_vertex_count(blob)
        except Exception as exc:
            results.append({'name': name, 'path': path, 'error': str(exc)[:200]})
            continue
        row = {'name': name, 'path': path, 'vertices': count}
        results.append(row)
        marker = ''
        if count == args.expect:
            marker = '  <- BASE MATCH'
            if winner is None:
                winner = (path, name, blob)
        elif GRAFT_VERTICES_RANGE[0] <= count <= GRAFT_VERTICES_RANGE[1]:
            marker = '  <- looks like the genitalia geograft'
        print(f'    {count:>8} verts  {name[:60]}{marker}', flush=True)
        if args.all_candidates:
            safe = re.sub(r'[^A-Za-z0-9_.-]', '_', name)[:80]
            (args.out.parent / f'candidate_{count}_{safe}.glb').write_bytes(blob)

    if args.report:
        args.report.write_text(json.dumps(results, indent=2))
        print(f'report -> {args.report}')

    if winner is None:
        print(f'\nNo mesh with exactly {args.expect} vertices. The morphs will not apply to '
              f'anything else, so nothing was written.\n'
              f'Re-run with --all-candidates to dump what was found.', file=sys.stderr)
        return 1

    path, name, blob = winner
    args.out.write_bytes(blob)
    print(f'\nwrote {args.out}  ({len(blob) / 1e6:.1f} MB)\n  from: {name}\n  verts: {args.expect}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
