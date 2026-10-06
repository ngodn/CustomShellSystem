"""Validate an additional cooked adapter against its pose and graph evidence."""
import hashlib
import json
from pathlib import Path


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def load_addition(specification, moving_ids):
    evidence = {}

    def read(path):
        path = Path(path).resolve(strict=True)
        evidence[str(path)] = digest(path)
        return json.loads(path.read_text())

    def protect(records, aliases=None):
        if not isinstance(records, dict) or not records:
            raise ValueError('Missing adapter evidence hashes')
        for name, checksum in records.items():
            path = Path(name)
            if not path.is_file() or digest(path) != checksum:
                # Editor probes can read an asset through a read-only bind mount.
                # Its package-relative path and bytes must match the cook receipt.
                suffix = name.partition('/Content/')[2]
                matches = [Path(candidate) for candidate, value in (aliases or {}).items()
                           if suffix and candidate.endswith('/Content/' + suffix) and value == checksum]
                if len(matches) != 1 or digest(matches[0]) != checksum:
                    raise ValueError('Adapter evidence changed: ' + name)
                path = matches[0]
            path = str(path.resolve(strict=True))
            evidence[path] = checksum

    spec = read(specification)
    if set(spec) != {'bindings', 'evaluation', 'package_receipt', 'cook_exit', 'cook_root', 'meshes'}:
        raise ValueError('Unexpected adapter addition fields')
    if read(spec['cook_exit']).get('exit_code') != 0:
        raise ValueError('Additional adapter cook failed')
    bindings = read(spec['bindings'])
    if bindings.get('passed') is not True or bindings.get('contract') != 'explicit-weight-v1':
        raise ValueError('Additional cooked graph comparison did not pass')
    protect(bindings['evidence'])
    receipt_path = str(Path(spec['package_receipt']).resolve(strict=True))
    if bindings['evidence'].get(receipt_path) != digest(receipt_path):
        raise ValueError('Cooked comparison does not cover this package receipt')
    receipt = read(receipt_path)
    protect(receipt['protected'])
    evaluated = read(spec['evaluation'])
    if evaluated.get('complete') is not True:
        raise ValueError('Additional adapter evaluation is incomplete')
    protect(evaluated['protected'], receipt['protected'])
    meshes = spec['meshes']
    if not meshes or len(meshes) != len(set(meshes)) or not all(
            isinstance(mesh, str) and mesh.startswith('/Game/') and '.' in mesh for mesh in meshes):
        raise ValueError('Expected distinct mesh paths')
    rows = evaluated.get('evaluations', [])
    covered = set()
    packages = {}
    adapters = bindings['adapters']
    if not adapters or len(adapters) > 32:
        raise ValueError('Expected 1 to 32 additional bindings')
    targets = set()
    for adapter in adapters:
        package, separator, object_name = adapter['target_class'].partition('.')
        if (not separator or not package.startswith('/Game/CSS/Locomotion/Adapters/ABP_') or
                any(part in ('', '.', '..') for part in package[1:].split('/')) or
                object_name != package.rsplit('/', 1)[-1] + '_C' or package in targets):
            raise ValueError('Invalid or duplicate additional adapter target')
        targets.add(package)
        suffix = '/Content/' + package.removeprefix('/Game/') + '.uasset'
        original = [(p, h) for p, h in evaluated['protected'].items() if p.endswith(suffix)]
        cooked_source = [(p, h) for p, h in receipt['protected'].items() if p.endswith(suffix)]
        if len(original) != 1 or len(cooked_source) != 1 or cooked_source[0][1] != original[0][1]:
            raise ValueError('Cooked and evaluated authoring graphs differ')
        protect(dict(original), receipt['protected'])
        payloads = {}
        for extension in ('.uasset', '.uexp'):
            relative = package.removeprefix('/Game/') + extension
            path = Path(spec['cook_root']).resolve(strict=True) / 'CSSAuthoring/Content' / relative
            checksum = receipt['cooked'].get('MortalShell2/Content/' + relative)
            if not checksum or digest(path) != checksum:
                raise ValueError('Additional cooked payload lacks verified readback: ' + relative)
            payloads[extension] = path
            evidence[str(path)] = checksum
        packages[package] = payloads
    mesh_layers = {}
    required_cases = {'inactive', 'active_start', 'active_seek', 'held_end', 'stopped',
                      'null_clip_guard', 'restored', 'zero_weight', 'full_clip'}
    for row in rows:
        package = row['layer'].split('.')[0]
        if (package not in targets or row['mesh'] not in meshes or
                row['layer'] != package + '.' + package.rsplit('/', 1)[-1]):
            raise ValueError('Unexpected additional adapter evaluation target')
        if mesh_layers.setdefault(row['mesh'], package) != package:
            raise ValueError('A mesh was evaluated with conflicting adapter layers')
        cases = row['cases']
        names = [case.get('case') for case in cases]
        if (not required_cases <= set(names) or len(names) != len(set(names)) or
                row.get('finger_bones_checked', 0) <= 0 or
                any(case.get('body_passed') is not True or case.get('fingers_passed') is not True for case in cases) or
                not any(case.get('case') == 'full_clip' and case.get('frames', 0) > 1 for case in cases)):
            raise ValueError('Full body and finger evaluation did not pass')
        pair = (row['mesh'], row['id'])
        if pair in covered:
            raise ValueError('Duplicate additional mesh/clip evaluation')
        covered.add(pair)
    if not moving_ids or not {(mesh, clip) for mesh in meshes for clip in moving_ids} <= covered:
        raise ValueError('Additional adapter evaluation is missing a mesh or moving clip')
    if {row['layer'].split('.')[0] for row in rows} != targets:
        raise ValueError('An additional adapter was not evaluated')
    return adapters, packages, evidence
