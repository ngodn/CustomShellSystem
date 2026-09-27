#!/usr/bin/env python3
"""Check, fix and explore CCS presets against the shipped catalogs.

  preset_check.py check  presets/*.json          # validate; exit 1 on any error
  preset_check.py fix    presets/*.json          # rewrite entries with canonical fields from the catalogs
  preset_check.py list   [--slot L1] [--source Brigands] [--grep swing]   # eligible moves per slot

Mirrors the runtime rules in src/core/core.cpp (eligible) and src/runtime/storage.cpp
(json_to_preset): a preset is one JSON file per name under presets/, schema_version 1,
light_chain {L1 L2 L3 LF LC}, heavy_chain {H1 H2 H3 HF HC}, ranged {R}; each entry names a
catalog move by move_id. Chain moves fit slots 1/2/3 of either chain, finishers only F,
holds only C, sidearm fire only R, enemy melee the chain and finisher slots, enemy ranged R.
Each entry may also carry per-slot tuning: "speed" (0.5 to 2.0, default 1.0), "hit_damage"
("move" or "weapon", default "move") and "weapon" ("inventory" or "move", default "inventory").
An entry with an empty move_id keeps the weapon's own attack and may still carry tuning.
"""
from __future__ import annotations
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / 'data'
LIGHT = ['L1', 'L2', 'L3', 'LF', 'LC']
HEAVY = ['H1', 'H2', 'H3', 'HF', 'HC']
RANGED = ['R']
SLOTS = LIGHT + HEAVY + RANGED
CHAIN = {'L1', 'L2', 'L3', 'H1', 'H2', 'H3'}
FINISHER = {'LF', 'HF'}
HOLD = {'LC', 'HC'}
NAME_RE = re.compile(r'^[A-Za-z0-9_-]{1,64}$')


def load_catalogs() -> dict[str, dict]:
    moves: dict[str, dict] = {}
    player = json.loads((DATA / 'catalog.json').read_text())
    for m in player['moves']:
        slots = set(m.get('slots') or [])
        if 'R' in slots:
            fits = {'R'}
        elif slots & HOLD:
            fits = set(HOLD)
        elif slots & FINISHER:
            fits = set(FINISHER)
        else:
            fits = set(CHAIN)
        payload = m.get('payload') or {}
        moves[m['id']] = {'id': m['id'], 'origin': 'player', 'type': 'hold' if fits == set(HOLD) else 'tarstone_finisher' if fits == set(FINISHER) else 'player',
                          'source': m.get('source_name', ''), 'name': m.get('display_name', ''), 'montage': m['montage'],
                          'ability': m.get('ability', ''), 'fits': fits, 'original_slots': sorted(slots),
                          'info': f"damage {payload.get('damage', '?')}, poise {payload.get('poise', '?')}, reaction {str(payload.get('reaction', '?')).split('.')[-1]}, hits {len(m.get('hits') or [])}"}
    enemy = json.loads((DATA / 'enemy-catalog.json').read_text())
    for m in enemy['moves']:
        name = m.get('display_name', '')
        ranged = any(w in name for w in ('Shoot', 'Crossbow', 'Throw', 'Bow '))
        moves[m['id']] = {'id': m['id'], 'origin': 'enemy', 'type': 'enemy', 'source': m.get('source_name', ''), 'name': name,
                          'montage': m['montage'], 'ability': '', 'fits': {'R'} if ranged else CHAIN | FINISHER, 'original_slots': [],
                          'info': f"{m.get('length', 0):.1f} s, {m.get('hit_windows', 0)} hit window(s), motion warp {'yes' if m.get('motion_warp') else 'no'}"}
    ranged_catalog = json.loads((DATA / 'ranged-catalog.json').read_text())
    for m in ranged_catalog['moves']:
        moves[m['id']] = {'id': m['id'], 'origin': 'player', 'type': 'player', 'source': m.get('source_name', ''), 'name': m.get('display_name', ''),
                          'montage': m['montage'], 'ability': m.get('ability', ''), 'fits': {'R'}, 'original_slots': ['R'], 'info': 'sidearm fire'}
    return moves


def by_montage(moves: dict[str, dict]) -> dict[str, dict]:
    return {m['montage']: m for m in moves.values()}


