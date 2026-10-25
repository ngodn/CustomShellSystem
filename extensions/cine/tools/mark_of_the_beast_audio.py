"""Cut the Mark of the Beast song into one track per take, for the CINE folder. Python 3.14 + ffmpeg.

  tools/mark_of_the_beast_audio.py SONG CINE_FOLDER

Writes CINE_FOLDER/audio/beast-<take>.wav, 48 kHz stereo, one per take in
tools/mark_of_the_beast_film.py (TAKES: id, song start, song end): each take's preset plays its
slice from the roll, so the take's first frame is that song second. A slice ends with a short
fade so the last take does not click. Local only: the song is the user's own copy and is never
committed.
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mark_of_the_beast_film import TAKES, SONG_END   # noqa: E402

FADE = 0.03


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    song, folder = sys.argv[1], Path(sys.argv[2]) / 'audio'
    folder.mkdir(parents=True, exist_ok=True)
    for take in TAKES:
        start, end = take['song'][0], min(take['song'][1], SONG_END)
        out = folder / f"{take['id']}.wav"
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-ss', f'{start:.3f}', '-t', f'{end - start:.3f}', '-i', song,
                        '-vn', '-ac', '2', '-ar', '48000', '-c:a', 'pcm_s16le',
                        '-af', f'afade=t=out:st={end - start - FADE:.3f}:d={FADE}', str(out)], check=True)
        print(out, f'{start:.2f} to {end:.2f} s')


if __name__ == '__main__':
    main()
