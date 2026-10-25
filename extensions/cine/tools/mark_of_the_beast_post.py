"""Assemble the Mark of the Beast film from its takes: the intercut, the narration over the
spoken intro, the title cards, the flashes on the skin swaps and the end card.
Python 3.12 with numpy and pillow (mise x uv@0.12.5 -- uv run --python 3.12 --with numpy --with pillow).

  mark_of_the_beast_post.py --takes DIR --slices DIR --song SONG --font FONT.ttf --out OUT.mp4
                            [--from SECONDS] [--to SECONDS] [--list]

TAKES holds the recordings (record.py), named <take id>-<date>.mp4; the newest of each id is
used. SLICES is the CINE audio folder with the take tracks (mark_of_the_beast_audio.py): each
recording is lined up with its own track by correlation, so the picture sits on the song to
the frame. SONG is the whole song. --from/--to render a part (a preview); --list prints the
edit and which takes are present. The edit (song time -> take) is CUTS below.

Timings are song seconds (docs/BEAST-GOAL.md): the speech 0.5 to 26.9 (narrated word by
word; the only words in the song), the bell at 29.5 (the title), the accents at 41.2 and 49.5 (the two names), the groove at
54, the skin swaps in p2, p3 and p4 (a flash each), the end card from 262.
"""
import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mark_of_the_beast_film import TAKES, SONG_END, SEATED_AT   # noqa: E402

FPS = 60
END = SONG_END
INK = (206, 168, 160)        # dusty rose, as the Profane Proxima card
GLOW = (150, 12, 6)
FAINT = (170, 150, 146)

# The edit: from this song second, this take, until the next entry.
CUTS = [
    (0.0, 'beast-p0'), (3.88, 'beast-g0'), (5.82, 'beast-p0'), (8.32, 'beast-g0'), (10.80, 'beast-p0'),
    (13.92, 'beast-g0'), (24.32, 'beast-p0'), (29.5, 'beast-g0'), (36.0, 'beast-p0'), (41.2, 'beast-p0'), (49.5, 'beast-g0'),
    (54.0, 'beast-p1'), (95.9, 'beast-g1'), (150.8, 'beast-p2'), (177.7, 'beast-g2'),
    (209.7, 'beast-g3'), (214.0, 'beast-p3'), (217.5, 'beast-g3'), (221.0, 'beast-p3'), (224.4, 'beast-g3'), (228.0, 'beast-p3'),
    (231.6, 'beast-p3'), (236.0, 'beast-g3'), (238.6, 'beast-g3'), (240.0, 'beast-p3'), (242.0, 'beast-g3'), (243.6, 'beast-p3'),
    (245.8, 'beast-p3'), (246.6, 'beast-p4'), (250.0, 'beast-g4'), (253.6, 'beast-p4'), (SEATED_AT, 'beast-g4'), (259.0, 'beast-p4'),
    (263.0, 'beast-g4'), (266.0, 'beast-p4'),
]

# The speech, word by word: (start, end, text), made hers (the recording says "He"; the words on
# screen are ours). Words without their own time are spread evenly.
NARRATION = [
    (0.54, 3.6, 'This criminal, this arch-criminal, has many aliases.'),
    (3.88, 5.6, "She's known not only as the Beast,"),
    (5.82, 8.1, 'she is known as the Woman of Sin.'),
    (8.32, 10.6, 'She is known as the Daughter of Perdition.'),
    (10.80, 13.5, "She's known as the Antichrist."),
    (13.92, 16.4, "We're getting closer."),
    (16.64, 23.9, 'And the closer to her, the faster things are going to happen.'),
    (24.32, 26.9, 'The faster... deliverance.'),
]
WORD_TIMES = {0: [0.54, 1.20, 1.46, 1.74, 2.04, 2.36], 1: [3.88, 4.04, 4.20, 4.52, 4.74, 4.94, 5.10],
              2: [5.82, 6.14, 6.30, 6.48, 6.86, 7.08, 7.28, 7.52], 3: [8.32, 8.50, 8.64, 8.82, 9.16, 9.38, 9.56, 9.70],
              4: [10.80, 11.06, 11.16, 11.42, 11.58]}

