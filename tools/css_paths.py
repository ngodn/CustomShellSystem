"""Portable paths for new CSS authoring outputs. Python 3.14.

These gates apply to new candidates, not read-only verification of old releases.
"""
from contextlib import contextmanager
from pathlib import Path, PureWindowsPath
import re
import shutil

MAX_PATH = 240  # Includes headroom below classic Windows MAX_PATH (260).
MAX_PACKAGE = 96
HASH = re.compile(r'[0-9a-fA-F]{16,}|[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}')
RESERVED = re.compile(r'(?i:con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\..*)?\Z')


def component(name: str) -> None:
    if (not re.fullmatch(r'[A-Za-z0-9_.-]{1,48}', name)
            or name in ('.', '..') or name.endswith('.')
            or HASH.search(name) or RESERVED.fullmatch(name)):
        raise ValueError(f'Use a short readable portable name: {name}')


def package_path(package: str) -> None:
    if not package.startswith('/Game/CSS/') or len(package) > MAX_PACKAGE:
        raise ValueError(f'Expected a short /Game/CSS/ asset package: {package}')
    for name in package.removeprefix('/Game/').split('/'):
        component(name)
        if '.' in name or '-' in name:
            raise ValueError(f'Use letters, digits and underscores in asset packages: {package}')


def output_path(path: Path) -> None:
    full = str(path.absolute())
    if len(full.encode('utf-16-le')) // 2 > MAX_PATH:
        raise ValueError(f'Output path exceeds {MAX_PATH} Windows characters: {full}')


def windows_destination(root: str, relative: str) -> str:
    base = PureWindowsPath(root)
    child = PureWindowsPath(relative)
    if not base.is_absolute() or child.is_absolute() or child.drive or '..' in child.parts:
        raise ValueError('Provide an absolute Windows destination root and a safe relative path')
    for name in child.parts:
        component(name)
    full = str(base / child)
    if len(full.encode('utf-16-le')) // 2 > MAX_PATH:
        raise ValueError(f'Windows destination exceeds {MAX_PATH} characters: {full}')
    return full


def new_directory(parent: Path, label: str) -> Path:
    """Reserve a readable sequence atomically, never reuse an existing directory."""
    component(label)
    output_path(parent)
    parent.mkdir(parents=True, exist_ok=True)
    for number in range(1, 10000):
        result = parent / f'{label}-{number:04d}'
        component(result.name)
        output_path(result)
        try:
            result.mkdir()
        except FileExistsError:
            continue
        return result
    raise FileExistsError(f'No unused {label} directory remains in {parent}')


@contextmanager
def temporary_directory(parent: Path, label: str):
    result = new_directory(parent, label)
    try:
        yield result
    finally:
        shutil.rmtree(result)
