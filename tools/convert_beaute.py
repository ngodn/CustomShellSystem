#!/usr/bin/env python3
"""Convert locally owned Beaute packs into isolated CSS IoStore packages.

Python 3.14. Only UE5 legacy headers produced by the pinned retoc are accepted.
Equal-length names leave all offsets and opaque cooked payloads intact.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path('/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2')
DEFAULT_RETOC = Path('/home/eins0fx/development/mods/stellar-blade/tools/retoc-build/release/retoc')
NAMESPACE = '/Game/CSSB01'
ASSETS = [
    'Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/' + name for name in (
        'SK_Sester_Genessa_V6', 'Sk_Sester_Genessa_V4_Corrupted',
        'SK_Sester_Genessa_V6_Physics', 'SKEL_Human_Skeleton_AnimBlueprint')
] + [
    'Sparta/Characters/Shells/KnightLady/Art/Mesh/' + name for name in (
        'SK_Shell_KnightLady_V04', 'SKEL_Human_Skeleton_AnimBlueprint')
] + [
    'Sparta/Weapons/Player/GenessasIncenseBurner/Art/Mesh/' + name for name in (
        'SK_GenessasIncenseBurner', 'SM_GenessasIncenseBurner',
        'SM_GenessasIncenseBurner_Handle', 'SM_GenessasIncenseBurner_Body')
]


def names(data: bytes) -> list[tuple[int, int, str]]:
    def i32(offset: int) -> int:
        return struct.unpack_from('<i', data, offset)[0]

    def string(offset: int) -> tuple[int, int, str]:
        size = i32(offset)
        if not 0 < size < 16384:
            raise ValueError('Only bounded ASCII name entries are supported')
        start, end = offset + 4, offset + 4 + size
        if end > len(data) or data[end - 1] != 0:
            raise ValueError('Truncated name entry')
        return start, end, data[start:end - 1].decode('ascii')

    if data[:4] != b'\xc1\x83\x2a\x9e' or i32(4) not in (-8, -9):
        raise ValueError('Unsupported legacy header')
    version = i32(4)
    pos = 48 if version == -9 else 24
    custom_count = i32(pos)
    if not 0 <= custom_count <= 4096:
        raise ValueError('Invalid custom version count')
    pos += 4 + 20 * custom_count
    if version == -8:
        pos += 4
    package = string(pos)
    pos = package[1] + 4  # Skip package flags.
    count, offset = struct.unpack_from('<ii', data, pos)
    if not 0 < count <= 100000 or not pos < offset < len(data):
        raise ValueError('Invalid name table')
    result = [package]
    for _ in range(count):
        entry = string(offset)
        result.append(entry)
        offset = entry[1] + 4  # Two name hashes, zero in retoc output.
        if offset > len(data):
            raise ValueError('Truncated name hashes')
    return result


def rename_header(data: bytes, mapping: dict[str, str]) -> tuple[bytes, list[dict]]:
    out, changes = bytearray(data), []
    for start, end, name in names(data):
        if name not in mapping:
            continue
        new = mapping[name]
        if len(new.encode('ascii')) != end - start - 1:
            raise ValueError('Relocation must preserve byte lengths')
        out[start:end - 1] = new.encode('ascii')
        changes.append({'old': name, 'new': new, 'offset': start})
    # The inverse must reproduce every original byte, not just parsed fields.
    inverse = bytearray(out)
    for change in changes:
        start = change['offset']
        inverse[start:start + len(change['old'])] = change['old'].encode('ascii')
    if bytes(inverse) != data or len(out) != len(data):
        raise ValueError('Relocation round trip failed')
    return bytes(out), changes


def run(*args: object) -> None:
    subprocess.run([str(x) for x in args], check=True)


def convert(game: Path, retoc: Path, work: Path, output: Path, source_packs: Path | None = None) -> None:
    game, work, output = game.resolve(), work.resolve(), output.resolve()
    source_packs = (source_packs or game / 'Content/Paks/~mods').resolve()
    if work.exists():
        raise ValueError(f'Use a fresh work directory: {work}')
    containers = work / 'containers'
    containers.mkdir(parents=True)
    for filename in ('global.utoc', 'global.ucas'):
        source = game / 'Content/Paks' / filename
        if not source.is_file():
            raise FileNotFoundError(source)
        (containers / filename).symlink_to(source)
    # The complete base container set is required to resolve external imports.
    # retoc otherwise emits UnknownPackage placeholders despite reporting success.
    for source in (game / 'Content/Paks').glob('pakchunk*'):
        if source.suffix in ('.utoc', '.ucas', '.pak'):
            (containers / source.name).symlink_to(source.resolve())
    inputs = {}
    for pack in ('BeauteGenessa_P', 'BeauteKnightLady_P'):
        for suffix in ('.pak', '.utoc', '.ucas'):
            source = source_packs / (pack + suffix)
            inputs[source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
            (containers / source.name).symlink_to(source)
    legacy, renamed = work / 'legacy', work / 'renamed'
    filters = [arg for asset in ASSETS for arg in ('-f', asset + '.uasset')]
    run(retoc, 'to-legacy', containers, legacy, '--version', 'UE5_6', '--no-shaders', *filters)
    assets = sorted(legacy.rglob('*.uasset'))
    if len(assets) != 10:
        raise ValueError(f'Expected the known 10 Beaute assets, found {len(assets)}')
    content = legacy / 'MortalShell2/Content'
    mapping = {}
    for asset in assets:
        old = '/Game/' + asset.relative_to(content).with_suffix('').as_posix()
        if not old.startswith('/Game/Sparta/'):
            raise ValueError(f'Unexpected source namespace: {old}')
        mapping[old] = old.replace('/Game/Sparta/', NAMESPACE + '/', 1)
    manifest = {'schema': 1, 'inputs_sha256': inputs, 'namespace': NAMESPACE, 'assets': []}
    for asset in assets:
        old = '/Game/' + asset.relative_to(content).with_suffix('').as_posix()
        new = mapping[old]
        destination = renamed / 'MortalShell2/Content' / (new.removeprefix('/Game/') + '.uasset')
        destination.parent.mkdir(parents=True, exist_ok=True)
        before = asset.read_bytes()
        if any('UnknownPackage' in name or 'UnknownExport' in name for _, _, name in names(before)):
            raise ValueError(f'Unresolved imports in {asset}')
        after, changes = rename_header(before, mapping)
        destination.write_bytes(after)
        payloads = {}
        for suffix in ('.uexp', '.ubulk', '.uptnl'):
            source = asset.with_suffix(suffix)
            if source.exists():
                target = destination.with_suffix(suffix)
                shutil.copyfile(source, target)
                if target.read_bytes() != source.read_bytes():
                    raise ValueError('Cooked payload copy mismatch')
                payloads[suffix] = hashlib.sha256(source.read_bytes()).hexdigest()
        manifest['assets'].append({'original': old, 'css': new, 'changes': changes,
                                  'source_sha256': hashlib.sha256(before).hexdigest(),
                                  'converted_sha256': hashlib.sha256(after).hexdigest(),
                                  'unchanged_payloads': payloads})
    shutil.copyfile(legacy / 'scriptobjects.bin', renamed / 'scriptobjects.bin')
    output.mkdir(parents=True, exist_ok=True)
    target = output / 'CSS_Beaute_P.utoc'
    if any(target.with_suffix(s).exists() for s in ('.utoc', '.ucas', '.pak')):
        raise FileExistsError(target)
    run(retoc, 'to-zen', renamed, target, '--version', 'UE5_6')
    run(retoc, 'verify', target)
    # Re-extract the generated container and compare every opaque export payload.
    check = work / 'check'
    check_containers = work / 'check-containers'
    check_containers.mkdir()
    for source in (containers / 'global.utoc', containers / 'global.ucas',
                   target, target.with_suffix('.ucas'), target.with_suffix('.pak')):
        (check_containers / source.name).symlink_to(source.resolve())
    for source in containers.glob('pakchunk*'):
        (check_containers / source.name).symlink_to(source.resolve())
    run(retoc, 'to-legacy', check_containers, check, '--version', 'UE5_6', '--no-shaders', '-f', '/CSSB01/')
    for source in renamed.rglob('*.uexp'):
        recovered = check / source.relative_to(renamed)
        if source.read_bytes() != recovered.read_bytes():
            raise ValueError(f'IoStore round trip changed payload: {source}')
    manifest['verification'] = 'retoc verify and all 10 export payload round trips passed'
    (output / 'conversion-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=DEFAULT_GAME)
    parser.add_argument('--retoc', type=Path, default=DEFAULT_RETOC)
    parser.add_argument('--work', type=Path, default=ROOT / 'work/conversion')
    parser.add_argument('--output', type=Path, default=ROOT / 'local-packs')
    parser.add_argument('--source-packs', type=Path, help='Original Beaute pack directory, including an installation backup')
    args = parser.parse_args()
    convert(args.game, args.retoc, args.work, args.output, args.source_packs)
