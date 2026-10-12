"""Verify the packaged motion trial changes only the intended rig defaults and fingerprints."""
import copy
import hashlib
import json
import math
from pathlib import Path
import sys


def read(path):
    return json.loads(path.read_text())


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


root = Path(__file__).resolve().parents[2]
stage, baseline = (Path(arg).resolve(strict=True) for arg in sys.argv[1:3])
eve, shared = 'CSS_EveStellarBlade_eins0fx_P', 'CSS_SharedAssets_P'
old_proof = read(baseline/'assembly.json')
proof = read(stage/'assembly.json')
assert old_proof['passed'] and proof['passed'] and not proof['installed']
package = '/Game/CSS/eins0fx/EveSB/CR_Body'
old_path = Path('/mnt/eins0fxE/CSSAuthoringWork/three-outfit-rig-physics/eve-final-container1/decoded.json')
old_hash = read(root/'work/three-outfit-rig-physics/peony-v2-motion-compare1/comparison.json')['inputs'][str(old_path)]
assert sha(old_path) == old_hash
old, new = read(old_path)[package], read(stage/'decoded.json')[package]
assert len(new) == len(old) == 4
old_defaults, = [o['Properties'] for o in old if o['Name'] == 'Default__CR_Body_C']
new_defaults, = [o['Properties'] for o in new if o['Name'] == 'Default__CR_Body_C']
angles = [math.radians(v) for v in (6, 6, 10, 10, 3, 3, 5)]
assert len(new_defaults['Limits']) == 7
assert max(abs(a-b) for a, b in zip(new_defaults['Limits'], angles)) < 1e-6
assert new_defaults['MaxTravel'] == [1.5, 1.5, 1, 1, .5, .5, .5]
normalized = copy.deepcopy(new)
defaults, = [o['Properties'] for o in normalized if o['Name'] == 'Default__CR_Body_C']
for name in ('Limits', 'MaxTravel'):
    defaults[name] = old_defaults[name]
assert normalized == old, 'Other cooked rig data changed'
changed = {path for path, digest in proof['payloads'].items()
           if digest != old_proof['payloads'][eve][path]}
assert changed == {package.replace('/Game/', 'MortalShell2/Content/')+suffix
                   for suffix in ('.uasset', '.uexp')}
assert set(proof['payloads']) == set(old_proof['payloads'][eve])
for name in (eve, shared):
    old_manifest, = (baseline/'metadata'/name).rglob('manifest.json')
    new_manifest, = (stage/'metadata'/name).rglob('manifest.json')
    expected, actual = read(old_manifest), read(new_manifest)
    if name == eve:
        expected['containers'] = actual['containers']
        eve_manifest = actual
    else:
        library = expected['catalog']['animation_library']
        count = 0
        for adapters in [library['adapters'], *[face['adapters'] for face in library['faces']]]:
            for adapter in adapters:
                if adapter['source_outfit'] == eve_manifest['id']:
                    adapter['source_toc_sha256'] = eve_manifest['containers']['.utoc']['sha256']
                    adapter['source_bulk_bytes'] = eve_manifest['containers']['.ucas']['bytes']
                    count += 1
        assert count == proof['shared_adapter_updates'] > 0
        for suffix in ('.utoc', '.ucas'):
            relative = shared+'/'+shared+suffix
            assert proof['files'][relative] == old_proof['files'][relative]
    assert expected == actual, 'Unexpected metadata change: '+name
for relative, digest in proof['files'].items():
    assert sha(stage/'pack'/relative) == digest
for label in ('container', 'catalog', 'imports', 'decode', 'roundtrip', 'shared-metadata-readback'):
    assert read(stage/(label+'-exit.json'))['returncode'] == 0
assert read(stage/'imports.json')['passed']
result = dict(passed=True, installed=False, rig_package=package,
              changed_numeric_defaults=14, other_cooked_rig_data_identical=True,
              unchanged_eve_payloads=len(proof['payloads'])-2,
              eve_catalog_unchanged=True, shared_assets_unchanged=True,
              shared_adapter_fingerprints_updated=proof['shared_adapter_updates'],
              assembly_sha256=sha(stage/'assembly.json'), decoded_sha256=sha(stage/'decoded.json'),
              pending=['Native gameplay acceptance'])
(stage/'verified.json').write_text(json.dumps(result, indent=2)+'\n')
print('Packaged rig verified: 14 numeric changes, all other cooked rig data identical')
