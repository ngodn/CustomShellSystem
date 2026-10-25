"""Finish the Profane Proxima film: the take as recorded up to bar 12, then a flash-cut
transition on the song's last hits and the title card slammed onto the drop.
Python 3.12 with numpy and pillow (mise x uv@0.12.5 -- uv run --python 3.12 --with numpy --with pillow).

  profane_proxima_post.py --take TAKE.mp4 --song SONG --font FONT.ttf --out OUT.mp4
                          [--from SECONDS] [--to SECONDS] [--offset SECONDS]

TAKE is the recording of the profane-proxima preset (record.py), with the song playing in it.
The song is found in the recording's sound, so the picture lines up with the clean track to
the frame; --offset gives that point by hand instead. --from/--to render only part (a preview).

Song time (tools/profane_proxima_film.py):
  0 - 15.98     the take as it is, with a flash on each skin swap (6.33, 10.44, 14.88).
  15.98         the last rise: a punch-in on the hit, the picture split into colours and
                darkening to red.
  16.32         the first hit: cut to the title card, PROFANE slams in.
  16.70         the second hit: PROXIMA slams in below it.
  16.85         the hard drop, the last sound: one last flash, her face comes up dark red behind
                the title, and the song ends.
  then          silence, so nothing moves: the title holds still until the picture fades by 19.6.
Never commit the song or the font: the song is the user's, the font is downloaded.
"""
import argparse
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FPS = 60
BEAT = 60.0 / 170.14
DROP = 16.850            # the hard drop: the music cuts out (16.83-16.85), the song ends here
PUNCH = [15.980, 16.324, 16.695]
HITS = (16.324, 16.695)  # the last two hits before the drop
# The skin swaps (tools/profane_proxima_film.py): a flash on each hides the frame
# the mesh changes on. (time, strength)
FLASHES = [(6.33, 0.5), (10.44, 0.5), (14.88, 0.6)]
AUDIO_FADE = (DROP - 0.02, 0.025)   # a clean cut on the drop, no click
END = 19.60
TAKE_END = 19.55         # the preset's last frame (end 19.6 s); later frames show the camera going home
FADE_OUT = 0.5
INK = (206, 168, 160)    # dusty rose, lighter than the song's title so it reads over the dark red
GLOW = (150, 12, 6)      # Devout embers red


def decode_audio(path, seconds, rate=8000, start=0.0):
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-ss', str(start), '-t', str(seconds), '-i', str(path),
                                   '-vn', '-ac', '1', '-ar', str(rate), '-f', 'f32le', '-'])
    return np.frombuffer(raw, dtype=np.float32)


def find_offset(take, song):
    """Seconds into the take recording where the song's first sample plays."""
    rate = 8000
    a = decode_audio(take, 40, rate)
    b = decode_audio(song, 16, rate)
    n = 1 << int(math.ceil(math.log2(len(a) + len(b))))
    corr = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)[:len(a)]
    lag = int(np.argmax(corr))
    # Strength: the peak against the spread of the correlation.
    score = corr[lag] / (np.std(corr) + 1e-9)
    return lag / rate, score


def distress(mask, seed):
    """A worn ink edge like the song's title: blur, noise, threshold."""
    rng = np.random.default_rng(seed)
    a = np.asarray(mask.filter(ImageFilter.GaussianBlur(3.0))).astype(np.float32) / 255
    noise = Image.fromarray((rng.random(a.shape) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2.2))
    n = np.asarray(noise).astype(np.float32) / 255
    fine = rng.random(a.shape).astype(np.float32)
    a = np.clip((a + (n - 0.5) * 0.85 + (fine - 0.5) * 0.12 - 0.45) * 7, 0, 1)
    return Image.fromarray((a * 255).astype(np.uint8))


