#!/usr/bin/env python3
"""Convert Mortal Shell II appearance packs into self-contained CSS packages. Python 3.14."""
from __future__ import annotations

import argparse
import copy
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
    outers={struct.unpack_from('<i',data,import_offset+i*32+16)[0] for i in range(import_count)}
    for i in range(import_count):
        name=fname(import_offset+i*32+20)
        # Retoc represents unused null import slots as an unreferenced Package
        # placeholder after duplicate imports collapse. An unresolved object or
        # a placeholder used as an object's outer is still a conversion error.
        if 'UnknownExport' in name or ('UnknownPackage' in name and
            (-(i+1) in outers or fname(import_offset+i*32+8)!='Package')):
            raise ValueError(f'Unresolved imports in {package}; include the complete game containers')
    return {'package': package, 'exports': exports}


def repair_imports(legacy:Path,recipe:Path|None) -> list[dict]:
    """Apply explicitly audited import-table substitutions, leaving payloads intact."""
    if recipe is None:return []
    records=json.loads(recipe.read_text())
    if not isinstance(records,list) or len(records)>256:raise ValueError('Invalid import repair recipe')
    headers={names(p.read_bytes())[0][2]:p for p in legacy.rglob('*.uasset')}
    report=[]
    for record in records:
        path=headers[record['package']];data=path.read_bytes()
        if digest(path)!=record['sha256'] or not record.get('reason'):
            raise ValueError('Import repair does not match the audited source header')
        entries=names(data);count,offset=struct.unpack_from('<ii',data,entries[0][1]+36)
        if not 0<=count<=100000 or offset+count*32>len(data):raise ValueError('Invalid import table')
        def fname(pos):
            index,number=struct.unpack_from('<ii',data,pos)
            return entries[index+1][2]+(f'_{number-1}' if number else '')
        repairs=record['replacements'];out=bytearray(data)
        for original,replacement in repairs.items():
            a,b=int(original),replacement
            if not 0<=a<count or type(b) is not int or not 0<=b<count:raise ValueError('Repair index outside import table')
            old_name,new_name=fname(offset+a*32+20),fname(offset+b*32+20)
            if old_name not in ('UnknownExport','/Engine/UnknownPackage') or 'Unknown' in new_name:
                raise ValueError('Repairs may only replace unresolved imports with an existing resolved import')
            out[offset+a*32:offset+(a+1)*32]=data[offset+b*32:offset+(b+1)*32]
        asset_info(bytes(out))
        path.write_bytes(out)
        report.append({**record,'repaired_sha256':digest(path)})
    return report


def relocation(pack_id: str, packages: list[str]) -> dict[str, str]:
    prefix = '/Game/CSS/' + hashlib.sha256(pack_id.encode()).hexdigest()[:16] + '/'
    result = {}
    for old in packages:
        numbered=re.search(r'_(0|[1-9][0-9]*)$',old)
        suffix=numbered[0] if numbered and int(numbered[1])<2147483647 else ''
        base=old[:-len(suffix)] if suffix else old
        budget = len(base.encode('ascii')) - len(prefix)
        if budget < 16:
            raise ValueError(f'Package path is too short for safe isolated relocation: {old}')
        raw = hashlib.shake_256(base.lower().encode()).hexdigest(budget)
        # Bound path components for Windows while preserving every byte offset.
        chars = list(raw[:budget])
        for i in range(63, budget-1, 64):
            chars[i] = '/'
        result[old] = prefix + ''.join(chars) + suffix
    if len({p.lower() for p in result.values()}) != len(result):
        raise ValueError('Relocated package collision')
    return result


def replace_references(data: bytes, mapping: dict[str,str], edits:list|None=None) -> tuple[bytes,int]:
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
            def replace(match):
                if edits is not None:edits.append((match.start(),match.group()))
                return b
            result, changed = re.subn(re.escape(a)+boundary, replace, result, flags=re.IGNORECASE)
            count += changed
    return result, count


