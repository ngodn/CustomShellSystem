#!/usr/bin/env python3
"""Add author dye recipes to verified CSS packages without changing cooked assets."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_controls import embed
from css_package import verify


def build(package:Path,recipe:Path,output:Path,repak:Path=DEFAULT_REPAK,
          package_version:str|None=None,replace_variant_colors:bool=False):
    manifest=verify(package,repak)
    if package_version is not None and (not isinstance(package_version,str) or not package_version.strip() or len(package_version.encode())>64):
        raise ValueError('Package version must be nonempty text, at most 64 UTF-8 bytes')
    if any('customize' in v or 'colors' in v for v in manifest['catalog']['outfits'][0]['variants']) and not replace_variant_colors:
        raise ValueError('Package has variant-specific colors. Rebuild from its project, or explicitly use --replace-variant-colors for one shared recipe.')
    output=output/package.name
    if output.exists():raise FileExistsError(output)
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.css-colors-',dir=output.parent) as temp:
        root=Path(temp);stage=root/'package';stage.mkdir()
        pak=next(package.glob('*.pak'))
        subprocess.run([str(repak),'unpack',str(pak),'--output',str(root/'metadata')],check=True,stdout=subprocess.DEVNULL)
        metadata=root/'metadata'/PACKAGE_ROOT/manifest['id']
        for name in manifest.get('resources',{}): (metadata/name).unlink()
        manifest['resources']={}
        for variant in manifest['catalog']['outfits'][0]['variants']:
            variant.pop('customize',None);variant.pop('colors',None)
        embed(recipe,manifest,metadata)
        if package_version is not None:manifest['version']=package_version
        (metadata/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        audit=metadata/'conversion.json'
        report=json.loads(audit.read_text())
        report['color_recipe_sha256']=digest(recipe)
        report['color_resources']=manifest['resources']
        report['runtime_tested']=False
        audit.write_text(json.dumps(report,indent=2)+'\n')
        for suffix in ('.utoc','.ucas'):shutil.copy2(pak.with_suffix(suffix),stage/pak.with_suffix(suffix).name)
        subprocess.run([str(repak),'pack',str(root/'metadata'),str(stage/pak.name),'--version','V8B'],check=True,stdout=subprocess.DEVNULL)
        verify(stage,repak)
        for suffix in ('.utoc','.ucas'):
            if digest(stage/pak.with_suffix(suffix).name)!=digest(pak.with_suffix(suffix)):raise ValueError('Cooked container changed while adding colors')
        stage.rename(output)
    print(output)
    return output

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('package',type=Path);p.add_argument('--recipe',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--repak',type=Path,default=DEFAULT_REPAK)
    p.add_argument('--package-version',help='New outfit version; omitted preserves the existing version')
    p.add_argument('--replace-variant-colors',action='store_true',help='Explicitly replace every variant recipe with this shared outfit recipe')
    a=p.parse_args();build(a.package,a.recipe,a.output,a.repak,a.package_version,a.replace_variant_colors)