CARDS = [
    # (from, to, lines, kind): kind 'rise' fades in and out, 'slam' lands like the Profane title.
    (29.5, 40.5, ['Mark Of', 'The Beast'], 'rise'),
    (33.8, 40.5, ['a Mortal Shell II series'], 'small'),
    (36.5, 40.5, ['by _eins0fx'], 'small'),
    (41.2, 48.6, ['Profane', 'Proxima'], 'slam'),
    (43.0, 48.6, ['Purpose gives the soul its shape.'], 'small'),
    (49.5, 53.6, ['Unholy', 'Genessa'], 'slam'),
    (51.0, 53.6, ['Let Sester Genessa take control over you.'], 'small'),
    (262.0, END, ['Mark Of', 'The Beast'], 'rise'),
    (263.5, END, ['Profane Proxima  .  Unholy Genessa'], 'small'),
    (265.0, END, ['a Mortal Shell II series by _eins0fx'], 'small'),
    (265.5, END, ['Custom Shell System  +  CSSX CINE'], 'tiny'),
]
FLASHES = [(150.8, 0.5), (164.7, 0.5), (209.7, 0.5), (246.6, 0.5), (253.6, 0.6)]   # the skin swaps


def decode_audio(path, seconds, rate=8000, start=0.0):
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-ss', str(start), '-t', str(seconds), '-i', str(path),
                                   '-vn', '-ac', '1', '-ar', str(rate), '-f', 'f32le', '-'])
    return np.frombuffer(raw, dtype=np.float32)


def find_offset(take, track):
    """Seconds into the recording where the take's track starts, and how clearly it was found."""
    rate = 8000
    a = decode_audio(take, 90, rate)
    b = decode_audio(track, 20, rate)
    n = 1 << int(math.ceil(math.log2(len(a) + len(b))))
    corr = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)[:len(a)]
    lag = int(np.argmax(corr))
    return lag / rate, float(corr[lag] / (np.std(corr) + 1e-9))


def distress(mask, seed):
    rng = np.random.default_rng(seed)
    a = np.asarray(mask.filter(ImageFilter.GaussianBlur(2.6))).astype(np.float32) / 255
    noise = Image.fromarray((rng.random(a.shape) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.0))
    n = np.asarray(noise).astype(np.float32) / 255
    fine = rng.random(a.shape).astype(np.float32)
    a = np.clip((a + (n - 0.5) * 0.8 + (fine - 0.5) * 0.1 - 0.45) * 7, 0, 1)
    return Image.fromarray((a * 255).astype(np.uint8))


def fit_size(font_path, size, lines, max_w):
    """The largest size at or under `size` at which every line fits `max_w` pixels: a title that
    is set from the frame height alone clips at the edges of a 9:16 frame."""
    while size > 8:
        font = ImageFont.truetype(str(font_path), size)
        if max(font.getlength(line) for line in lines) <= max_w:
            break
        size = int(size * 0.96)
    return size


class Text:
    """Rendered text as alpha masks: one per line, with a soft glow mask."""

    def __init__(self, font_path, size, lines, worn=True, seed=1):
        font = ImageFont.truetype(str(font_path), size)
        self.lines = []
        pad = int(size * 0.4)
        for i, text in enumerate(lines):
            box = font.getbbox(text)
            m = Image.new('L', (box[2] - box[0] + 2 * pad, box[3] - box[1] + 2 * pad), 0)
            ImageDraw.Draw(m).text((pad - box[0], pad - box[1]), text, font=font, fill=255)
            if worn:
                m = distress(m, seed + i)
            self.lines.append((np.asarray(m).astype(np.float32) / 255,
                               np.asarray(m.filter(ImageFilter.GaussianBlur(size * 0.12))).astype(np.float32) / 255))
        self.pad = pad
        self.height = sum(m.shape[0] for m, _ in self.lines) - 2 * pad * (len(self.lines) - 1)
        self.size = size


def paste(img, alpha, colour, x, y, gain=1.0):
    h, w = alpha.shape
    x0, y0, x1, y1 = max(0, x), max(0, y), min(img.shape[1], x + w), min(img.shape[0], y + h)
    if x0 >= x1 or y0 >= y1:
        return
    a = alpha[y0 - y:y1 - y, x0 - x:x1 - x, None] * gain
    region = img[y0:y1, x0:x1]
    region *= 1 - a
    region += a * np.asarray(colour, np.float32)


def add(img, alpha, colour, x, y, gain=1.0):
    h, w = alpha.shape
    x0, y0, x1, y1 = max(0, x), max(0, y), min(img.shape[1], x + w), min(img.shape[0], y + h)
    if x0 >= x1 or y0 >= y1:
        return
    img[y0:y1, x0:x1] += alpha[y0 - y:y1 - y, x0 - x:x1 - x, None] * gain * np.asarray(colour, np.float32)