def reference_mapping(mapping:dict[str,str]) -> dict[str,str]:
    result=dict(mapping)
    for old,new in mapping.items():
        numbered=re.search(r'_(0|[1-9][0-9]*)$',old)
        if numbered and int(numbered[1])<2147483647:
            root=old[:-len(numbered[0])];target=new[:-len(numbered[0])]
            if root in result and result[root]!=target:raise ValueError('Numbered FName relocation conflict')
            result[root]=target
    return result


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

    def extract(self, packs: list[Path], game: Path, includes: list[str]|None=None) -> Path:
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
                    if not re.fullmatch(r'\.\./\.\./\.\./[A-Za-z0-9_-]+/Content/.+',path) or '..' in path[9:].split('/'):
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
        for package in includes or []:
            if not re.fullmatch(r'/Game/[A-Za-z0-9_/-]+',package) or '..' in package:
                raise ValueError('Included dependency must be a game package path')
            filters += ['-f','/Content/'+package.removeprefix('/Game/')+'.uasset']
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
    if getattr(args,'variant_sources',None):return convert_grouped(args)
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
    legacy = converter.extract(packs,game,getattr(args,'include',None))
    repairs=repair_imports(legacy,getattr(args,'import_repairs',None))
    if any(legacy.rglob('*.umap')):
        raise ValueError('Map packages are not wearable appearances; remove them from the conversion input')
    assets = sorted(legacy.rglob('*.uasset'))
    if not assets or len(assets)>4096:
        raise ValueError('Expected 1 to 4096 cooked appearance assets')
    information = {p:asset_info(p.read_bytes()) for p in assets}
    mapping = relocation(pack_id,[info['package'] for info in information.values()])
    references=reference_mapping(mapping)
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
    report={'schema':1,'package_id':pack_id,'source_packs':[], 'assets':[], 'import_repairs':repairs,'tools':{
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
            edits=[]
            after,changes=replace_references(data,references,edits) if suffix in ('.uasset','.uexp') else (data,0)
            restored=bytearray(after)
            for offset,original in reversed(edits):restored[offset:offset+len(original)]=original
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
        from css_controls import embed
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


def variant_sources(path:Path) -> list[dict]:
    data=json.loads(path.read_text())
    if not isinstance(data,list) or not 1<=len(data)<=256:raise ValueError('Expected 1 to 256 variant source groups')
    seen=set()
    for item in data:
        id=item['id']
        if not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,63}',id) or id in seen:
            raise ValueError('Variant source IDs must be unique lowercase identifiers')
        seen.add(id)
        if not isinstance(item.get('name'),str) or not 1<=len(item['name'])<=128:raise ValueError('Variant needs a display name')
        if not isinstance(item.get('inputs'),list) or not item['inputs']:raise ValueError('Variant needs source inputs')
        item['inputs']=[(path.parent/p).resolve(strict=True) for p in item['inputs']]
        for field in ('materials','colors','customize','import_repairs'):
            if item.get(field):item[field]=(path.parent/item[field]).resolve(strict=True)
    return data


def share_variant_textures(renamed:Path,reports:list[dict]) -> tuple[list[dict],dict]:
    """Share only byte-identical, single-export textures across source groups."""
    seen={};aliases={};assets=[copy.deepcopy(a) for r in reports for a in r['assets']]
    for asset in assets:
        exports=asset['exports']
        if len(exports)!=1 or exports[0]['class']!='Texture2D' or exports[0]['outer']!=0:continue
        key=tuple((suffix,meta['source_sha256']) for suffix,meta in sorted(asset['files'].items()))
        if key in seen:
            canonical=seen[key]
            if len(asset['css'])==len(canonical):aliases[asset['css']]=canonical
        else:seen[key]=asset['css']
    unique=[];saved=0
    for asset in assets:
        path=renamed/'MortalShell2/Content'/asset['css'].removeprefix('/Game/')
        if asset['css'] in aliases:
            for suffix in asset['files']:
                file=Path(str(path)+suffix);saved+=file.stat().st_size;file.unlink()
            continue
        for suffix,meta in asset['files'].items():
            file=Path(str(path)+suffix)
            if suffix in ('.uasset','.uexp') and aliases:
                before=file.read_bytes();edits=[]
                after,count=replace_references(before,aliases,edits)
                restored=bytearray(after)
                for offset,data in reversed(edits):restored[offset:offset+len(data)]=data
                if restored!=before:raise ValueError('Shared texture reference inverse failed')
                file.write_bytes(after)
                meta['shared_references_changed']=count
            meta['converted_sha256']=digest(file)
        unique.append(asset)
    return unique,{'aliases':aliases,'legacy_bytes_saved':saved}


def convert_grouped(args:argparse.Namespace) -> Path:
    """Combine verified, isolated source groups into one wardrobe entry and trio."""
    from css_controls import verify_resources
    from css_sources import unchanged
    groups=variant_sources(args.variant_sources.resolve(strict=True))
    if not args.name or not args.id:raise ValueError('Grouped conversion requires --name and a stable --id')
    if args.mesh or args.variant or args.materials or args.colors:
        raise ValueError('Define meshes, materials and controls within each variant source group')
    stem=output_name(args.name,args.author,args.name_format)
    output=args.output.resolve()/stem
    if output.exists():raise FileExistsError(output)
    work=args.work.resolve() if args.work else ROOT/'work'/f'convert-{stem}'
    work.mkdir(parents=True,exist_ok=False)
    converter=Converter(args.retoc,args.repak,work)
    watched=[]
    if getattr(args,'source_snapshot',None):
        snapshot=json.loads(args.source_snapshot.read_text())
        for source in snapshot['sources']:
            if source['ready']:
                directory=Path(source['directory'])
                if any(directory.parent in p.parents for group in groups for p in group['inputs']):
                    unchanged(directory,source['snapshot']);watched.append((directory,source['snapshot']))
        if not watched:raise ValueError('No source snapshot covers these variant inputs')
        if any(not any(directory.parent in p.parents for directory,_ in watched)
               for group in groups for p in group['inputs']):
            raise ValueError('An input variant has no stable source snapshot')
    merged=work/'renamed';merged.mkdir()
    metadata=work/'metadata'/PACKAGE_ROOT/args.id;metadata.mkdir(parents=True)
    manifest=None;variants=[];reports=[];resources={}
    for group in groups:
        child=copy.copy(args)
        child.variant_sources=None;child.source_snapshot=None
        child.inputs=group['inputs'];child.id=args.id+'.'+group['id']
        child.name=group['name'];child.work=work/group['id'];child.output=work/'children'/group['id']
        child.mesh=group.get('mesh');child.variant=[]
        child.materials=group.get('materials');child.colors=group.get('customize') or group.get('colors')
        child.include=group.get('include',[]);child.import_repairs=group.get('import_repairs')
        child.shell=group.get('shell',args.shell)
        if child.colors:
            # Source recipes belong to the public wardrobe entry. Child packages
            # are only verification stages, with their own isolated asset IDs.
            recipe=json.loads(child.colors.read_text())
            if recipe['id']!=args.id:raise ValueError('Variant recipe belongs to another outfit')
            recipe['id']=child.id
            recipe_dir=work/(group['id']+'-customize');recipe_dir.mkdir()
            for file in child.colors.parent.glob('dye-*.png'):(recipe_dir/file.name).symlink_to(file.resolve())
            child.colors=recipe_dir/'customize.json';child.colors.write_text(json.dumps(recipe))
        convert(child)
        child_meta=child.work/'metadata'/PACKAGE_ROOT/child.id
        current=json.loads((child_meta/'manifest.json').read_text())
        report=json.loads((child_meta/'conversion.json').read_text());reports.append(report)
        if manifest is None:manifest=copy.deepcopy(current)
        outfit=current['catalog']['outfits'][0]
        if len(outfit['variants'])!=1:raise ValueError('A source group must select one body mesh')
        variant=outfit['variants'][0];variant['id']=group['id'];variant['name']=group['name']
        if 'customize' in outfit:variant['customize']=outfit['customize']
        variants.append(variant)
        for name,info in current.get('resources',{}).items():
            if name in resources and resources[name]!=info:raise ValueError('Variant dye filename collision: '+name)
            if name not in resources:shutil.copy2(child_meta/name,metadata/name)
            resources[name]=info
        for file in (child.work/'renamed').rglob('*'):
            if not file.is_file():continue
            target=merged/file.relative_to(child.work/'renamed')
            if target.exists():
                if digest(target)!=digest(file):raise ValueError('Variant asset or shader collision: '+str(target))
                continue
            target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,target)
    combined_assets,shared=share_variant_textures(merged,reports)
    print(f'Shared {len(shared["aliases"])} identical textures ({shared["legacy_bytes_saved"]//1048576} MiB)',flush=True)
    stage=work/'package';stage.mkdir()
    target=stage/(stem+'.utoc')
    converter.run(converter.retoc,'to-zen',merged,target,'--version','UE5_6')
    converter.run(converter.retoc,'verify',target)
    checks=work/'check-containers';converter.base_containers(args.game.resolve(),checks)
    for suffix in ('.utoc','.ucas'):(checks/(stem+suffix)).symlink_to(target.with_suffix(suffix))
    converter.run(converter.retoc,'to-legacy',checks,work/'check','--version','UE5_6','--no-parallel','--no-shaders','-f','/CSS/')
    for asset in combined_assets:
        path=work/'check/MortalShell2/Content'/asset['css'].removeprefix('/Game/')
        info=asset_info(Path(str(path)+'.uasset').read_bytes())
        if info['package']!=asset['css'] or info['exports']!=asset['exports']:raise ValueError('Combined package export mismatch')
        for suffix,info in asset['files'].items():
            if suffix!='.uasset' and digest(Path(str(path)+suffix))!=info['converted_sha256']:
                raise ValueError('Combined variant payload changed')
    manifest.update(id=args.id,name=args.name,version=args.package_version,resources=resources)
    outfit=manifest['catalog']['outfits'][0]
    outfit.update(id=args.id,name=args.name,description=args.description or '',variants=variants)
    outfit.pop('customize',None)
    manifest['containers']={suffix:{'file':stem+suffix,'bytes':target.with_suffix(suffix).stat().st_size,
                                  'sha256':digest(target.with_suffix(suffix))} for suffix in ('.utoc','.ucas')}
    shutil.copy2(args.thumbnail,metadata/'thumbnail.png')
    verify_resources(manifest,metadata)
    (metadata/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if (metadata/'manifest.json').stat().st_size>256*1024:raise ValueError('Combined manifest exceeds native limit')
    (metadata/'conversion.json').write_text(json.dumps({'schema':1,'runtime_tested':False,
        'verification':'passed: all isolated and combined payload round trips','variants':reports,
        'combined_assets':combined_assets,'shared_textures':shared},indent=2)+'\n')
    target.with_suffix('.pak').unlink()
    converter.run(converter.repak,'pack',work/'metadata',target.with_suffix('.pak'),'--version','V8B')
    converter.run(converter.repak,'unpack',target.with_suffix('.pak'),'--output',work/'metadata-check')
    for file in metadata.iterdir():
        if digest(file)!=digest(work/'metadata-check'/PACKAGE_ROOT/args.id/file.name):raise ValueError('Combined metadata round trip failed')
    for directory,snapshot in watched:unchanged(directory,snapshot)
    output.parent.mkdir(parents=True,exist_ok=True)
    staging=Path(tempfile.mkdtemp(prefix=f'.{stem}-',dir=output.parent))
    try:
        for file in stage.iterdir():shutil.copy2(file,staging/file.name)
        if output.exists():raise FileExistsError(output)
        staging.rename(output)
    finally:
        if staging.exists():shutil.rmtree(staging)
    print(f'Verified {len(variants)} outfit variants: {output}',flush=True)
    return output


def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs',nargs='*',type=Path)
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
    # --colors is what every existing build script passes, so it stays as an alias.
    parser.add_argument('--customize','--colors',dest='colors',type=Path,
                        help='Author control recipe with adjacent dye PNG resources; embedded in the package')
    parser.add_argument('--include',action='append',help='Game material or other dependency to include and relocate with source assets')
    parser.add_argument('--import-repairs',type=Path,help='Audited source-hash-bound import table repair recipe')
    parser.add_argument('--variant-sources',type=Path,help='JSON list of separately packaged outfit variants; keeps overlapping source assets isolated')
    parser.add_argument('--source-snapshot',type=Path,help='Stable incoming-source snapshot from css_sources.py; checked again before publication')
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