class Title:
    """The two lines, drawn once at full size; each frame scales and places them."""

    def __init__(self, font_path, width, height):
        self.w, self.h = width, height
        size = int(height * 0.25)
        font = ImageFont.truetype(str(font_path), size)
        # Narrow windows: the longer line fits 86 percent of the width.
        widest = max(font.getbbox(text)[2] - font.getbbox(text)[0] for text in ('Profane', 'Proxima'))
        if widest > width * 0.86:
            size = int(size * width * 0.86 / widest)
            font = ImageFont.truetype(str(font_path), size)
        self.lines = []
        for i, text in enumerate(('Profane', 'Proxima')):
            box = font.getbbox(text)
            pad = int(size * 0.35)
            m = Image.new('L', (box[2] - box[0] + 2 * pad, box[3] - box[1] + 2 * pad), 0)
            ImageDraw.Draw(m).text((pad - box[0], pad - box[1]), text, font=font, fill=255)
            m = distress(m, 7 + i)
            glow = m.filter(ImageFilter.GaussianBlur(size * 0.10))
            self.lines.append((m, glow))
        gap = int(size * 0.05)
        heights = [m.height for m, _ in self.lines]
        total = sum(heights) - 2 * int(size * 0.35) + gap
        top = (height - total) // 2 - int(size * 0.35)
        self.centres = [(width // 2, top + heights[0] // 2), (width // 2, top + heights[0] - int(size * 0.35) * 2 + gap + heights[1] // 2)]

    def layer(self, line, scale, white=0.0, glow=1.0, jitter=(0, 0)):
        """(alpha of the ink, alpha of the glow, box) for one line at `scale`."""
        m, g = self.lines[line]
        if abs(scale - 1) > 1e-3:
            size = (max(1, int(m.width * scale)), max(1, int(m.height * scale)))
            m, g = m.resize(size, Image.BILINEAR), g.resize(size, Image.BILINEAR)
        cx, cy = self.centres[line]
        x, y = cx - m.width // 2 + jitter[0], cy - m.height // 2 + jitter[1]
        return np.asarray(m).astype(np.float32) / 255, np.asarray(g).astype(np.float32) / 255 * glow, (x, y)


def paste(img, alpha, colour, box):
    """img (float H,W,3) <- colour over alpha at box, clipped to the frame."""
    x, y = box
    h, w = alpha.shape
    x0, y0, x1, y1 = max(0, x), max(0, y), min(img.shape[1], x + w), min(img.shape[0], y + h)
    if x0 >= x1 or y0 >= y1:
        return
    a = alpha[y0 - y:y1 - y, x0 - x:x1 - x, None]
    region = img[y0:y1, x0:x1]
    region *= 1 - a
    region += a * np.asarray(colour, np.float32)


def add(img, alpha, colour, box):
    x, y = box
    h, w = alpha.shape
    x0, y0, x1, y1 = max(0, x), max(0, y), min(img.shape[1], x + w), min(img.shape[0], y + h)
    if x0 >= x1 or y0 >= y1:
        return
    img[y0:y1, x0:x1] += alpha[y0 - y:y1 - y, x0 - x:x1 - x, None] * np.asarray(colour, np.float32)


def decay(t, at, length):
    return math.exp(-(t - at) / length) if t >= at else 0.0


def punch_zoom(frame, scale):
    if scale <= 1.0005:
        return frame
    h, w = frame.shape[:2]
    cw, ch = int(w / scale), int(h / scale)
    x, y = (w - cw) // 2, (h - ch) // 2
    return np.asarray(Image.fromarray(frame[y:y + ch, x:x + cw]).resize((w, h), Image.BILINEAR))


def split(img, dx):
    """Red one way, blue the other."""
    d = int(round(dx))
    if d == 0:
        return img
    out = img.copy()
    out[:, d:, 0] = img[:, :-d, 0]
    out[:, :-d, 2] = img[:, d:, 2]
    return out


def grade_red(img, amount, darken):
    """Toward a dark, desaturated red: the drop's palette."""
    lum = img @ np.array([0.299, 0.587, 0.114], np.float32)
    red = np.stack([lum * 1.15, lum * 0.32, lum * 0.28], -1)
    return (img * (1 - amount) + red * amount) * darken


def soften(img, radius):
    u8 = np.clip(img, 0, 255).astype(np.uint8)
    return np.asarray(Image.fromarray(u8).filter(ImageFilter.GaussianBlur(radius))).astype(np.float32)


def render(t, frame, title, rng):
    """One output frame at song time t from the take's frame at that time."""
    if t < PUNCH[0]:
        flash = sum(s * decay(t, at, 0.07) for at, s in FLASHES)
        if flash < 0.01:
            return frame
        img = frame.astype(np.float32)
        img += (255 - img) * flash
        return np.clip(img, 0, 255).astype(np.uint8)

    if t < HITS[0]:
        # The last rise: punch-ins on the hits, darkening to red.
        e = decay(t, PUNCH[0], 0.09)
        frame = punch_zoom(frame, 1 + 0.06 * e)
        img = frame.astype(np.float32)
        build = min(1.0, (t - PUNCH[0]) / (HITS[0] - PUNCH[0]))
        img = grade_red(img, 0.25 + 0.5 * build, 1.0 - 0.3 * build)
        img = split(img, 22 * e + 4 * build)
        img += (255 - img) * 0.55 * decay(t, PUNCH[0], 0.07)
        return np.clip(img, 0, 255).astype(np.uint8)

    # The title card. Every flash sits on a sound: PROFANE on the first hit, PROXIMA on the
    # second, the last flash on the drop. After the drop there is no sound, so nothing moves:
    # the title holds still over her face until the fade.
    slams = [decay(t, at, 0.06) for at in (*HITS, DROP)]
    shake = 14 * slams[0] + 14 * slams[1] + 10 * slams[2]
    jitter = (int(rng.normal(0, shake)), int(rng.normal(0, shake * 0.6))) if shake > 0.5 else (0, 0)
    dark = 0.16 if t < DROP else 0.16 + (0.42 - 0.16) * min(1.0, (t - DROP) / 0.25)
    img = soften(grade_red(frame.astype(np.float32), 0.70, dark), 6)
    for line, at in ((0, HITS[0]), (1, HITS[1])):
        if t < at:
            continue
        scale = 1 + 0.32 * decay(t, at, 0.055)
        ink, glow, box = title.layer(line, scale, glow=1 + 1.6 * (decay(t, at, 0.08) + decay(t, DROP, 0.08)), jitter=jitter)
        add(img, glow, (GLOW[0] * 1.6, GLOW[1] * 1.6, GLOW[2] * 1.6), box)
        paste(img, ink, INK, box)
    img = split(img, 14 * max(slams))
    img += (255 - np.clip(img, 0, 255)) * (0.6 * slams[0] + 0.6 * slams[1] + 0.85 * slams[2])
    if t > END - FADE_OUT:
        img *= max(0.0, (END - t) / FADE_OUT)
    return np.clip(img, 0, 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--take', required=True)
    ap.add_argument('--song', required=True)
    ap.add_argument('--font', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--offset', type=float)
    ap.add_argument('--from', dest='start', type=float, default=0.0)
    ap.add_argument('--to', dest='stop', type=float, default=END)
    opts = ap.parse_args()

    if opts.offset is None:
        offset, score = find_offset(opts.take, opts.song)
        print(f'song starts {offset:.3f} s into the take (match {score:.1f})')
        if score < 8:
            raise SystemExit('The song was not found clearly in the take; pass --offset.')
    else:
        offset = opts.offset
    info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_entries',
                                               'stream=width,height', '-of', 'json', opts.take]))['streams'][0]
    w, h = info['width'], info['height']
    title = Title(opts.font, w, h)
    rng = np.random.default_rng(3)
    length = opts.stop - opts.start
    # The take ends a little before END; its last frame holds under the title if so.
    reader = subprocess.Popen(['ffmpeg', '-v', 'error', '-ss', f'{offset + opts.start:.4f}', '-i', opts.take, '-t', f'{length:.4f}',
                               '-vf', f'fps={FPS}', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE)
    audio = (f"atrim=start={opts.start}:end={min(opts.stop, AUDIO_FADE[0] + AUDIO_FADE[1])},asetpts=PTS-STARTPTS,"
             f"afade=t=out:st={max(0.0, AUDIO_FADE[0] - opts.start)}:d={AUDIO_FADE[1]},apad=whole_dur={length}")
    writer = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{w}x{h}', '-r', str(FPS), '-i', '-',
                               '-i', opts.song, '-map', '0:v', '-map', '1:a', '-af', audio,
                               '-c:v', 'libx264', '-preset', 'slow', '-crf', '15', '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
                               '-c:a', 'aac', '-b:a', '320k', '-t', f'{length:.4f}', opts.out], stdin=subprocess.PIPE)
    size = w * h * 3
    frames = int(round(length * FPS))
    last = None
    for k in range(frames):
        raw = reader.stdout.read(size)
        if len(raw) == size and (last is None or opts.start + k / FPS <= TAKE_END):
            last = np.frombuffer(raw, np.uint8).reshape(h, w, 3)
        elif last is None:
            raise SystemExit('The take has no picture at that time.')
        t = opts.start + k / FPS
        writer.stdin.write(render(t, last, title, rng).tobytes())
        if k % 120 == 0:
            print(f'{t:6.2f} s', flush=True)
    writer.stdin.close()
    writer.wait()
    reader.kill()
    print(opts.out)


if __name__ == '__main__':
    main()
