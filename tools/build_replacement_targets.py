"""Build catalog/replacement-targets.json: the package ids of every game asset that sits in a
character folder CSS can wear from, keyed by that folder.

A replacement mod is an IoStore container (.utoc/.ucas) without CSS metadata that ships game
assets at their original paths, so the engine loads the mod's copy instead of the game's. The
container names its packages only by id, a CityHash64 of the lowercase UTF-16 package path, so
CSS cannot read folder names out of it. This table lets the runtime turn those ids back into
"this mod touches the Knight Lady folder" with one lookup per chunk and no decompression.

Inputs
- work/research/list/files.txt: every file in the game's containers (GameDump tool, `list` mode).
- packaging/catalog/npc-appearances.css.json: the Use NPC / Enemy roster, one mesh per variant.
- OFFICIAL_SHELLS below: the official shells' default meshes, as the game reports them at
  runtime (runtime/status.json, catalog.original_shell_choices). CSS discovers those live; this
  copy only decides which folders the table covers.

A folder is the character's own folder under Characters/<Group>/, one level deeper inside the
family folders listed in FAMILIES (Brigands and the MS1 Fallgrim set keep one sub-folder per
enemy). Every .uasset/.umap under a folder is one entry. Shared folders (Shells/_Shared and the
like) hold no wearable mesh and are left out on purpose: a mod that only touches them changes
many characters at once and no single look would be honest to list.

Rebuild after a game patch that adds or moves character assets, then re-run the tests.
"""
import json, re, sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILE_LIST = ROOT / 'work/research/list/files.txt'
ROSTER = ROOT / 'packaging/catalog/npc-appearances.css.json'
OUTPUT = ROOT / 'packaging/catalog/replacement-targets.json'
GAME_BUILD = 'CL93241'   # from the UE4SS usmap name; bump when files.txt is regenerated

OFFICIAL_SHELLS = [
    '/Game/Sparta/Characters/Shells/Tiel/Art/Mesh/SK_Tiel',
    '/Game/Sparta/Characters/Shells/Necrophage/Art/Mesh/SK_Necrophage_ArmoredOn',
    '/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/SK_Sester_Genessa_V6',
    '/Game/Sparta/Characters/Shells/Smert/Mesh/SK_Shell_Smert',
    '/Game/Sparta/Characters/Shells/Harros/Art/Mesh/SK_Harros',
    '/Game/Sparta/Characters/Shells/Eredrim/Art/Mesh/SK_Eredrim',
    '/Game/Sparta/Characters/Shells/ThornBoi/Art/Mesh/SK_ThornBoi',
    '/Game/Sparta/Characters/Shells/Gragu/Art/Mesh/SK_Shell_Gragu',
    '/Game/Sparta/Characters/Shells/KnightLady/Art/Mesh/SK_Shell_KnightLady_V04',
    '/Game/Sparta/Characters/Shells/Solomon/Art/Mesh/SK_Solomon',
]
# Family folders: the character folder is one level below these.
FAMILIES = {'Enemies/Brigands', 'Enemies/Fallgrim'}
# Shared assets every CSS look or animation option depends on, named so a patch container that
# overrides one can be called out by name (a warning, never a block). (folder prefix, name filter)
SHARED = [
    ('/Game/Sparta/Characters/Humans/_Shared/', r'.*'),                      # SKEL_Human_Skeleton: the compatibility gate for every look
    ('/Game/Sparta/Characters/Humans/Player/', r'/(ABP|ABPL|BS)_[^/]*$'),    # the player's animation graphs and blend spaces CSS hooks
    ('/Game/Sparta/Core/Characters/Player/Common/', r'.*'),                  # the shell base classes CSS reads and patches
]

MASK = (1 << 64) - 1
K0 = 0xc3a5c85c97cb3127
K1 = 0xb492b66fbe98f273
K2 = 0x9ae16a3b2f90404f
K_MUL = 0x9ddfea08eb382d69


def _fetch64(s, i):
    return int.from_bytes(s[i:i + 8], 'little')


def _fetch32(s, i):
    return int.from_bytes(s[i:i + 4], 'little')


def _rot(v, n):
    return v if n == 0 else ((v >> n) | (v << (64 - n))) & MASK


def _shift_mix(v):
    return v ^ (v >> 47)


def _bswap64(v):
    return int.from_bytes(v.to_bytes(8, 'little'), 'big')


