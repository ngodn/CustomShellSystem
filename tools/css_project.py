#!/usr/bin/env python3
"""Create, check and build repeatable CSS outfit projects. Python 3.14."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess
import time

import css_convert as converter
from css_colors import resource_info, validate
from css_package import release_zip, verify

FIELDS = {
    'format', 'format_version', 'id', 'name', 'author', 'version', 'description',
    'source_url', 'thumbnail', 'thumbnail_source', 'name_format', 'inputs',
    'mesh', 'variants', 'shells', 'materials', 'colors', 'include',
    'import_repairs', 'variant_sources',
}


def text(value, name, limit=256):
    if not isinstance(value, str) or not value.strip() or len(value.encode()) > limit:
        raise ValueError(f'{name} must be nonempty text, at most {limit} UTF-8 bytes')
    return value


def strings(value, name):
    if not isinstance(value, list) or any(not isinstance(x, str) or not x.strip() for x in value):
        raise ValueError(f'{name} must be a list of nonempty strings')
    return value


def color_recipe(path: Path, identity: str):
    data = json.loads(path.read_text())
    if data.get('id') != identity:
        raise ValueError(f'Color recipe ID must be {identity}: {path}')
    for name in validate(data['colors']):
        resource_info(path.parent / name)


def read_project(path: Path) -> argparse.Namespace:
    """Validate author inputs without claiming cooked-asset or game compatibility."""
    path = path.resolve(strict=True)
    data = json.loads(path.read_text())
    if not isinstance(data, dict) or set(data) - FIELDS:
        raise ValueError('Unknown project field; see docs/modding/project-format.md')
    if data.get('format') != 'CSS.Project' or data.get('format_version') != 1:
        raise ValueError('Expected CSS.Project format_version 1')
    identity = text(data.get('id'), 'id', 96)
    if not re.fullmatch(r'[a-z0-9][a-z0-9._-]{0,95}', identity) or '..' in identity:
        raise ValueError('id must be a stable lowercase CSS identifier without ..')
    name, author = text(data.get('name'), 'name'), text(data.get('author'), 'author')
    version = text(data.get('version'), 'version', 64)
    pattern = text(data.get('name_format', converter.DEFAULT_FORMAT), 'name_format')
    converter.output_name(name, author, pattern)

    def file(key):
        value = data.get(key)
        return (path.parent / text(value, key, 4096)).resolve(strict=True) if value is not None else None

    thumbnail = file('thumbnail')
    if thumbnail is None:
        raise ValueError('thumbnail is required')
    converter.png_info(thumbnail)
    inputs = [(path.parent / p).resolve(strict=True) for p in strings(data.get('inputs', []), 'inputs')]
    grouped = file('variant_sources')
    variants = strings(data.get('variants', []), 'variants')
    shells = strings(data.get('shells', []), 'shells')
    if any(not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,95}', s) for s in shells):
        raise ValueError('Invalid shell tag')
    materials, colors, repairs = file('materials'), file('colors'), file('import_repairs')
    mesh = data.get('mesh')
    if mesh is not None:
        text(mesh, 'mesh', 1024)
    includes = strings(data.get('include', []), 'include')
    if grouped:
        if inputs or mesh or variants or materials or colors or repairs or includes:
            raise ValueError('variant_sources owns inputs, mesh, variants, materials, colors, include and import_repairs')
        for group in converter.variant_sources(grouped):
            converter.discover(group['inputs'])
            if group.get('colors'):
                color_recipe(group['colors'], identity)
    else:
        if not inputs:
            raise ValueError('Provide inputs or variant_sources')
        converter.discover(inputs)
        if mesh and variants:
            raise ValueError('Use mesh or variants, not both')
        if colors:
            color_recipe(colors, identity)
    description = data.get('description', '')
    if not isinstance(description, str) or len(description.encode()) > 4096:
        raise ValueError('description must be text, at most 4096 UTF-8 bytes')
    for key in ('source_url', 'thumbnail_source'):
        if key in data:
            text(data[key], key, 4096)
    return argparse.Namespace(
        inputs=inputs, name=name, author=author, id=identity,
        package_version=version, description=description,
        source_url=data.get('source_url'), thumbnail=thumbnail,
        thumbnail_source=data.get('thumbnail_source', 'author-provided'),
        name_format=pattern, mesh=mesh, variant=variants, shell=shells,
        materials=materials, colors=colors, include=includes, import_repairs=repairs,
        variant_sources=grouped, source_snapshot=None,
    )


def initialize(directory: Path, identity: str, name: str, author: str):
    if not re.fullmatch(r'[a-z0-9][a-z0-9._-]{0,95}', identity) or '..' in identity:
        raise ValueError('Choose a stable lowercase id, for example author.outfit')
    converter.output_name(text(name, 'name'), text(author, 'author'))
    directory.mkdir(parents=True, exist_ok=False)
    (directory / 'source').mkdir()
    data = {
        'format': 'CSS.Project', 'format_version': 1,
        'id': identity, 'name': name, 'author': author, 'version': '1.0.0',
        'description': '', 'thumbnail': 'thumbnail.png',
        'thumbnail_source': 'author-provided', 'inputs': ['source'],
    }
    (directory / 'css-project.json').write_text(json.dumps(data, indent=2) + '\n')
    (directory / '.gitignore').write_text('/source/\n/work/\n/dist/\n')
    (directory / 'README.md').write_text(
        '# Outfit project\n\n'
        'Put finished Mortal Shell II cooked appearance containers in `source/`.\n'
        'Add your square PNG portrait as `thumbnail.png` (128 to 1024 pixels).\n'
        'Edit `css-project.json`, then run the CSS project check and build commands.\n'
        'This starter contains no model, thumbnail, material or skeleton placeholders.\n'
        'See the CSS repository docs/modding/README.md for authoring and release steps.\n'
    )
    return directory / 'css-project.json'


def build(path: Path, game: Path, retoc: Path, repak: Path, output: Path | None = None,
          work: Path | None = None, source_snapshot: Path | None = None) -> tuple[Path, Path]:
    args = read_project(path)
    root = path.resolve().parent
    args.game, args.retoc, args.repak = game.resolve(strict=True), retoc.resolve(strict=True), repak.resolve(strict=True)
    args.output = output.resolve() if output else root / 'dist'
    args.work = work.resolve() if work else root / 'work' / str(time.time_ns())
    args.source_snapshot = source_snapshot.resolve(strict=True) if source_snapshot else None
    if args.source_snapshot and not args.variant_sources:
        raise ValueError('Source snapshots currently require variant_sources')
    stem = converter.output_name(args.name, args.author, args.name_format)
    archive = args.output / f'{stem}.zip'
    if archive.exists():
        raise FileExistsError(archive)
    result = converter.convert(args)
    verify(result, args.repak)
    release_zip(result, archive, args.repak)
    return result, archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='action', required=True)
    init = commands.add_parser('init', help='Create an empty author project')
    init.add_argument('directory', type=Path)
    for name in ('id', 'name', 'author'):
        init.add_argument('--' + name, required=True)
    check = commands.add_parser('check', help='Check recipe, sources and images, without cooking or converting')
    check.add_argument('project', type=Path)
    make = commands.add_parser('build', help='Convert, verify, then produce the outfit trio and install ZIP')
    make.add_argument('project', type=Path)
    for name in ('game', 'retoc', 'repak'):
        make.add_argument('--' + name, type=Path, required=True)
    make.add_argument('--output', type=Path)
    make.add_argument('--work', type=Path)
    make.add_argument('--source-snapshot', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'init':
            print(initialize(args.directory, args.id, args.name, args.author))
        elif args.action == 'check':
            result = read_project(args.project)
            print(f'Project inputs checked: {result.name}. Cooked assets and gameplay are not validated by this check.')
        else:
            package, archive = build(args.project, args.game, args.retoc, args.repak, args.output, args.work, args.source_snapshot)
            print(f'Package: {package}\nRelease ZIP: {archive}\nTest in the game before publishing.')
    except (ValueError, OSError, RuntimeError, KeyError, TypeError, struct.error, subprocess.CalledProcessError) as error:
        parser.exit(1, f'CSS project {args.action} failed: {error}\n')


if __name__ == '__main__':
    main()
