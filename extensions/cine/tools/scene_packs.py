"""Package the CINE scene packs for release: one download per pack, each a pak folder.
Python 3.14, standard library.

  tools/scene_packs.py [PACK ...] [--output DIR]

A pack is a cooked UE mod (CINE_<Pack>_eins0fx_P: .pak, .ucas, .utoc) built and verified by
assets/packs/build_pack.py, which writes work/packs/<Pack>/receipt.json with the trio's hashes.
This tool reads those receipts, checks the files still match, and writes
extensions/core/dist/cine-pack-<slug>-v<version>/MSII-CINE-<Pack>-v<version>.zip. The archive
holds the pack folder as the player drops it into MortalShell2/Content/Paks/~mods/, with a
README and a release record inside it. An archive that already exists is left alone (bump the
pack's version to ship a new build).
"""
import argparse
import hashlib
import json
import re
import subprocess
import time
import zipfile
from pathlib import Path

CINE = Path(__file__).resolve().parent.parent
REPO = CINE.parent.parent
DIST = CINE.parent / 'core' / 'dist'
WORK = CINE / 'work' / 'packs'
REQUIRES = 'CINE 2.0.0'

# What each pack needs, and for the music packs what they are about; names, songs and each
# scene's own description come from the pack itself.
PACKS = {
    'IrisOut': {
        'needs': 'the Eve, Unholy Genessa and Commander White outfits, and CSS Shared Assets for the Iris Out dance',
        'about': 'The Iris Out dance, cut to the song. The four-pass scenes dance it four times: the bones alone, '
                 'then Eve, Unholy Genessa and Commander White, each staged from wireframe to clay to the finished '
                 'look before she dances. The + Rig scenes dance it twice: the outfit, then the bones alone. '
                 'The Static scenes hold the camera you framed yourself; Iris Out 9:16 is laid out for Reels.',
    },
    'SomethingAintRight': {
        'needs': 'the Eve, Unholy Genessa and Commander White outfits',
        'about': 'Showcase cuts on the song. Showreel 124 and Showreel Finale run through Unholy Genessa, '
                 'Commander White and Eve split down the middle: the wireframe on one side, the finished look on '
                 'the other. Commander White Release (and its 9:16 version) shows her palettes, her pieces and her rig.',
    },
    'TheyKnowNotWhatTheyDo': {
        'needs': 'the Profane Proxima outfit',
        'about': "A close 17 second cut for Profane Proxima on the song's turning points, in Eve's default idle: "
                 'the glow colour and the Human / Proxima skin change with the music, and the camera falls from '
                 'high above her to the ground. Body shots only from behind.',
    },
    'MarkOfTheBeast': {'needs': 'the outfits the scenes show, and CSS Shared Assets for the dances'},
    'Walks': {'needs': 'a route and a final shot you record in CINE Mode (Route tab)'},
    'Turntable': {'needs': 'nothing beyond CINE'},
    'PoseGlide': {'needs': 'nothing beyond CINE'},
    'PhotoOrbit': {'needs': 'nothing beyond CINE'},
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO, text=True).strip()


def slug(token):
    return re.sub(r'(?<!^)(?=[A-Z])', '-', token).lower()


def receipt(token):
    path = WORK / token / 'receipt.json'
    if not path.is_file():
        raise FileNotFoundError(f'{path} is missing; build the pack with assets/packs/build_pack.py')
    r = json.loads(path.read_text())
    if r.get('status') != 'passed':
        raise ValueError(f'{token}: the last build did not pass ({r.get("status")})')
    return r


def rows(r):
    """The pack's table rows as {name: JSON}, from the rows the build cooked."""
    return {row['Name']: json.loads(row['DevComment']) for row in json.loads(Path(r['canonical_rows']['path']).read_bytes())}


def readme(token, table, meta):
    m = table['manifest']
    title = f"CINE {m['name']} {m['version']}"
    lines = [title, '=' * len(title), '']
    lines += [f"Music scenes for CINE on {m['song']}." if m.get('song') else 'Scenes for CINE.', '']
    if meta.get('about'):
        lines += [meta['about'], '']
    lines += ['Scenes']
    for scene in m['presets']:
        p = table[f'preset.{scene}']
        # The music lives in this pack now: drop the old note about a track file in the CINE folder.
        about = re.sub(r'\s*Track: \S+ in the CINE folder\.', '', p.get('description', '')).strip()
        lines += [f"- {p.get('name', scene)}: {about}" if about else f"- {p.get('name', scene)}"]
    lines += ['']
    lines += [f"Requirements: {REQUIRES} or newer (CSSX 1.3.2 or newer), Custom Shell System 1.0.5 or newer for the "
              f"look changes, and {meta['needs']}.", '']
    lines += ['Install',
              '1. Install CINE 2.0.0 first.',
              f'2. Close the game. Put the CINE_{token}_eins0fx_P folder into MortalShell2/Content/Paks/~mods/.',
              '3. Start the game, pause, System > Activate CINE Mode, and pick a scene on the Take tab.', '',
              f'Remove: delete the CINE_{token}_eins0fx_P folder.', '']
    return '\n'.join(lines).encode()


def build(token, output):
    meta = PACKS[token]
    r = receipt(token)
    table = rows(r)
    m = table['manifest']
    container = r['container']
    folder = output / f"cine-pack-{slug(token)}-v{m['version']}"
    archive = folder / f"MSII-CINE-{token}-v{m['version']}.zip"
    if archive.exists():
        print(f'{archive} exists; left unchanged (bump the pack version to ship a new build)')
        return archive
    files = {}
    for item in r['trio']:
        data = Path(item['path']).read_bytes()
        if len(data) != item['bytes'] or digest(data) != item['sha256']:
            raise ValueError(f'{item["path"]} no longer matches its build receipt; rebuild the pack')
        files[f'{container}/{Path(item["path"]).name}'] = data
    files[f'{container}/README.txt'] = readme(token, table, meta)
    commit = git('rev-parse', 'HEAD')
    record = {'product': f"CINE {m['name']}", 'version': m['version'], 'song': m.get('song', ''),
              'scenes': m['presets'], 'requires': REQUIRES, 'source_commit': commit,
              'files': {name: digest(data) for name, data in sorted(files.items())}}
    files[f'{container}/release.json'] = (json.dumps(record, indent=2) + '\n').encode()
    stamp = time.gmtime(max(315532800, int(git('show', '-s', '--format=%ct', commit))))[:6]
    folder.mkdir(parents=True, exist_ok=True)
    # The cooked trio is already compressed; storing it keeps the archive quick to open.
    with zipfile.ZipFile(archive, 'x') as z:
        for name in sorted(files):
            info = zipfile.ZipInfo(name, stamp)
            info.compress_type = zipfile.ZIP_STORED if name.endswith(('.ucas', '.utoc', '.pak')) else zipfile.ZIP_DEFLATED
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
    ap.add_argument('packs', nargs='*', help='pack tokens to build (default: all)')
    ap.add_argument('--output', type=Path, default=DIST)
    opts = ap.parse_args()
    for token in opts.packs or PACKS:
        if token not in PACKS:
            raise SystemExit(f'Unknown pack {token}; packs: {", ".join(PACKS)}')
        build(token, opts.output)


if __name__ == '__main__':
    main()
