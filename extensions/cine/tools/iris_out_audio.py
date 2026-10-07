"""Build the Iris Out take's track from the full song and its instrumental. Python 3.14 + numpy + ffmpeg.

  tools/iris_out_audio.py SONG INSTRUMENTAL CINE_FOLDER [--sources isss] [--out iris-out-x4.wav]

Writes CINE_FOLDER/audio/<out>. --sources: one letter a pass, i (the instrumental, for the
bones pass) or s (the song); iris-out is isss, iris-out-2 (Eve first, the bones second) siss. Local only: the audio is the user's own copy and is
never committed. Everything is on the song's own timeline (the dance section starts at 44.936 s;
the instrumental runs 36.3 ms behind the song; 136 BPM by librosa's beat tracker).

The take is four passes of the dance section, one pass period (23.127 s) apart:

  pass 1 (bones)    the instrumental, from the section's start
  gap (x3)          the instrumental's own build-up into the section (38.021 s onwards)
  passes 2 to 4     the song, from the section's start
  ending            the song running on past the section, fading out over two bars

Mixed like a DJ mix, sample by sample, not spliced:
- the instrumental is gain-matched to the song (equal loudness over the dance section), and a
  look-ahead peak limiter keeps the lifted peaks under full scale;
- leaving a pass: the song runs on past the section while the gap's build-up comes in under it,
  an equal-power crossfade over two beats on the beat grid (the jump point is the beat-matched
  one found by work/iris-out/find_jump.py: same place in the bar, closest harmony and timbre);
- the gap is brought to the dance section's loudness by a slow leveller (the build-up is
  quieter by nature, its middle most), so the whole take plays at one loudness (the user's choice);
- arriving: the instrumental build-up and the song are the same music at the same moment, so a
  one-beat equal-gain crossfade before the downbeat brings the voice in on the music it belongs to
  (equal power on identical music would bump the level 3 dB);
- every gain is a smooth per-sample curve; the result is checked for clicks at every join.
"""
import subprocess
import sys
from pathlib import Path

import numpy as np

RATE = 48000
S0, LEAVE, JUMP = 44.936, 61.148, 38.021
INST_OFFSET = 0.0363
BEAT = 60 / 136.0
BAR = 4 * BEAT
PERIOD = LEAVE - JUMP            # 23.127 s
GAP_DB = 0.0                     # the gaps at the music's loudness (the user's choice, 7 Oct)
TRIM_DB = -1.5                   # the whole track, before the limiter
OUT_FADE = 2 * BEAT              # leaving a pass
IN_FADE = BEAT                   # arriving on the next pass's downbeat


def decode(path):
    raw = subprocess.check_output(['ffmpeg', '-v', 'error', '-i', path, '-vn', '-ac', '2', '-ar', str(RATE), '-f', 'f32le', '-'])
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)


def take(source, start, length):
    """`length` seconds of `source` from `start` (source seconds), zero past either end."""
    a, n = int(round(start * RATE)), int(round(length * RATE))
    out = np.zeros((n, 2))
    lo, hi = max(a, 0), min(a + n, len(source))
    if hi > lo:
        out[lo - a:hi - a] = source[lo:hi]
    return out


def rms(x):
    return float(np.sqrt(np.mean(x ** 2)) + 1e-12)


def ramp(n, rising):
    """Equal-power curve over n samples: sin from 0 to 1, or cos from 1 to 0. For two different
    pieces of music (their powers add)."""
    t = (np.arange(n) + 0.5) / n
    return np.sin(t * np.pi / 2) if rising else np.cos(t * np.pi / 2)


def linear(n, rising):
    """Equal-gain curve: for the same music on both sides (the instrumental and the song at
    the same moment), whose amplitudes add; equal power there would bump the level 3 dB."""
    t = (np.arange(n) + 0.5) / n
    return 0.5 - 0.5 * np.cos(t * np.pi) if rising else 0.5 + 0.5 * np.cos(t * np.pi)


