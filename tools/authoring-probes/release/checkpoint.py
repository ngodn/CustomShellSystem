"""Package the exact tested CSS/Eve checkpoint without rebuilding or touching the game."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'tools'))
from css_package import verify

WORK = ROOT/'work/delivery1'
OUT = ROOT/'dist/delivery-2026-09-21'
OUT.mkdir(exist_ok=True)
archive = OUT/'CSS-Eve-BlackPearl-checkpoint.zip'
assert not archive.exists()
receipt = json.loads((ROOT/'work/stock1/ui-live/deployment.json').read_text())
assert json.loads((ROOT/'work/stock1/ui-check/result.json').read_text())['passed']
package = ROOT/'work/eve1/CSS_EveStellarBlade_eins0fx_P'
manifest = verify(package)
assert manifest['name'] == 'Eve (Stellar Blade)'
variant = manifest['catalog']['outfits'][0]['variants'][0]
assert variant['name'] == 'Black Pearl' and variant['ground_offset_cm'] == -3
runtime = 'Binaries/Win64/ue4ss/Mods/CustomShellSystem/'
core = (ROOT/'work/stock1/ui-live/core.dll').read_bytes()
digest = lambda data: hashlib.sha256(data).hexdigest()
assert digest(core) == receipt['core_sha256']
loader = (WORK/'inputs/main.dll').read_bytes()
contract = json.loads((WORK/'inputs/loader-contract.json').read_text())
assert digest(loader) == contract['dll_sha256'] and contract['abi'] == 1
for blob in (core, loader):
    assert len(blob) >= 64 and blob[:2] == b'MZ'
    offset = struct.unpack_from('<I', blob, 60)[0]
    assert offset+24 <= len(blob) and blob[offset:offset+4] == b'PE\0\0'
    assert struct.unpack_from('<H', blob, offset+4)[0] == 0x8664
    assert struct.unpack_from('<H', blob, offset+22)[0] & 0x2000
readme = (ROOT/'packaging/checkpoint-readme.txt').read_bytes()
files = {
    'README.txt': readme,
    runtime+'README.txt': readme,
    runtime+'enabled.txt': b'',
    runtime+'core.json': (json.dumps(receipt['selector'], indent=2)+'\n').encode(),
    runtime+'dlls/main.dll': loader,
    runtime+'cores/css_core-choices1.dll': core,
    runtime+'assets/inventory-logo-v1.png': (ROOT/'assets/inventory-logo-v1.png').read_bytes(),
    runtime+'THIRD_PARTY_NOTICES.txt': (ROOT/'packaging/THIRD_PARTY_NOTICES.txt').read_bytes(),
}
for path in sorted(package.iterdir()):
    blob = path.read_bytes()
    assert digest(blob) == receipt['package_sha256'][path.name]
    files['Content/Paks/~mods/'+package.name+'/'+path.name] = blob
assert len(files) == 11
assert all(len(name) < 140 and '..' not in Path(name).parts for name in files)
record = dict(kind='playable-development-checkpoint', date='2026-09-21',
    css_source_commit='5f6bb73344d80a33c5ce2c014fd14d9c885f44fe',
    core_build='development, exact tested choices1 binary', loader_contract=contract,
    outfit_version=manifest['version'], experimental_animations=False,
    files={name: digest(blob) for name, blob in sorted(files.items())})
files['checkpoint.json'] = (json.dumps(record, indent=2)+'\n').encode()
with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for name, blob in sorted(files.items()):
        info = zipfile.ZipInfo(name, (2026, 9, 21, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        z.writestr(info, blob)
with zipfile.ZipFile(archive) as z:
    assert len(z.namelist()) == len(files) and set(z.namelist()) == set(files)
    assert z.testzip() is None
    assert all(z.read(name) == blob for name, blob in files.items())
    extracted = WORK/'readback'
    assert not extracted.exists()
    z.extractall(extracted)
    assert verify(extracted/'Content/Paks/~mods'/package.name) == manifest
checksum = digest(archive.read_bytes())
archive.with_suffix('.zip.sha256').write_text(checksum+'  '+archive.name+'\n')
(OUT/'README.txt').write_bytes(readme)
(WORK/'verification.json').write_text(json.dumps(dict(passed=True, archive=str(archive),
    sha256=checksum, bytes=archive.stat().st_size, members=len(files),
    extracted_outfit_verified=True, exact_tested_core=True, personal_state_included=False,
    scope='Archive integrity and exact tested payload, not new live-install or final-release acceptance.'), indent=2)+'\n')
print(archive)
print(checksum)