def draw_lines(img, text, cy, alpha, scale=1.0, glow=1.0, colour=INK, centre_x=None):
    """The text's lines stacked around cy, at `alpha`, scaled, with a glow."""
    cx = img.shape[1] // 2 if centre_x is None else centre_x
    total = text.height * scale
    y = cy - total / 2
    for ink, soft in text.lines:
        h, w = ink.shape
        if abs(scale - 1) > 1e-3:
            size = (max(1, int(w * scale)), max(1, int(h * scale)))
            ink = np.asarray(Image.fromarray((ink * 255).astype(np.uint8)).resize(size, Image.BILINEAR)).astype(np.float32) / 255
            soft = np.asarray(Image.fromarray((soft * 255).astype(np.uint8)).resize(size, Image.BILINEAR)).astype(np.float32) / 255
            h, w = ink.shape
        x = int(cx - w / 2)
        if glow > 0:
            add(img, soft, (GLOW[0] * 1.5, GLOW[1] * 1.5, GLOW[2] * 1.5), x, int(y), alpha * glow)
        paste(img, ink, colour, x, int(y), alpha)
        y += h - 2 * text.pad * scale


def smooth(u):
    u = min(1.0, max(0.0, u))
    return u * u * (3 - 2 * u)


def decay(t, at, length):
    return math.exp(-(t - at) / length) if t >= at else 0.0


