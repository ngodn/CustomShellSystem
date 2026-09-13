#!/usr/bin/env python3
"""Convert Mortal Shell II appearance packs into self-contained CSS packages. Python 3.14."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path, PurePosixPath
from string import Template

from convert_beaute import DEFAULT_GAME, DEFAULT_RETOC, names

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_REPAK = ROOT / 'build/repak/release/repak'
DEFAULT_FORMAT = 'CSS_${NAME}_${AUTHORorMODDER}_P'
PACKAGE_ROOT = 'MortalShell2/Content/CSS/Packages'
SUFFIXES = ('.uasset', '.uexp', '.ubulk', '.uptnl', '.m.ubulk')


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def token(value: str) -> str:
    result = re.sub(r'[^A-Za-z0-9_-]+', '_', value.strip()).strip('_-')
    if not result or len(result) > 100:
        raise ValueError('Name and author need 1 to 100 filename-safe characters')
    return result


def output_name(name: str, author: str, pattern: str = DEFAULT_FORMAT) -> str:
    values = {'NAME': token(name), 'AUTHOR': token(author), 'MODDER': token(author), 'AUTHORorMODDER': token(author)}
    for key in sorted(values, key=len, reverse=True):
        pattern=pattern.replace('$'+key,'${'+key+'}')
    try:
        result = Template(pattern).substitute(values)
    except (ValueError, KeyError) as error:
        raise ValueError('Unknown naming token. Use $NAME, $AUTHOR, $MODDER or ${AUTHORorMODDER}') from error
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,179}', result):
        raise ValueError('Output format must produce a filename stem, without paths or extensions')
    if result.upper() in {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1,10)), *(f'LPT{i}' for i in range(1,10))}:
        raise ValueError('Output name is reserved on Windows')
    if not result.endswith('_P'):
        result += '_P'
    return result


def discover(inputs: list[Path]) -> list[Path]:
    """Return one representative per pack, finding companions from any member."""
    candidates: set[Path] = set()
    for source in inputs:
        source = source.resolve(strict=True)
        if source.is_dir():
            candidates.update(p.resolve() for p in source.rglob('*') if p.suffix.lower() in ('.pak', '.utoc', '.ucas'))
        elif source.suffix.lower() in ('.pak', '.utoc', '.ucas'):
            candidates.add(source)
        else:
            raise ValueError(f'Expected a .pak, .utoc, .ucas or pack directory: {source}')
    packs: set[Path] = set()
    for source in sorted(candidates):
        toc, cas = source.with_suffix('.utoc'), source.with_suffix('.ucas')
        if toc.is_file():
            if not cas.is_file():
                raise ValueError(f'Missing companion: {cas}')
            packs.add(toc)
        elif source.suffix.lower() != '.pak':
            raise ValueError(f'Missing companion: {toc}')
        else:
            packs.add(source)
    if not packs:
        raise ValueError('No pack files found')
    if len(packs) > 64:
        raise ValueError('Select at most 64 source packs')
    return sorted(packs)


def asset_info(data: bytes) -> dict:
    """Bounded cooked UE5.6 import/export inspection, matching retoc legacy_asset.rs."""
    entries = names(data)
    if struct.unpack_from('<i', data, 4)[0] != -9:
        raise ValueError('Expected a cooked UE5.6 header; other engine layouts are not accepted')
    pos = entries[0][1]
    flags = struct.unpack_from('<I', data, pos)[0]
    if not flags & 0x80000000:
        raise ValueError('Editor-only data is not supported in CSS packages')
    strings = [n for _, _, n in entries[1:]]
    export_count, export_offset = struct.unpack_from('<ii', data, pos + 28)
    import_count, import_offset = struct.unpack_from('<ii', data, pos + 36)
    stride = 96 if flags & 0x2000 else 112
    for count, offset, size in ((export_count, export_offset, stride), (import_count, import_offset, 32)):
        if not 0 <= count <= 100000 or offset < 0 or offset + count*size > len(data):
            raise ValueError('Import or export table exceeds the cooked header')

    def fname(offset: int) -> str:
        index, number = struct.unpack_from('<ii', data, offset)
        if not 0 <= index < len(strings) or number < 0:
            raise ValueError('Invalid name reference')
        return strings[index] + (f'_{number-1}' if number else '')

    exports = []
    for i in range(export_count):
        offset = export_offset + i*stride
        cls, _, _, outer = struct.unpack_from('<4i', data, offset)
        if cls < 0:
            if -cls > import_count:
                raise ValueError('Export class exceeds import table')
            class_name = fname(import_offset + (-cls-1)*32 + 20)
        elif 0 < cls <= export_count:
            class_name = fname(export_offset + (cls-1)*stride + 16)
        elif cls == 0:
            class_name = 'Class'
        else:
            raise ValueError('Export class exceeds export table')
        exports.append({'name': fname(offset+16), 'class': class_name, 'outer': outer,
                        'asset': struct.unpack_from('<I', data, offset+68)[0] == 1})
    package = entries[0][2]
    if not package.startswith('/Game/') or '..' in package.split('/'):
        raise ValueError(f'Only game content packages are supported: {package}')
    if any('UnknownPackage' in n or 'UnknownExport' in n for _, _, n in entries):
        raise ValueError(f'Unresolved imports in {package}; include the complete game containers')
    return {'package': package, 'exports': exports}


def relocation(pack_id: str, packages: list[str]) -> dict[str, str]:
    prefix = '/Game/CSS/' + hashlib.sha256(pack_id.encode()).hexdigest()[:16] + '/'
    result = {}
    for old in packages:
        budget = len(old.encode('ascii')) - len(prefix)
        if budget < 16:
            raise ValueError(f'Package path is too short for safe isolated relocation: {old}')
        raw = hashlib.shake_256(old.lower().encode()).hexdigest(budget)
        # Bound path components for Windows while preserving every byte offset.
        chars = list(raw[:budget])
        for i in range(63, budget-1, 64):
            chars[i] = '/'
        result[old] = prefix + ''.join(chars)
    if len({p.lower() for p in result.values()}) != len(result):
        raise ValueError('Relocated package collision')
    return result


def replace_references(data: bytes, mapping: dict[str,str]) -> tuple[bytes,int]:
    """Rewrite complete package-name references, with identical encoded lengths."""
    result = data
    count = 0
    for encoding in ('ascii', 'utf-16-le'):
        for old, new in sorted(mapping.items(), key=lambda pair: -len(pair[0])):
            a, b = old.encode(encoding), new.encode(encoding)
            if len(a) != len(b):
                raise ValueError('Relocation cannot resize cooked data')
            # Package references may end here or continue as .Object:Subobject.
            boundary = b'(?=\x00|[.\x27\x22:])' if encoding == 'ascii' else b'(?=\x00\x00|[.\x27\x22:]\x00)'
            result, changed = re.subn(re.escape(a)+boundary, lambda _: b, result)
            count += changed
    return result, count


def png_info(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) > 4*1024*1024 or len(data) < 33 or data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
        raise ValueError('Thumbnail must be a PNG no larger than 4 MiB')
    width, height = struct.unpack_from('>II', data, 16)
    if width != height or not 128 <= width <= 1024:
        raise ValueError('Thumbnail must be square, between 128 and 1024 pixels (512 recommended)')
    return {'file': 'thumbnail.png', 'width': width, 'height': height, 'sha256': digest(path)}


def select_variants(information: list[dict], mesh: str | None, definitions: list[str], name: str) -> list[dict]:
    candidates = [(info['package'], e['name']) for info in information for e in info['exports']
                  if e['class']=='SkeletalMesh' and e['asset'] and e['outer']==0]
    if mesh and definitions:
        raise ValueError('Use either --mesh or --variant, not both')
    choices = '\n'.join(p+'.'+n for p,n in candidates)
    if not definitions:
        matches = [(p,n) for p,n in candidates if p==mesh or p+'.'+n==mesh] if mesh else [
            (p,n) for p,n in candidates if '/Characters/' in p and '/Weapons/' not in p]
        if len(matches)!=1:
            raise ValueError('Choose --mesh or repeat --variant ID=PATH for multiple bodies:\n'+choices)
        p,n=matches[0]
        return [{'id':'default','name':name,'mesh':p+'.'+n}]
    result=[]
    seen=set()
    for definition in definitions:
        variant_id, separator, path=definition.partition('=')
        if not separator or not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9._-]{0,95}',variant_id) or variant_id in seen:
            raise ValueError('Each --variant needs a unique ID=ORIGINAL_OBJECT_PATH')
        matches=[(p,n) for p,n in candidates if p==path or p+'.'+n==path]
        if len(matches)!=1:
            raise ValueError(f'Variant {variant_id} does not select an input skeletal mesh:\n'+choices)
        seen.add(variant_id)
        p,n=matches[0]
        result.append({'id':variant_id,'name':variant_id.replace('_',' ').replace('-',' ').title(),'mesh':p+'.'+n})
    if len(result)>256:
        raise ValueError('A wardrobe entry supports at most 256 variants')
    return result


def material_recipe(path: Path | None, information: list[dict], mapping: dict[str,str]) -> dict[str,str]:
    if path is None:
        return {}
    if path.stat().st_size>64*1024:
        raise ValueError('Material recipe exceeds 64 KiB')
    recipe=json.loads(path.read_text())
    if not isinstance(recipe,dict) or len(recipe)>128:
        raise ValueError('Material recipe must map slot numbers to original material object paths')
    materials={i['package']+'.'+e['name'] for i in information for e in i['exports']
               if e['class'] in ('Material','MaterialInstanceConstant') and e['outer']==0}
    result={}
    for slot,value in recipe.items():
        if not re.fullmatch(r'0|[1-9][0-9]{0,2}',slot) or int(slot)>=128 or not isinstance(value,str) or value not in materials:
            raise ValueError(f'Material slot {slot} must reference a material included in the input pack')
        package,object_name=value.rsplit('.',1)
        result[slot]=mapping[package]+'.'+object_name
    return result


class Converter:
    def __init__(self, retoc: Path, repak: Path, work: Path):
        self.retoc, self.repak, self.work = retoc.resolve(), repak.resolve(), work
        for tool in (self.retoc, self.repak):
            if not tool.is_file():
                raise ValueError(f'Missing tool: {tool}; provide --retoc / --repak')
        self.step = 0

    def run(self, tool: Path, *args: object) -> str:
        self.step += 1
        result = subprocess.run([str(tool), *map(str,args)], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (self.work/f'{self.step:02d}-{args[0]}.log').write_text(result.stdout)
        if result.returncode or re.search(r'\([1-9][0-9]* failed\)', result.stdout):
            raise RuntimeError(f'{args[0]} failed; see {self.work/f"{self.step:02d}-{args[0]}.log"}')
        return result.stdout

    def base_containers(self, game: Path, destination: Path) -> None:
        destination.mkdir()
        source = game/'Content/Paks'
        for name in ('global.utoc', 'global.ucas'):
            if not (source/name).is_file():
                raise ValueError(f'Missing game dependency: {source/name}')
        for path in sorted(source.iterdir()):
            if path.suffix in ('.pak','.utoc','.ucas') and (path.name.startswith('pakchunk') or path.stem=='global'):
                (destination/path.name).symlink_to(path)

    def extract(self, packs: list[Path], game: Path) -> Path:
        containers, legacy = self.work/'containers', self.work/'legacy'
        self.base_containers(game, containers)
        expected: set[str] = set()
        filters = []
        for index, pack in enumerate(packs):
            if pack.suffix == '.utoc':
                listing = self.run(self.retoc, 'list', pack, '--path')
                for line in listing.splitlines():
                    match = re.search(r'\s(ExportBundleData|ShaderCodeLibrary)\s+(.+)$', line)
                    if not match:
                        continue
                    kind, path = match.groups()
                    if not path.startswith('../../../MortalShell2/Content/') or '..' in path[9:].split('/'):
                        raise ValueError(f'Unexpected source mount path: {path}')
                    if path.lower() in {p.lower() for p in expected}:
                        raise ValueError(f'Input packs overlap at {path}; convert alternative versions separately')
                    expected.add(path)
                    filters += ['-f',path]
                for suffix in ('.utoc','.ucas'):
                    # Explicit priority makes even non-_P source names override base assets.
                    (containers/f'CSSInput{index}_4000000000_P{suffix}').symlink_to(pack.with_suffix(suffix))
            pak = pack.with_suffix('.pak')
            if pak.exists():
                contents = self.run(self.repak,'list',pak)
                if contents.strip():
                    loose = self.work/f'loose-{index}'
                    self.run(self.repak,'unpack',pak,'--output',loose)
                    for file in loose.rglob('*'):
                        if file.is_file():
                            rel = file.relative_to(loose)
                            if rel.suffix not in ('.uasset','.uexp','.ubulk','.uptnl','.bin','.json','.ushaderbytecode'):
                                raise ValueError(f'Unsupported non-asset file in source pack: {rel}')
                            target = legacy/rel
                            target.parent.mkdir(parents=True,exist_ok=True)
                            if target.exists():
                                raise ValueError(f'Input packs overlap at {rel}')
                            shutil.copyfile(file,target)
        if filters:
            # Retoc enumerates duplicate base/mod package IDs. Serial conversion avoids concurrent writes.
            self.run(self.retoc,'to-legacy',containers,legacy,'--version','UE5_6','--no-parallel',*filters)
        else:
            self.run(self.retoc,'to-legacy',containers,legacy,'--version','UE5_6','--no-assets','--no-shaders')
        for path in expected:
            rel = path.removeprefix('../../../')
            if path.endswith('.uasset') and not (legacy/rel).is_file():
                raise ValueError(f'Retoc omitted source asset: {path}')
        return legacy


def convert(args: argparse.Namespace) -> Path:
    packs = discover(args.inputs)
    name = args.name or re.sub(r'_P$', '', packs[0].stem)
    stem = output_name(name,args.author,args.name_format)
    pack_id = args.id or (token(args.author)+'.'+token(name)).lower()
    if not re.fullmatch(r'[a-z0-9][a-z0-9._-]{0,95}',pack_id) or '..' in pack_id:
        raise ValueError('Package ID must be a stable lowercase CSS identifier')
    if not name.strip() or len(name.encode())>256 or len((args.description or '').encode())>4096:
        raise ValueError('Display name or description exceeds CSS text limits')
    thumbnail = args.thumbnail.resolve(strict=True)
    thumbnail_meta = png_info(thumbnail)
    output = args.output.resolve()/stem
    if output.exists():
        raise FileExistsError(f'Output already exists: {output}')
    work = args.work.resolve() if args.work else ROOT/'work'/f'convert-{stem}'
    work.mkdir(parents=True,exist_ok=False)
    converter = Converter(args.retoc,args.repak,work)
    game = args.game.resolve(strict=True)
    print(f'Inspecting {len(packs)} source pack(s): {name}',flush=True)
    legacy = converter.extract(packs,game)
    if any(legacy.rglob('*.umap')):
        raise ValueError('Map packages are not wearable appearances; remove them from the conversion input')
    assets = sorted(legacy.rglob('*.uasset'))
    if not assets or len(assets)>4096:
        raise ValueError('Expected 1 to 4096 cooked appearance assets')
    information = {p:asset_info(p.read_bytes()) for p in assets}
    mapping = relocation(pack_id,[info['package'] for info in information.values()])
    if len(mapping) != len(assets):
        raise ValueError('Duplicate package names in source')
    variants=select_variants(list(information.values()),args.mesh,args.variant or [],name)
    overrides=material_recipe(args.materials,list(information.values()),mapping)
    if not overrides:
        bodies={v['mesh'].rsplit('.',1)[0] for v in variants}
        if any(info['package'] in bodies and any('WorldGridMaterial' in n for _,_,n in names(file.read_bytes()))
               for file,info in information.items()):
            print('Source body references WorldGridMaterial. Verify its slots; use --materials for required component overrides.',flush=True)
    match = re.search(r'/Characters/Shells/([^/]+)/',variants[0]['mesh'])
    shells = args.shell or ([f'CharacterId.Player.Shell.{match[1]}'] if match else [])
    if not shells:
        raise ValueError('Cannot infer the source shell; supply --shell CharacterId.Player.Shell.NAME')
    if any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,95}',shell) for shell in shells):
        raise ValueError('Invalid shell tag')
    for variant in variants:
        package,object_name=variant['mesh'].rsplit('.',1)
        variant['mesh']=mapping[package]+'.'+object_name
        if overrides: variant['materials']=overrides
    renamed=work/'renamed'
    report={'schema':1,'package_id':pack_id,'source_packs':[], 'assets':[], 'tools':{
        'retoc_sha256':digest(converter.retoc),'repak_sha256':digest(converter.repak)}}
    for pack in packs:
        for suffix in ('.pak','.utoc','.ucas') if pack.suffix=='.utoc' else ('.pak',):
            source=pack.with_suffix(suffix)
            if source.exists(): report['source_packs'].append({'file':str(source),'sha256':digest(source)})
    print(f'Relocating {len(assets)} assets and their references',flush=True)
    for source,info in information.items():
        old,new=info['package'],mapping[info['package']]
        dest=renamed/'MortalShell2/Content'/new.removeprefix('/Game/')
        dest.parent.mkdir(parents=True,exist_ok=True)
        entry={'original':old,'css':new,'exports':info['exports'],'files':{}}
        for suffix in SUFFIXES:
            file=source.with_suffix(suffix)
            if not file.exists(): continue
            data=file.read_bytes()
            after,changes=replace_references(data,mapping) if suffix in ('.uasset','.uexp') else (data,0)
            restored,_=replace_references(after,{v:k for k,v in mapping.items()}) if changes else (after,0)
            if restored!=data: raise ValueError(f'Inverse relocation failed: {file}')
            target=Path(str(dest)+suffix); target.write_bytes(after)
            entry['files'][suffix]={'source_sha256':hashlib.sha256(data).hexdigest(),
                                    'converted_sha256':digest(target),'references_changed':changes}
        parsed=asset_info(Path(str(dest)+'.uasset').read_bytes())
        if parsed['package']!=new or parsed['exports']!=info['exports']:
            raise ValueError(f'Relocation changed export identity: {old}')
        report['assets'].append(entry)
    shutil.copyfile(legacy/'scriptobjects.bin',renamed/'scriptobjects.bin')
    for file in legacy.rglob('*.ushaderbytecode'):
        target=renamed/file.relative_to(legacy); target.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(file,target)
    for file in legacy.rglob('*.assetinfo.json'):
        target=renamed/file.relative_to(legacy); target.parent.mkdir(parents=True,exist_ok=True)
        text=file.read_text()
        for old,new in mapping.items(): text=text.replace(old,new)
        json.loads(text); target.write_text(text)
    stage=work/'package'; stage.mkdir()
    target=stage/(stem+'.utoc')
    converter.run(converter.retoc,'to-zen',renamed,target,'--version','UE5_6')
    converter.run(converter.retoc,'verify',target)
    checks=work/'check-containers'; converter.base_containers(game,checks)
    for suffix in ('.utoc','.ucas'): (checks/(stem+suffix)).symlink_to(target.with_suffix(suffix))
    converter.run(converter.retoc,'to-legacy',checks,work/'check','--version','UE5_6','--no-parallel','--no-shaders','-f','/CSS/')
    for entry in report['assets']:
        path=work/'check/MortalShell2/Content'/entry['css'].removeprefix('/Game/')
        header=asset_info(Path(str(path)+'.uasset').read_bytes())
        if header['package']!=entry['css'] or header['exports']!=entry['exports']:
            raise ValueError('Packed export identity did not survive re-extraction')
        for suffix,meta in entry['files'].items():
            if suffix!='.uasset' and digest(Path(str(path)+suffix))!=meta['converted_sha256']:
                raise ValueError(f'Cooked payload changed on repacking: {entry["original"]}{suffix}')
    report['verification']='passed: inverse relocation, export identity, IoStore verification and every cooked payload round trip'
    report['runtime_tested']=False
    manifest={'format':'CSS.Package','format_version':1,'id':pack_id,'name':name,'author':args.author,
              'version':args.package_version,'game':'MortalShell2','engine':'5.6','thumbnail':thumbnail_meta,
              'source_url':args.source_url or '', 'thumbnail_source':args.thumbnail_source,
              'containers':{suffix:{'file':stem+suffix,'bytes':target.with_suffix(suffix).stat().st_size,'sha256':digest(target.with_suffix(suffix))} for suffix in ('.utoc','.ucas')},
              'catalog':{'schema':1,'outfits':[{'id':pack_id,'name':name,'author':args.author,
                 'description':args.description or '', 'category':'Shell','shells':shells,'compatibility':'same_skeleton',
                 'thumbnail':'thumbnail.png','variants':variants}]}}
    metadata=work/'metadata'/PACKAGE_ROOT/pack_id; metadata.mkdir(parents=True)
    if getattr(args,'colors',None):
        from css_colors import embed
        embed(args.colors.resolve(strict=True),manifest,metadata)
    (metadata/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (metadata/'conversion.json').write_text(json.dumps(report,indent=2)+'\n')
    shutil.copyfile(thumbnail,metadata/'thumbnail.png')
    target.with_suffix('.pak').unlink() # Replace retoc's empty marker with the CSS metadata archive.
    converter.run(converter.repak,'pack',work/'metadata',target.with_suffix('.pak'),'--version','V8B')
    unpacked=work/'metadata-check'
    converter.run(converter.repak,'unpack',target.with_suffix('.pak'),'--output',unpacked)
    for source in metadata.iterdir():
        if digest(source)!=digest(unpacked/PACKAGE_ROOT/pack_id/source.name):
            raise ValueError('Embedded metadata or thumbnail failed byte verification')
    # Publish the three-file package only after every verification succeeds.
    output.parent.mkdir(parents=True,exist_ok=True)
    staging=Path(tempfile.mkdtemp(prefix=f'.{stem}-',dir=output.parent))
    try:
        for source in stage.iterdir(): shutil.copy2(source,staging/source.name)
        if output.exists(): raise FileExistsError(output)
        staging.rename(output)
    finally:
        if staging.exists(): shutil.rmtree(staging)
    print(f'Verified CSS package: {output}\nMetadata and thumbnail are embedded in {stem}.pak',flush=True)
    return output


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs',nargs='+',type=Path)
    parser.add_argument('--name',help='Display name; defaults to the first pack filename without _P')
    parser.add_argument('--author','--modder',dest='author',required=True)
    parser.add_argument('--name-format',default=DEFAULT_FORMAT,help='Filename template; quote $ tokens in the shell')
    parser.add_argument('--id',help='Stable package ID, independent of the output filename')
    parser.add_argument('--package-version',default='1.0.0')
    parser.add_argument('--thumbnail',required=True,type=Path,help='Author-supplied square PNG, preferably 512x512')
    parser.add_argument('--thumbnail-source',default='author-provided',help='Artwork provenance, such as rendered-from-source-mesh')
    parser.add_argument('--description')
    parser.add_argument('--source-url')
    parser.add_argument('--mesh',help='Original body package or full object path when automatic selection is ambiguous')
    parser.add_argument('--variant',action='append',help='ID=original mesh path; repeat to group regular/corrupted or other variants')
    parser.add_argument('--materials',type=Path,help='JSON mapping material slot numbers to original material paths, applied to all variants')
    parser.add_argument('--colors',type=Path,help='Author color recipe with adjacent dye PNG resources; embedded in the package')
    parser.add_argument('--shell',action='append',help='Source shell tag; may be repeated')
    parser.add_argument('--game',type=Path,default=DEFAULT_GAME)
    parser.add_argument('--retoc',type=Path,default=DEFAULT_RETOC)
    parser.add_argument('--repak',type=Path,default=DEFAULT_REPAK)
    parser.add_argument('--output',type=Path,default=ROOT/'dist')
    parser.add_argument('--work',type=Path,help='Fresh work directory; retained for inspection')
    args=parser.parse_args()
    try:
        convert(args)
    except (ValueError,OSError,RuntimeError,struct.error) as error:
        parser.exit(1,f'CSS conversion failed: {error}\n')


if __name__=='__main__':
    main()
