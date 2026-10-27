"""Check Lane E's protected surfaces against its recorded starting commit."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

HERE = Path(__file__).resolve().parent
CINE = HERE.parents[2]
REPO = CINE.parents[1]
base = (HERE / 'base-commit.txt').read_text().strip()

def original(path):
    return subprocess.check_output(['git', 'show', f'{base}:{path}'], cwd=REPO)

def function(text, name):
    match = re.search(r'^[^\n]*Extension::' + name + r'\([^\n]*', text, re.M)
    if match is None:
        raise ValueError(name)
    begin = text.index('{', match.start())
    depth, end = 1, begin + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]

before = original('extensions/cine/src/cine.cpp').decode()
after = (CINE / 'src/cine.cpp').read_text()
functions = {name: function(before, name) == function(after, name) for name in
             ('start_render', 'release_render', 'render_frame_tick', 'render_finished', 'render_line', 'step_back_in')}
files = {}
for name in ('render.cpp', 'render.hpp'):
    data = (CINE / 'src' / name).read_bytes()
    files[name] = {'unchanged': data == original('extensions/cine/src/' + name), 'sha256': hashlib.sha256(data).hexdigest()}
photo_lines = lambda text: [line.strip() for line in text.splitlines() if 'photo_parked_' in line or 'photo.step_' in line]
photo_unchanged = photo_lines(before) == photo_lines(after)
def render_controls(value):
    found = []
    if isinstance(value, dict):
        if str(value.get('id', '')).startswith('render'):
            found.append(value)
        else:
            for child in value.values(): found.extend(render_controls(child))
    elif isinstance(value, list):
        for child in value: found.extend(render_controls(child))
    return found
render_menu = render_controls(json.loads(original('extensions/cine/menu.json'))) == render_controls(json.loads((CINE / 'menu.json').read_text()))
core_changes = subprocess.check_output(['git', 'diff', base, '--name-only', '--', 'extensions/core/src/core'], cwd=REPO).decode().splitlines()
result = {'base': base, 'functions': functions, 'files': files, 'photo_lines_unchanged': photo_unchanged,
          'render_menu_unchanged': render_menu, 'core_source_changes': core_changes}
(HERE / 'scope-check.json').write_text(json.dumps(result, indent=2) + '\n')
if not all(functions.values()) or not all(value['unchanged'] for value in files.values()) or not photo_unchanged or not render_menu or core_changes:
    raise SystemExit('Protected scope changed')
print('Protected renderer methods, render sources and Photo Mode step lines unchanged')