def check_file(path: Path, moves: dict[str, dict], fix: bool) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text())
    except (OSError, ValueError) as e:
        return [f'{path.name}: unreadable JSON ({e})']
    if not isinstance(data, dict):
        return [f'{path.name}: top level must be an object']
    if data.get('schema_version', 1) != 1:
        errors.append(f'{path.name}: schema_version must be 1')
    name = data.get('preset_name', '')
    if not NAME_RE.match(name):
        errors.append(f'{path.name}: preset_name must be letters, digits, dash or underscore (1-64)')
    elif name != path.stem:
        errors.append(f'{path.name}: preset_name "{name}" must equal the file name "{path.stem}"')
    for key, limit in (('author', 256), ('description', 4096), ('base_weapon', 256)):
        value = data.get(key, '')
        if not isinstance(value, str) or len(value.encode()) > limit:
            errors.append(f'{path.name}: {key} must be a string of at most {limit} bytes')
    if not data.get('description'):
        errors.append(f'{path.name}: description is required (what the combo is for and why the moves fit)')
    montages = by_montage(moves)
    filled = 0
    for chain, slots in (('light_chain', LIGHT), ('heavy_chain', HEAVY), ('ranged', RANGED)):
        block = data.get(chain, {})
        if not isinstance(block, dict):
            errors.append(f'{path.name}: {chain} must be an object')
            continue
        for slot in block:
            if slot not in slots:
                errors.append(f'{path.name}: {chain} cannot hold slot {slot}')
        for slot in slots:
            entry = block.get(slot)
            if entry is None:
                continue
            if not isinstance(entry, dict):
                errors.append(f'{path.name}: {slot} must be an object')
                continue
            speed = entry.get('speed', 1.0)
            if not isinstance(speed, (int, float)) or isinstance(speed, bool) or not 0.5 <= speed <= 2.0:
                errors.append(f'{path.name}: {slot}.speed must be a number from 0.5 to 2.0')
            if entry.get('hit_damage', 'move') not in ('move', 'weapon'):
                errors.append(f'{path.name}: {slot}.hit_damage must be "move" or "weapon"')
            if entry.get('weapon', 'inventory') not in ('inventory', 'move'):
                errors.append(f'{path.name}: {slot}.weapon must be "inventory" or "move"')
            if not entry.get('move_id') and not entry.get('montage'):
                continue   # the weapon's own attack, possibly with tuning
            move = moves.get(entry.get('move_id', '')) or montages.get(entry.get('montage', ''))
            if not move:
                errors.append(f'{path.name}: {slot} names no catalog move (move_id "{entry.get("move_id", "")}", montage "{entry.get("montage", "")}")')
                continue
            if slot not in move['fits']:
                errors.append(f'{path.name}: {slot} cannot take {move["id"]} (fits {", ".join(sorted(move["fits"]))})')
            canonical = {'type': move['type'], 'source': move['source'], 'move_id': move['id'], 'ability': move['ability'], 'montage': move['montage']}
            for field, value in canonical.items():
                if entry.get(field, '') != value:
                    if fix:
                        entry[field] = value
                    else:
                        errors.append(f'{path.name}: {slot}.{field} should be "{value}"')
            filled += 1
    if filled == 0:
        errors.append(f'{path.name}: no slot is filled')
    if fix and not errors:
        path.write_text(json.dumps(data, indent=2) + '\n')
    return errors


def list_moves(moves: dict[str, dict], slot: str | None, source: str | None, grep: str | None) -> None:
    rows = [m for m in moves.values() if (not slot or slot in m['fits']) and (not source or source.lower() in m['source'].lower())
            and (not grep or grep.lower() in (m['name'] + ' ' + m['id']).lower())]
    rows.sort(key=lambda m: (m['origin'], m['source'], m['name']))
    for m in rows:
        print(f"{m['id']}\n    {m['origin']:6} {m['source']:18} {m['name']}\n    fits {', '.join(sorted(m['fits']))}; {m['info']}")
    print(f'{len(rows)} move(s)')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('action', choices=['check', 'fix', 'list'])
    parser.add_argument('files', nargs='*', help='preset JSON files (check, fix)')
    parser.add_argument('--slot', choices=SLOTS)
    parser.add_argument('--source')
    parser.add_argument('--grep')
    args = parser.parse_args()
    moves = load_catalogs()
    if args.action == 'list':
        list_moves(moves, args.slot, args.source, args.grep)
        return 0
    if not args.files:
        parser.error('give at least one preset file')
    errors: list[str] = []
    for file in args.files:
        errors += check_file(Path(file), moves, args.action == 'fix')
    for error in errors:
        print(error)
    print(f'{len(args.files)} file(s), {len(errors)} problem(s)')
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
