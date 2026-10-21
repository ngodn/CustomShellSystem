#!/usr/bin/env python3
"""Stage exact, verified shader-tail replacements in retoc UE5.6 materials. Python 3.14."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys

from convert_beaute import names
from css_convert import asset_info


def patch_asset(header: bytes, payload: bytes, old: bytes, new: bytes) -> tuple[bytes, bytes]:
    info = asset_info(header)
    if len(info['exports']) != 1 or info['exports'][0]['class'] != 'MaterialInstanceConstant':
        raise ValueError('Only single-export MaterialInstanceConstant packages are supported')
    if payload[-4:] != b'\xc1\x83\x2a\x9e' or not payload[:-4].endswith(old):
        raise ValueError('Material does not end with the exact verified donor shader data')
    if len(old) < 1024 or payload.count(old) != 1 or struct.unpack_from('<i', old)[0] != 2:
        raise ValueError('Expected one complete two-platform donor shader block')
    summary = names(header)[0][1]
    export = struct.unpack_from('<i', header, summary + 32)[0]
    size, offset = struct.unpack_from('<qq', header, export + 28)
    if size != len(payload) - 4 or offset != len(header):
        raise ValueError('Unexpected export bounds')
    # The canonical retoc UE5.6 summary has empty generations/engine strings.
    # Refuse other layouts, even if the payload happens to contain a matching tail.
    bulk_offset = summary + 144
    if struct.unpack_from('<q', header, bulk_offset)[0] != len(header) + size:
        raise ValueError('Unexpected retoc UE5.6 bulk-data offset')
    changed = payload[:len(payload) - 4 - len(old)] + new + payload[-4:]
    result = bytearray(header)
    struct.pack_into('<q', result, export + 28, len(changed) - 4)
    struct.pack_into('<q', result, bulk_offset, len(header) + len(changed) - 4)
    return bytes(result), changed


def compatibility(properties: dict) -> dict:
    return {key: properties.get(key) for key in (
        'Parent', 'StaticParametersRuntime', 'StaticParameters',
        'BasePropertyOverrides', 'bHasStaticPermutationResource')}


def same_compiled_shader(data: bytes, resources: list, reference: bytes, reference_resources: list) -> bool:
    if len(resources) != 2 or len(reference_resources) != 2:
        return False
    normalized = copy.deepcopy(resources)
    for material, expected in zip(normalized, reference_resources, strict=True):
        identity = material['LoadedShaderMap']['ShaderMapId']
        old = identity['CookedShaderMapIdHash']
        new = expected['LoadedShaderMap']['ShaderMapId']['CookedShaderMapIdHash']
        if old != new:
            old_bytes, new_bytes = bytes.fromhex(old), bytes.fromhex(new)
            if len(old_bytes) != 20 or len(new_bytes) != 20 or data.count(old_bytes) != 1:
                return False
            data = data.replace(old_bytes, new_bytes)
            identity['CookedShaderMapIdHash'] = new
    return data == reference and normalized == reference_resources


def from_game(argv: list[str]) -> None:
    parser = argparse.ArgumentParser(description='Replace material shaders from compatible current-game donors')
    for name in ('target-readback', 'donor-readback', 'target-slices', 'donor-slices', 'target-legacy', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--compiled-reference-readback', type=Path)
    parser.add_argument('--compiled-reference-slices', type=Path)
    args = parser.parse_args(argv)
    if args.output.exists() or args.output.with_suffix('.json').exists():
        raise ValueError('Output directory and report must both be new')
    targets = json.loads(args.target_readback.read_text())
    donors = json.loads(args.donor_readback.read_text())

    def read_slices(directory, readback):
        result = {}
        for row in json.loads((directory / 'slices.json').read_text()):
            package = row['package']
            if package in result or len(readback[package]) != 1:
                raise ValueError('Duplicate package or multiple material exports')
            if row['resources'] != readback[package][0]['LoadedMaterialResources']:
                raise ValueError('Shader slice disagrees with independent material readback')
            if Path(row['file']).name != row['file']:
                raise ValueError('Expected a local shader slice filename')
            data = (directory / row['file']).read_bytes()
            if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
                raise ValueError('Shader slice identity changed')
            result[package] = data
        return result

    old = read_slices(args.target_slices, targets)
    new = read_slices(args.donor_slices, donors)
    # Several native materials share both the permutation and its exact bytes.
    # Treat these as one donor while retaining distinct compiled alternatives.
    unique = {}
    for package, data in new.items():
        identity = (json.dumps(compatibility(donors[package][0]['Properties']), sort_keys=True),
                    json.dumps(donors[package][0]['LoadedMaterialResources'], sort_keys=True), data)
        unique.setdefault(identity, package)
    new = {package: new[package] for package in unique.values()}
    if bool(args.compiled_reference_readback) != bool(args.compiled_reference_slices):
        raise ValueError('Compiled references require both readback and slices')
    references = json.loads(args.compiled_reference_readback.read_text()) if args.compiled_reference_readback else {}
    reference_slices = read_slices(args.compiled_reference_slices, references) if references else {}
    staged = []
    for path in sorted(args.target_legacy.rglob('*.uasset')):
        header, payload = path.read_bytes(), path.with_suffix('.uexp').read_bytes()
        package = asset_info(header)['package']
        signature = compatibility(targets[package][0]['Properties'])
        matches = [p for p in new if compatibility(donors[p][0]['Properties']) == signature]
        proof = None
        if not matches and references:
            proven = []
            for candidate in new:
                donor_signature = compatibility(donors[candidate][0]['Properties'])
                if any(signature[k] != donor_signature[k] for k in (
                        'Parent', 'BasePropertyOverrides', 'bHasStaticPermutationResource')):
                    continue
                for ref in reference_slices:
                    if compatibility(references[ref][0]['Properties']) != donor_signature:
                        continue
                    if same_compiled_shader(old[package], targets[package][0]['LoadedMaterialResources'],
                                            reference_slices[ref], references[ref][0]['LoadedMaterialResources']):
                        proven.append((candidate, ref))
                        break
            equivalent = []
            for candidate, ref in proven:
                if not any(same_compiled_shader(
                        new[candidate], donors[candidate][0]['LoadedMaterialResources'],
                        new[other], donors[other][0]['LoadedMaterialResources'])
                        for other, _ in equivalent):
                    equivalent.append((candidate, ref))
            proven = equivalent
            if len(proven) == 1:
                matches = [proven[0][0]]
                proof = proven[0][1]
        if len(matches) != 1:
            raise ValueError(f'Expected one compatible game donor for {package}; got {len(matches)}')
        donor = matches[0]
        result = patch_asset(header, payload, old[package], new[donor])
        staged.append((path.relative_to(args.target_legacy), package, donor, proof, result))
    if not staged or len(staged) != len(old):
        raise ValueError('Not every target slice has a legacy material')
    args.output.mkdir(parents=True)
    rows = []
    for relative, package, donor, proof, (header, payload) in staged:
        dest = args.output / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(header)
        dest.with_suffix('.uexp').write_bytes(payload)
        rows.append({'package': package, 'donor': donor, 'compiled_reference': proof,
                     'uasset_sha256': hashlib.sha256(header).hexdigest(),
                     'uexp_sha256': hashlib.sha256(payload).hexdigest()})
    args.output.with_suffix('.json').write_text(json.dumps({
        'scope': 'Current-game shader slices; packed readback and runtime validation still required.',
        'materials': rows}, indent=2) + '\n')
    print(f'Staged {len(rows)} materials from independently verified game shader slices')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original-readback', 'patched-readback', 'original-legacy',
                 'patched-legacy', 'target-audit', 'target-legacy', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.output.with_suffix('.json').exists():
        raise ValueError('Output directory and its JSON report must both be new')
    before = json.loads(args.original_readback.read_text())
    after = json.loads(args.patched_readback.read_text())
    targets = {m['package']: m for m in json.loads(args.target_audit.read_text())['materials']}
    donors = []
    for path in sorted(args.original_legacy.rglob('*.uasset')):
        relative = path.relative_to(args.original_legacy)
        replacement = args.patched_legacy / relative
        header, payload = path.read_bytes(), path.with_suffix('.uexp').read_bytes()
        new_header, new_payload = replacement.read_bytes(), replacement.with_suffix('.uexp').read_bytes()
        package = asset_info(header)['package']
        a, b = before[package], after[package]
        if len(a) != 1 or len(b) != 1:
            raise ValueError('Expected one export per donor')
        if {k: v for k, v in a[0].items() if k != 'LoadedMaterialResources'} != {
                k: v for k, v in b[0].items() if k != 'LoadedMaterialResources'}:
            raise ValueError('Donor patch changed fields other than shader resources')
        # Both supplied maps change their name-table count immediately after the
        # resource count. The independent readback and byte reconstruction gate
        # this narrow format instead of assuming an arbitrary diff is a shader.
        first = next(i for i, (x, y) in enumerate(zip(payload, new_payload)) if x != y)
        start = first - 4
        old, new = payload[start:-4], new_payload[start:-4]
        if struct.unpack_from('<i', new)[0] != 2:
            raise ValueError('Patched donor must also contain two platforms')
        if patch_asset(header, payload, old, new) != (new_header, new_payload):
            raise ValueError('Replacement does not reproduce the supplied patch byte for byte')
        donors.append((package, compatibility(a[0]['Properties']), old, new))
    if not donors:
        raise ValueError('No verified donors')
    staged = []
    for path in sorted(args.target_legacy.rglob('*.uasset')):
        header, payload = path.read_bytes(), path.with_suffix('.uexp').read_bytes()
        package = asset_info(header)['package']
        properties = targets[package]['properties']
        matches = [(p, old, new) for p, signature, old, new in donors
                   if signature == compatibility(properties) and payload[:-4].endswith(old)]
        if len(matches) != 1:
            raise ValueError(f'Expected one compatible exact donor for {package}; got {len(matches)}')
        donor, old, new = matches[0]
        result = patch_asset(header, payload, old, new)
        staged.append((path.relative_to(args.target_legacy), package, donor, result))
    if not staged:
        raise ValueError('No target materials')
    args.output.mkdir(parents=True)
    rows = []
    for relative, package, donor, (header, payload) in staged:
        target = args.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(header)
        target.with_suffix('.uexp').write_bytes(payload)
        rows.append({'package': package, 'donor': donor,
                     'uasset_sha256': hashlib.sha256(header).hexdigest(),
                     'uexp_sha256': hashlib.sha256(payload).hexdigest()})
    args.output.with_suffix('.json').write_text(json.dumps({
        'scope': 'Staged shader replacements only. Requires container readback and current-game validation.',
        'materials': rows}, indent=2) + '\n')
    print(f'Staged {len(rows)} materials from {len(donors)} verified donor blocks')


if __name__ == '__main__':
    if sys.argv[1:2] == ['from-game']:
        from_game(sys.argv[2:])
    else:
        main()