class Film:
    def __init__(self, font, w, h):
        self.w, self.h = w, h
        font_path = font
        big = int(h * 0.19)
        self.cards = {}
        for i, (a, b, lines, kind) in enumerate(CARDS):
            size = fit_size(font, {'small': int(h * 0.045), 'tiny': int(h * 0.03)}.get(kind, big), lines, w * 0.9)
            self.cards[i] = Text(font, size, lines, worn=kind not in ('small', 'tiny'), seed=10 + i)
        # The narration: each line wrapped into rows that fit the frame, each word with its time.
        self.narration = []
        size = int(min(h * 0.095, w * 0.11))
        font = ImageFont.truetype(str(font), size)
        self.narration_font = font
        for i, (a, b, text) in enumerate(NARRATION):
            words = text.split(' ')
            times = WORD_TIMES.get(i) or [a + (b - a) * k / max(1, len(words)) * 0.8 for k in range(len(words))]
            times = list(times) + [times[-1]] * (len(words) - len(times))
            rows, row = [], []
            for word in words:
                if row and font.getlength(' '.join(row + [word])) > w * 0.86:
                    rows.append(row); row = []
                row.append(word)
            rows.append(row)
            rendered, k = [], 0
            for row in rows:
                line = ' '.join(row)
                mask = Text(font_path, size, [line], worn=True, seed=30 + i + 7 * len(rendered)).lines[0]
                spans, x = [], 0.0
                for word in row:
                    ww = font.getlength(word + ' ')
                    spans.append((x, ww, times[k])); x += ww; k += 1
                rendered.append((mask, spans))
            self.narration.append((a, b, rendered))
        self.vignette = self._vignette()
        # The intro's gradient: black at the bottom edge, where the take's CINE caption sits,
        # clear by 60 percent of the way up. The narration reads on it.
        y = np.arange(h, dtype=np.float32)[:, None, None]
        self.bottom = np.clip((y - h * 0.60) / (h * 0.40), 0, 1) ** 1.4

    def _vignette(self):
        y, x = np.mgrid[0:self.h, 0:self.w].astype(np.float32)
        r = np.sqrt(((x - self.w / 2) / (self.w / 2)) ** 2 + ((y - self.h / 2) / (self.h / 2)) ** 2)
        return np.clip((r - 0.55) / 0.9, 0, 1)[..., None]

    def narrate(self, img, t):
        for i, (a, b, rows) in enumerate(self.narration):
            # A line holds until the next line starts (or 1.2 s past its end): never two at once.
            until = min(b + 1.2, self.narration[i + 1][0]) if i + 1 < len(self.narration) else b + 1.2
            if not (a <= t < until):
                continue
            hold = min(1.0, (t - a) / 0.25)
            img *= 1 - 0.5 * hold * self.vignette
            img *= 1 - 0.3 * hold
            fade = 1.0 if t < b else max(0.0, min(1.0, (until - t) / 0.25, 1 - (t - b) / 1.2))
            pad = int(self.narration_font.size * 0.4)
            row_h = int(self.narration_font.size * 1.25)
            # The block is centred on the frame, its rows stacked around the middle.
            top = int(self.h * 0.5 - row_h * (len(rows) - 1) / 2)
            for r, ((ink, soft), spans) in enumerate(rows):
                h, w = ink.shape
                shown = np.zeros(w, np.float32)
                for x0, ww, at in spans:
                    if t >= at:
                        k = smooth((t - at) / 0.18)
                        # A little margin either side: blackletter capitals swing past the advance width.
                        lo, hi = max(0, int(x0) + pad - self.narration_font.size // 3), min(w, int(x0 + ww) + pad)
                        shown[lo:hi] = np.maximum(shown[lo:hi], k)
                x = int(self.w / 2 - w / 2); y = int(top + r * row_h - h / 2)
                add(img, soft * shown[None, :] * fade, (GLOW[0] * 1.4, GLOW[1] * 1.4, GLOW[2] * 1.4), x, y, 1.0)
                paste(img, ink * shown[None, :] * fade, (232, 196, 188), x, y)

    def cards_at(self, img, t, rng):
        stack_y = {}
        # The title block is centred on the frame; its small lines hang under the big text.
        centre = int(self.h * 0.47)
        below = int(self.h * 0.60)
        for i, (a, b, lines, kind) in enumerate(CARDS):
            if not (a <= t < b + 0.8):
                continue
            text = self.cards[i]
            if kind == 'rise':
                alpha = smooth((t - a) / 1.6) * (1.0 if t < b else max(0.0, 1 - (t - b) / 0.8))
                img *= 1 - 0.5 * alpha
                draw_lines(img, text, centre, alpha, 1 + 0.03 * smooth((t - a) / 8), glow=1.0)
                below = centre + text.height // 2 + int(self.h * 0.012)
            elif kind == 'slam':
                e = decay(t, a, 0.07)
                alpha = 1.0 if t < b else max(0.0, 1 - (t - b) / 0.8)
                shake = 14 * e
                jitter = (int(rng.normal(0, shake)), int(rng.normal(0, shake * 0.6))) if shake > 0.5 else (0, 0)
                img *= 1 - 0.45 * alpha
                # The hit goes white and stays white for a third of a second, then clears: her
                # own outfit lands under it (she is not drawn for a few frames while it loads).
                white = 1.0 if t - a < 0.3 else math.exp(-(t - a - 0.3) / 0.12)
                if 0 <= t - a and white > 0.005:
                    img += (255 - np.clip(img, 0, 255)) * 0.85 * white
                draw_lines(img, text, centre + jitter[1], alpha, 1 + 0.3 * e, glow=1 + 1.4 * e, centre_x=self.w // 2 + jitter[0])
                below = centre + text.height // 2 + int(self.h * 0.012)
            elif kind == 'tiny':
                alpha = smooth((t - a) / 1.0) * (1.0 if t < b else max(0.0, 1 - (t - b) / 0.8))
                draw_lines(img, text, int(self.h * 0.86), alpha * 0.75, glow=0.0, colour=FAINT)
            else:
                alpha = smooth((t - a) / 1.0) * (1.0 if t < b else max(0.0, 1 - (t - b) / 0.8))
                y = below + text.height // 2 + stack_y.get('small', 0)
                draw_lines(img, text, y, alpha * 0.9, glow=0.0, colour=FAINT)
                stack_y['small'] = stack_y.get('small', 0) + text.height + int(self.h * 0.012)

    def quiet(self, t):
        """Nothing to draw at t: the frame goes out as it came in (most of the film)."""
        if t < 54.0 or t >= 262.0:
            return False
        if any(a <= t < b + 0.8 for a, b, _, _ in CARDS):
            return False
        return not any(decay(t, at, 0.07) > 0.005 for at, _ in FLASHES)

    def frame(self, t, frame, rng):
        if self.quiet(t):
            return frame
        img = frame.astype(np.float32)
        # The gradient covers the whole intro, narration and name cards, and is gone by the groove.
        if t < 54.0:
            img *= 1 - self.bottom * (1.0 if t < 53.0 else max(0.0, 1 - (t - 53.0) / 1.0))
        if t < 27.0:
            self.narrate(img, t)
        self.cards_at(img, t, rng)
        flash = sum(s * decay(t, at, 0.07) for at, s in FLASHES)
        if flash > 0.005:
            img += (255 - np.clip(img, 0, 255)) * flash
        if t >= 262.0:
            img *= max(0.0, 1 - (t - 262.0) / (END - 262.0)) ** 0.7 * 0.5 + 0.5 * max(0.0, 1 - (t - 266.0) / (END - 266.0))
        if t > END - 1.0:
            img *= max(0.0, END - t)
        return np.clip(img, 0, 255).astype(np.uint8)


def newest(takes_dir, take_id):
    found = sorted(Path(takes_dir).glob(f'{take_id}-*.mp4'), key=lambda p: p.stat().st_mtime)
    return found[-1] if found else None


def segments(start, stop):
    """(from, to, take) of the edit within start..stop."""
    out = []
    for i, (at, take) in enumerate(CUTS):
        to = CUTS[i + 1][0] if i + 1 < len(CUTS) else END
        a, b = max(at, start), min(to, stop)
        if a < b:
            out.append((a, b, take))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--takes', required=True)
    ap.add_argument('--slices', required=True)
    ap.add_argument('--song', required=True)
    ap.add_argument('--font', required=True)
    ap.add_argument('--out')
    ap.add_argument('--from', dest='start', type=float, default=0.0)
    ap.add_argument('--to', dest='stop', type=float, default=END)
    ap.add_argument('--list', action='store_true')
    opts = ap.parse_args()

    starts = {t['id']: t['song'][0] for t in TAKES}
    needed = sorted({take for _, _, take in segments(opts.start, opts.stop)})
    files = {take: newest(opts.takes, take) for take in needed}
    missing = [take for take, f in files.items() if f is None]
    if opts.list or missing:
        for take in needed:
            print(f"{take:10s} {'missing' if files[take] is None else files[take].name}")
        if missing:
            raise SystemExit('Record the missing takes first.')
        if opts.list:
            return
    if not opts.out:
        raise SystemExit('--out is needed to render.')
    # Where each take's track starts in its recording.
    lags = {}
    sizes = set()
    for take, f in files.items():
        lag, score = find_offset(f, Path(opts.slices) / f'{take}.wav')
        if score < 8:
            raise SystemExit(f'{f.name}: the track was not found clearly in the recording (score {score:.1f}).')
        lags[take] = lag
        info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries', 'stream=width,height', '-of', 'json', f]))['streams'][0]
        sizes.add((info['width'], info['height']))
        print(f'{take}: track starts {lag:.3f} s into {f.name} (match {score:.0f}), {info["width"]}x{info["height"]}')
    if len(sizes) != 1:
        raise SystemExit(f'The takes differ in size: {sorted(sizes)}. Record them all in the same window.')
    (w, h), = sizes
    film = Film(opts.font, w, h)
    rng = np.random.default_rng(5)
    length = opts.stop - opts.start
    audio = f'atrim=start={opts.start}:end={opts.stop},asetpts=PTS-STARTPTS,afade=t=out:st={max(0.0, length - 1.0)}:d=1.0'
    writer = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{w}x{h}', '-r', str(FPS), '-i', '-',
                               '-i', opts.song, '-map', '0:v', '-map', '1:a', '-af', audio,
                               '-c:v', 'h264_nvenc', '-preset', 'p7', '-tune', 'hq', '-rc', 'vbr', '-cq', '17', '-b:v', '0', '-maxrate', '60M', '-bufsize', '120M', '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
                               '-c:a', 'aac', '-b:a', '320k', '-t', f'{length:.4f}', opts.out], stdin=subprocess.PIPE)
    size = w * h * 3
    frame_index = 0
    for a, b, take in segments(opts.start, opts.stop):
        at_in_take = lags[take] + (a - starts[take])
        n = int(round(b * FPS)) - int(round(a * FPS))
        reader = subprocess.Popen(['ffmpeg', '-v', 'error', '-ss', f'{at_in_take:.4f}', '-i', str(files[take]), '-t', f'{n / FPS + 0.5:.4f}',
                                   '-vf', f'fps={FPS}', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE)
        last = None
        for k in range(n):
            raw = reader.stdout.read(size)
            if len(raw) == size:
                last = np.frombuffer(raw, np.uint8).reshape(h, w, 3)
            elif last is None:
                raise SystemExit(f'{files[take].name} has no picture at {at_in_take:.2f} s.')
            t = a + k / FPS
            writer.stdin.write(film.frame(t, last, rng).tobytes())
            frame_index += 1
        reader.kill()
        print(f'{a:7.2f} to {b:7.2f}  {take}', flush=True)
    writer.stdin.close()
    writer.wait()
    print(opts.out)


if __name__ == '__main__':
    main()
