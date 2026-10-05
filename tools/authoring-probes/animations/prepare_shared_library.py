"""Prepare a private shared-library cook plan from the audited animation batches."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def read(path):
    return json.loads(path.read_text())


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def prepare(work, output):
    if output.exists():
        raise FileExistsError(output)
    entries, copies, receipts = [], [], {}
    skeleton = '/Game/CSS/Shared/SKEL_Base.SKEL_Base'

    def add(identity, name, category, playback, source, leaf, slot=None, aliases=None):
        destination = f'/Game/CSS/Locomotion/Eve/{category}/AN_{leaf}'
        entry = dict(id=identity, name=name, category=category, playback=playback,
                     hide_weapons=True, assets=[dict(asset=destination+'.AN_'+leaf, skeleton=skeleton)])
        if slot:
            entry['slot'] = slot
        if aliases:
            entry['aliases'] = aliases
        entries.append(entry)
        copies.append(dict(id=identity, source=source.split('.')[0], destination=destination))

    manifest_path = work/'outfit-compatibility/CSS_EveStellarBlade_eins0fx_P/manifest.json'
    manifest = read(manifest_path)
    receipts[str(manifest_path)] = digest(manifest_path)
    variants = manifest['catalog']['outfits'][0]['variants']
    idles = variants[0]['animations']['idle']
    if len(idles) != 8 or any(v['animations']['idle'] != idles for v in variants):
        raise ValueError('Expected the eight identical released Eve idle definitions')
    for idle in idles:
        slug = 'default' if idle['id'] == 'eve' else idle['name'].removeprefix('CSS ').lower().replace(' ', '_')
        leaf = 'Default' if slug == 'default' else ''.join(word.title() for word in slug.split('_'))
        add('css.eve.idle.'+slug, idle['name'], 'idle', 'loop', idle['clip'], leaf, 'idle',
            [dict(slot='idle', id=idle['id'], asset=idle['clip'])])

    names = {'S01A000': ('BassDownLow', 'Bass Down Low', 'dance'),
             'S02A000': ('BlackHole', 'Black Hole', 'dance'),
             'S03A000': ('EveCatwalk', 'Eve Catwalk', 'catwalk'),
             'S04A000': ('ComeCloser', 'Come Closer', 'dance'),
             'S07A000': ('JangWonYoungSpin', 'Jang Won Young Spin', 'dance'),
             'Adam': ('AdamCatwalk', 'Adam Catwalk', 'catwalk'),
             'Lily': ('LilyCatwalk', 'Lily Catwalk', 'catwalk'),
             'Example200': ('Example200', 'ATOOL Gesture 200', 'performance'),
             'Example801': ('Example801', 'ATOOL Performance 801', 'performance'),
             'CameraBody': ('AfterBruteBattle', 'After Brute Battle', 'performance')}
    for batch_name in ('retarget-l2', 'retarget-l3a', 'retarget-l3l', 'retarget-l4s', 'retarget-l4d', 'retarget-l4c'):
        batch = work/batch_name
        for name in ('retarget-result.json', 'readback-result.json', 'sampling-result.json'):
            path = batch/name
            if not read(path)['passed']:
                raise ValueError(f'Unverified batch: {path}')
            receipts[str(path)] = digest(path)
        config = read(batch/'batch-config.json')
        for label, filename in read(batch/'batch.json'):
            source_data = read(batch/(filename+'.json'))
            if label.startswith('Photo'):
                leaf = source_data['source'].rsplit('/', 1)[-1].split('.')[0]
                name = re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', leaf)
                name = {'Slav Squart': 'Slav Squat', 'OMG': 'Oh My'}.get(name, name)
                category, playback = 'photo', 'pose'
            elif label.startswith('S11A'):
                leaf = 'ATOOL'+str(int(label[4:])+1)
                name, category, playback = 'ATOOL Performance '+str(int(label[4:])+1), 'performance', 'once'
            else:
                leaf, name, category = names[label]
                playback = 'once'
            identity = 'css.eve.'+category+'.'+leaf.lower()
            source = f'/Game/CSS/AnimLab/RT_{config["revision"]}_{label}'
            add(identity, name, category, playback, source, leaf)
            copies[-1].update(expected_keys=source_data['frames'], expected_hz=source_data['fps'],
                              expected_duration=source_data['duration'], original_source=source_data['source'])
    if len(entries) != 57 or len({e['id'] for e in entries}) != 57 or len({c['destination'] for c in copies}) != 57:
        raise ValueError('Unexpected or duplicate animation coverage')
    library = dict(schema=1, animation_library=dict(id='css.locomotion', name='CSS Locomotion Shared Assets',
                   author='eins0fx', version='0.1.0-dev', entries=entries))
    output.mkdir(parents=True)
    (output/'catalog.json').write_text(json.dumps(library, indent=2)+'\n')
    (output/'plan.json').write_text(json.dumps(dict(copies=copies, receipts=receipts,
        pending=['DumbDumb: seven unknown source bone mappings', 'Iris Out: seven unknown source bone mappings'],
        scope='Private cook candidate. Contact, playback, facial integration and game acceptance are incomplete.'), indent=2)+'\n')
    (output/'cook.txt').write_text(''.join(c['destination']+'\n' for c in copies))
    print(f'{len(copies)} sequences prepared in {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('work', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.work.resolve(strict=True), args.output.resolve())
