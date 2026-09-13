#!/usr/bin/env python3
"""Add author dye recipes to verified CSS packages without changing cooked assets."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from css_convert import DEFAULT_REPAK, PACKAGE_ROOT, digest
from css_colors import embed
from css_package import verify


def build(package:Path,recipe:Path,output:Path,repak:Path=DEFAULT_REPAK):
    manifest=verify(package,repak)
    output=output/package.name
    if output.exists():raise FileExistsError(output)
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.css-colors-',dir=output.parent) as temp:
        root=Path(temp);stage=root/'package';stage.mkdir()
        pak=next(package.glob('*.pak'))
        subprocess.run([str(repak),'unpack',str(pak),'--output',str(root/'metadata')],check=True,stdout=subprocess.DEVNULL)
        metadata=root/'metadata'/PACKAGE_ROOT/manifest['id']
        for name in manifest.get('resources',{}): (metadata/name).unlink()
        embed(recipe,manifest,metadata)
        manifest['version']='1.1.0'
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
    a=p.parse_args();build(a.package,a.recipe,a.output,a.repak)