def _hash_len16(u, v, mul=K_MUL):
    a = ((u ^ v) * mul) & MASK
    a ^= a >> 47
    b = ((v ^ a) * mul) & MASK
    b ^= b >> 47
    return (b * mul) & MASK


def _hash_len0to16(s):
    n = len(s)
    if n >= 8:
        mul = (K2 + n * 2) & MASK
        a = (_fetch64(s, 0) + K2) & MASK
        b = _fetch64(s, n - 8)
        c = (_rot(b, 37) * mul + a) & MASK
        d = ((_rot(a, 25) + b) * mul) & MASK
        return _hash_len16(c, d, mul)
    if n >= 4:
        mul = (K2 + n * 2) & MASK
        a = _fetch32(s, 0)
        return _hash_len16((n + (a << 3)) & MASK, _fetch32(s, n - 4), mul)
    if n > 0:
        a, b, c = s[0], s[n >> 1], s[n - 1]
        y = (a + (b << 8)) & MASK
        z = (n + (c << 2)) & MASK
        return (_shift_mix((y * K2 ^ z * K0) & MASK) * K2) & MASK
    return K2


def _hash_len17to32(s):
    n = len(s)
    mul = (K2 + n * 2) & MASK
    a = (_fetch64(s, 0) * K1) & MASK
    b = _fetch64(s, 8)
    c = (_fetch64(s, n - 8) * mul) & MASK
    d = (_fetch64(s, n - 16) * K2) & MASK
    return _hash_len16((_rot((a + b) & MASK, 43) + _rot(c, 30) + d) & MASK,
                       (a + _rot((b + K2) & MASK, 18) + c) & MASK, mul)


def _weak32(w, x, y, z, a, b):
    a = (a + w) & MASK
    b = _rot((b + a + z) & MASK, 21)
    c = a
    a = (a + x + y) & MASK
    b = (b + _rot(a, 44)) & MASK
    return (a + z) & MASK, (b + c) & MASK


def _weak32_at(s, i, a, b):
    return _weak32(_fetch64(s, i), _fetch64(s, i + 8), _fetch64(s, i + 16), _fetch64(s, i + 24), a, b)


def _hash_len33to64(s):
    n = len(s)
    mul = (K2 + n * 2) & MASK
    a = (_fetch64(s, 0) * K2) & MASK
    b = _fetch64(s, 8)
    c = _fetch64(s, n - 24)
    d = _fetch64(s, n - 32)
    e = (_fetch64(s, 16) * K2) & MASK
    f = (_fetch64(s, 24) * 9) & MASK
    g = _fetch64(s, n - 8)
    h = (_fetch64(s, n - 16) * mul) & MASK
    u = (_rot((a + g) & MASK, 43) + ((_rot(b, 30) + c) & MASK) * 9) & MASK
    v = (((a + g) & MASK) ^ d) + f + 1 & MASK
    w = (_bswap64(((u + v) & MASK) * mul & MASK) + h) & MASK
    x = (_rot((e + f) & MASK, 42) + c) & MASK
    y = ((_bswap64(((v + w) & MASK) * mul & MASK) + g) * mul) & MASK
    z = (e + f + c) & MASK
    a = (_bswap64((((x + z) & MASK) * mul + y) & MASK) + b) & MASK
    b = (_shift_mix((((z + a) & MASK) * mul + d + h) & MASK) * mul) & MASK
    return (b + x) & MASK


def cityhash64(s: bytes) -> int:
    """CityHash64 v1.1, as Unreal's Core/Hash/CityHash.cpp computes it."""
    n = len(s)
    if n <= 16:
        return _hash_len0to16(s)
    if n <= 32:
        return _hash_len17to32(s)
    if n <= 64:
        return _hash_len33to64(s)
    x = _fetch64(s, n - 40)
    y = (_fetch64(s, n - 16) + _fetch64(s, n - 56)) & MASK
    z = _hash_len16((_fetch64(s, n - 48) + n) & MASK, _fetch64(s, n - 24))
    v = _weak32_at(s, n - 64, n, z)
    w = _weak32_at(s, n - 32, (y + K1) & MASK, x)
    x = (x * K1 + _fetch64(s, 0)) & MASK
    remaining = (n - 1) & ~63
    i = 0
    while True:
        x = (_rot((x + y + v[0] + _fetch64(s, i + 8)) & MASK, 37) * K1) & MASK
        y = (_rot((y + v[1] + _fetch64(s, i + 48)) & MASK, 42) * K1) & MASK
        x ^= w[1]
        y = (y + v[0] + _fetch64(s, i + 40)) & MASK
        z = (_rot((z + w[0]) & MASK, 33) * K1) & MASK
        v = _weak32_at(s, i, (v[1] * K1) & MASK, (x + w[0]) & MASK)
        w = _weak32_at(s, i + 32, (z + w[1]) & MASK, (y + _fetch64(s, i + 16)) & MASK)
        z, x = x, z
        i += 64
        remaining -= 64
        if remaining == 0:
            break
    return _hash_len16((_hash_len16(v[0], w[0]) + (_shift_mix(y) * K1) + z) & MASK,
                       (_hash_len16(v[1], w[1]) + x) & MASK)


