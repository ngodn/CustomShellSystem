"""Build the CINE scene packs: one download per song, each with its scene presets and music.
Python 3.14, standard library.

  tools/scene_packs.py [SLUG ...] [--output DIR]

A pack unzips into the player's ue4ss/Mods/CSSX/extensions/ folder and lands inside
eins0fx.cine: presets/ (CINE lists every preset in that folder), audio/ (the music each scene
plays from the roll) and scenes/ (the pack's README and its release record with file hashes).
The music is read from extensions/cine/audio/, which stays out of git. Output goes to
extensions/core/dist/cine-scene-<slug>-v<version>/ beside the CINE releases; an archive that
already exists is left alone (bump the pack's version to ship a new build).
"""
import argparse
import hashlib
import json
import subprocess
import time
import zipfile
from pathlib import Path

CINE = Path(__file__).resolve().parent.parent
REPO = CINE.parent.parent
DIST = CINE.parent / 'core' / 'dist'
REQUIRES = 'CINE 2.0.0'

PACKS = {
    'iris-out': {
        'title': 'Iris Out',
        'version': '1.0.0',
        'song': 'IRIS OUT',
        'presets': ['iris-out', 'iris-out-2', 'iris-out-9x16', 'iris-out-static', 'iris-out-static-1',
                    'iris-out-eve-rig', 'iris-out-unholy-rig', 'iris-out-white-rig'],
        'audio': ['iris-out-x4.wav', 'iris-out-2-x4.wav', 'iris-out-look-rig.wav'],
        'needs': 'the Eve, Unholy Genessa and Commander White outfits, and CSS Shared Assets for the Iris Out dance',
        'about': 'The Iris Out dance, cut to the song. The four-pass scenes dance it four times: the bones alone, '
                 'then Eve, Unholy Genessa and Commander White, each staged from wireframe to clay to the finished '
                 'look before she dances. The + Rig scenes dance it twice: the outfit, then the bones alone. '
                 'The Static scenes hold the camera you framed yourself; Iris Out 9:16 is laid out for Reels.',
    },
    'something-aint-right': {
        'title': "Something Ain't Right",
        'version': '1.0.0',
        'song': "XG - SOMETHING AIN'T RIGHT",
        'presets': ['showreel-124', 'showreel-finale', 'commander-white-release', 'commander-white-release-9x16'],
        'audio': ['showreel.wav', 'showreel-finale.wav'],
        'needs': 'the Eve, Unholy Genessa and Commander White outfits',
        'about': 'Showcase cuts on the song. Showreel 124 and Showreel Finale run through Unholy Genessa, '
                 'Commander White and Eve split down the middle: the wireframe on one side, the finished look on '
                 'the other. Commander White Release (and its 9:16 version) shows her palettes, her pieces and her rig.',
    },
    'they-know-not-what-they-do': {
        'title': 'They Know Not What They Do',
        'version': '1.0.0',
        'song': 'Polyphia - THEY KNOW NOT WHAT THEY DO',
        'presets': ['profane-proxima'],
        'audio': ['profane-proxima.wav'],
        'needs': 'the Profane Proxima outfit',
        'about': 'A close 17 second cut for Profane Proxima on the song\'s turning points, in Eve\'s default idle: '
                 'the glow colour and the Human / Proxima skin change with the music, and the camera falls from '
                 'high above her to the ground. Body shots only from behind.',
    },
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO, text=True).strip()


def readme(slug, pack):
    names = []
    for name in pack['presets']:
        names.append(json.loads((CINE / 'presets' / f'{name}.cine.json').read_text())['name'])
    lines = [
        f"CINE Scenes: {pack['title']} {pack['version']}",
        '=' * len(f"CINE Scenes: {pack['title']} {pack['version']}"),
        '',
        f"Music scenes for CINE on {pack['song']}.",
        '',
        pack['about'],
        '',
        'Scenes: ' + ', '.join(names) + '.',
        '',
        f"Requirements: {REQUIRES} or newer (CSSX 1.3.1), Custom Shell System 1.0.5 or newer, and {pack['needs']}.",
        '',
        'Install',
        '1. Install CINE 2.0.0 first.',
        '2. Close the game. Extract this archive into ue4ss/Mods/CSSX/extensions/ so the files',
        '   land inside the eins0fx.cine folder (presets/, audio/ and scenes/).',
        '3. In CINE, Shot > Reload presets (or start the game) and pick a scene.',
        '',
        'For each take: leave Expression on "As set in CSS" (the default), set Breakdown track to "From the preset",',
        'enter Cine World, start your recorder and press F8. The music plays from the roll and your',
        'own look comes back when the scene ends.',
        '',
        'Remove: delete the files listed in scenes/' + slug + '.json from the eins0fx.cine folder.',
        '',
    ]
    return '\n'.join(lines).encode()


def build(slug, output):
    pack = PACKS[slug]
    folder = output / f"cine-scene-{slug}-v{pack['version']}"
    archive = folder / f"CINE-Scenes-{pack['title'].replace(' ', '-').replace(chr(39), '')}-v{pack['version']}.zip"
    if archive.exists():
        print(f'{archive} exists; left unchanged (bump the version to ship a new build)')
        return archive
    base = 'eins0fx.cine/'
    files = {}
    for name in pack['presets']:
        path = CINE / 'presets' / f'{name}.cine.json'
        preset = json.loads(path.read_text())
        audio = preset.get('audio', '')
        if audio and audio.removeprefix('audio/') not in pack['audio']:
            raise ValueError(f'{name} plays {audio}, which the pack does not carry')
        files[base + f'presets/{name}.cine.json'] = path.read_bytes()
    for name in pack['audio']:
        files[base + f'audio/{name}'] = (CINE / 'audio' / name).read_bytes()
    files[base + f'scenes/{slug}.txt'] = readme(slug, pack)
    commit = git('rev-parse', 'HEAD')
    record = {
        'product': f"CINE Scenes: {pack['title']}", 'version': pack['version'], 'song': pack['song'],
        'requires': REQUIRES, 'source_commit': commit,
        'files': {name: digest(data) for name, data in sorted(files.items())},
    }
    files[base + f'scenes/{slug}.json'] = (json.dumps(record, indent=2) + '\n').encode()
    stamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', commit))))[:6]
    folder.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in sorted(files):
            info = zipfile.ZipInfo(name, stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, files[name])
    with zipfile.ZipFile(archive) as z:
        bad = z.testzip()
        if bad:
            raise ValueError(f'{archive} is damaged at {bad}')
    (folder / (archive.name + '.sha256')).write_text(f'{digest(archive.read_bytes())}  {archive.name}\n')
    print(archive, f'{archive.stat().st_size:,} bytes', len(files), 'files')
    return archive


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('slugs', nargs='*', help='packs to build (default: all)')
    ap.add_argument('--output', type=Path, default=DIST)
    opts = ap.parse_args()
    for slug in opts.slugs or PACKS:
        if slug not in PACKS:
            raise SystemExit(f'Unknown pack {slug}; packs: {", ".join(PACKS)}')
        build(slug, opts.output)


if __name__ == '__main__':
    main()