def limit(mix, ceiling=0.98, release=0.12):
    """A look-ahead peak limiter: the gain comes down just before a peak (one 5 ms block ahead)
    and recovers smoothly over `release` seconds, so peaks stay under the ceiling while the
    loudness stays where it is (scaling the whole track down would make everything quieter)."""
    block = int(0.005 * RATE)
    n = len(mix)
    peaks = np.abs(mix).max(axis=1)
    blocks = -(-n // block)
    padded = np.zeros(blocks * block); padded[:n] = peaks
    need = np.minimum(1.0, ceiling / np.maximum(padded.reshape(blocks, block).max(axis=1), 1e-9))
    ahead = np.minimum(need, np.append(need[1:], 1.0))          # start coming down a block early
    smooth = np.empty(blocks); g = 1.0; k = np.exp(-0.005 / release)
    for i in range(blocks):
        g = ahead[i] if ahead[i] < g else ahead[i] + (g - ahead[i]) * k
        smooth[i] = g
    gain = np.interp(np.arange(n), np.arange(blocks) * block + block / 2, smooth)
    out = mix * gain[:, None]
    reduced = float(np.mean(gain < 0.999)) * 100
    print(f'limiter: {reduced:.1f}% of the track touched, deepest {20 * np.log10(gain.min()):+.1f} dB')
    return np.clip(out, -ceiling, ceiling)


def level_to(piece, target, most=2.0, window=2.0):
    """A slow leveller: the piece's loudness, measured in 100 ms steps and smoothed over
    `window` seconds, brought to `target` (RMS), never lifted more than `most` times. Like a
    mastering leveller, not a compressor: it moves over seconds, so nothing pumps."""
    step = int(0.1 * RATE)
    n = len(piece); blocks = -(-n // step)
    padded = np.zeros((blocks * step, 2)); padded[:n] = piece
    loud = np.sqrt(np.mean(padded.reshape(blocks, step, 2) ** 2, axis=(1, 2))) + 1e-9
    reach = max(1, int(window / 0.1))
    kernel = np.hanning(2 * reach + 1); kernel /= kernel.sum()
    smooth = np.convolve(np.pad(loud, reach, mode='edge'), kernel, mode='valid')
    gain = np.clip(target / smooth, 1 / most, most)
    return np.interp(np.arange(n), np.arange(blocks) * step + step / 2, gain)


def main():
    import argparse
    args = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    args.add_argument('song'); args.add_argument('instrumental'); args.add_argument('cine')
    args.add_argument('--sources', default='isss', help='one letter a pass (2 to 4): i instrumental, s song')
    args.add_argument('--out', default='iris-out-x4.wav')
    opts = args.parse_args()
    if not 2 <= len(opts.sources) <= 4 or set(opts.sources) - {'i', 's'}:
        raise SystemExit('--sources is 2 to 4 letters, i or s')
    passes = len(opts.sources)
    song, inst = decode(opts.song), decode(opts.instrumental)
    cine = Path(opts.cine)
    # The instrumental on the song's timeline, at the song's loudness over the dance section.
    def inst_at(start, length):
        return take(inst, start + INST_OFFSET, length)
    section_song, section_inst = take(song, S0, LEAVE - S0), inst_at(S0, LEAVE - S0)
    loudness = rms(section_song) / rms(section_inst)
    headroom = float(np.max(np.abs(section_song))) / float(np.max(np.abs(section_inst)))
    gain = loudness   # as loud as the song; the limiter at the end catches the peaks this lifts
    # The gap's build-up is quieter than the section by nature: matched to the section too,
    # so every part of the take plays at one loudness.
    gap_gain = rms(section_song) / rms(inst_at(JUMP, S0 - JUMP) * gain)
    print(f'gap gain {20 * np.log10(gap_gain):+.2f} dB to match the section')
    print(f'instrumental gain {20 * np.log10(gain):+.2f} dB (loudness match; the song peaks {20 * np.log10(headroom):+.2f} dB higher)')

    total = (passes - 1) * PERIOD + (LEAVE - S0) + 2 * BAR
    mix = np.zeros((int(round(total * RATE)) + 1, 2))

    def add(at, piece, envelope):
        a = int(round(at * RATE))
        n = min(len(piece), len(mix) - a)
        mix[a:a + n] += piece[:n] * envelope[:n, None]

    gap_level = 10 ** (GAP_DB / 20)   # the leveller already brings the gap to the section's loudness
    for k in range(passes):
        t0 = k * PERIOD                                  # the pass's first downbeat, on the take's clock
        vocal = opts.sources[k] == 's'
        src = (lambda s, n: take(song, s, n)) if vocal else (lambda s, n: inst_at(s, n) * gain)
        last = k == passes - 1
        # The pass, from one beat before its downbeat (the arriving crossfade) to its end.
        pre = IN_FADE if k > 0 else 0.0
        tail = 2 * BAR if last else OUT_FADE / 2          # runs on past LEAVE under the leaving crossfade
        length = pre + (LEAVE - S0) + tail
        piece = src(S0 - pre, length)
        env = np.ones(len(piece))
        if k > 0:
            env[:int(round(pre * RATE))] = linear(int(round(pre * RATE)), True)
        else:
            m = int(0.3 * RATE); env[:m] = ramp(m, True)   # the take opens with a short fade-in
        if last:   # the song's own two bars, fading over the second
            m = int(round(BAR * RATE)); env[-m:] = ramp(m, False)
        else:      # leaving: out over OUT_FADE, centred on LEAVE
            m = int(round(OUT_FADE * RATE)); env[-m:] = ramp(m, False)
        add(t0 - pre, piece, env)
        if last:
            break
        # The gap: the instrumental's build-up from JUMP (in at LEAVE - OUT_FADE/2) up to the next
        # downbeat. Its last beat is the arriving crossfade, the very beat the next pass fades in
        # over, so the two always add up to one.
        g0 = t0 + (LEAVE - S0) - OUT_FADE / 2
        g_len = (PERIOD - (LEAVE - S0)) + OUT_FADE / 2
        piece = inst_at(JUMP - OUT_FADE / 2, g_len) * gain
        piece *= level_to(piece, rms(section_song))[:, None]   # one loudness all the way through
        n = len(piece)
        level = np.full(n, gap_level)
        swell = int(round(BAR * RATE)); arrive = int(round(IN_FADE * RATE))
        # back to full over the last bar before the downbeat
        s_end = n - arrive
        # from the gap's level to the section's (1.0) over the last bar, into the next downbeat
        level[s_end - swell:s_end] = gap_level + (1 - gap_level) * (0.5 - 0.5 * np.cos(np.linspace(0, np.pi, swell)))
        level[s_end:] = 1.0
        env = level.copy()
        m = int(round(OUT_FADE * RATE)); env[:m] *= ramp(m, True)       # coming in under the leaving song
        env[-arrive:] *= linear(arrive, False)                          # handing over to the song (the same music)
        add(g0, piece, env)

    mix *= 10 ** (TRIM_DB / 20)   # room under the ceiling, so the limiter only meets the lifted peaks
    mix = limit(mix)
    # A click is a sample-to-sample jump far beyond the music's own: compare each join's
    # neighbourhood with the track's typical jump size.
    jumps = np.abs(np.diff(mix[:, 0]))
    typical = float(np.percentile(jumps, 99.9))
    for k in range(passes - 1):
        for label, at in (('leave', k * PERIOD + (LEAVE - S0)), ('arrive', (k + 1) * PERIOD)):
            a = int(at * RATE)
            near = jumps[a - int(0.6 * RATE):a + int(0.6 * RATE)]
            before = rms(mix[a - int(0.5 * RATE):a]); after = rms(mix[a:a + int(0.5 * RATE)])
            print(f'pass {k + 1} {label:6s} at {at:6.2f} s: largest step {near.max() / typical:4.2f}x the track\'s 99.9th percentile, level {20 * np.log10(before):+.1f} -> {20 * np.log10(after):+.1f} dBFS')
    for k in range(passes):
        t0 = k * PERIOD
        seg = mix[int((t0 + 1) * RATE):int((t0 + LEAVE - S0 - 1) * RATE)]
        line = f'pass {k + 1} ({"song" if opts.sources[k] == "s" else "instrumental"}) {20 * np.log10(rms(seg)):+.1f} dBFS'
        if k < passes - 1:
            gap = mix[int((t0 + LEAVE - S0 + 1) * RATE):int((t0 + PERIOD - 1) * RATE)]
            line += f', gap after it {20 * np.log10(rms(gap)):+.1f} dBFS'
        print(line)
    (cine / 'audio').mkdir(parents=True, exist_ok=True)
    out = cine / 'audio' / opts.out
    pcm = (np.clip(mix, -1, 1) * 32767).astype('<i2').tobytes()
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-f', 's16le', '-ar', str(RATE), '-ac', '2', '-i', '-', str(out)], input=pcm, check=True)
    print(out, f'{len(mix) / RATE:.3f} s')


if __name__ == '__main__':
    main()