def package_id(package: str) -> int:
    """FPackageId::FromName: CityHash64 over the lowercased UTF-16 name, no terminator."""
    return cityhash64(package.lower().encode('utf-16-le'))


def game_path(file: str) -> str:
    """<Project>/Content/X/Y.uasset -> /Game/X/Y, the engine's mount of the project content root."""
    return re.sub(r'^[^/]+/Content/', '/Game/', file).rsplit('.', 1)[0]


def character_folder(package: str):
    """The folder key a package path belongs to, or None outside the character trees."""
    m = re.match(r'^(/Game/[^/]+/Characters/)([^/]+)/([^/]+)/(.*)$', package)
    if not m:
        return None
    root, group, name, rest = m.groups()
    if name.startswith('_'):
        return None
    if f'{group}/{name}' in FAMILIES:
        parts = rest.split('/')
        if len(parts) < 2 or parts[0].startswith('_'):
            return None
        return f'{root}{group}/{name}/{parts[0]}/'
    return f'{root}{group}/{name}/'


def main() -> int:
    roster = json.loads(ROSTER.read_text())
    meshes = [v['mesh'].split('.')[0] for o in roster['outfits'] for v in o['variants']] + OFFICIAL_SHELLS
    keys = {}
    for mesh in meshes:
        folder = character_folder(mesh)
        if not folder:
            print(f'not in a character folder, skipped: {mesh}', file=sys.stderr)
            continue
        keys.setdefault(folder, []).append(mesh)
    files = [line.strip() for line in FILE_LIST.read_text().splitlines()]
    packages = sorted({game_path(f) for f in files if f.endswith(('.uasset', '.umap'))})
    folders = {folder: [] for folder in sorted(keys)}
    seen = {}
    for package in packages:
        folder = character_folder(package)
        if folder not in folders:
            continue
        pid = package_id(package)
        if pid in seen and seen[pid] != package:
            raise SystemExit(f'package id collision: {package} and {seen[pid]}')
        seen[pid] = package
        folders[folder].append(pid)
    for folder, meshes_here in keys.items():
        ids = set(folders[folder])
        for mesh in meshes_here:
            if package_id(mesh) not in ids:
                raise SystemExit(f'roster mesh is not in the game file list: {mesh}')
    shared = {}
    for package in packages:
        for prefix, pattern in SHARED:
            if package.startswith(prefix) and re.search(pattern, package):
                pid = package_id(package)
                if pid in seen and seen[pid] != package:
                    raise SystemExit(f'package id collision: {package} and {seen[pid]}')
                shared[f'{pid:016x}'] = package
    document = {
        'schema': 1,
        'generated': date.today().isoformat(),
        'source': {'file_list': str(FILE_LIST.relative_to(ROOT)), 'game_build': GAME_BUILD},
        'id': 'CityHash64 of the lowercase UTF-16LE package path, 16 hex digits each, concatenated',
        'folders': {folder: ''.join(f'{pid:016x}' for pid in sorted(ids)) for folder, ids in folders.items()},
        'shared': dict(sorted(shared.items())),
    }
    OUTPUT.write_text(json.dumps(document, indent=1) + '\n')
    total = sum(len(ids) for ids in folders.values())
    print(f'{OUTPUT.relative_to(ROOT)}: {len(folders)} folders, {total} packages, {len(shared)} shared assets, {OUTPUT.stat().st_size} bytes')
    for folder, ids in folders.items():
        print(f'  {len(ids):5d}  {folder}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
