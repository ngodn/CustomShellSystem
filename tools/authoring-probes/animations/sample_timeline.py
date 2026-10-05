"""Endpoint-inclusive timelines for verified 25/30/60 Hz source animations."""
from dataclasses import dataclass
import math


@dataclass(frozen=True)
class SampleTimeline:
    source_frames: int
    frames: int
    fps: int

    def source_frame(self, frame):
        if not isinstance(frame, int) or not 0 <= frame < self.frames:
            raise ValueError('Sample outside the output timeline')
        return frame * (self.source_frames - 1) / (self.frames - 1)


def timeline(frame_count, duration, fps=None):
    if type(frame_count) is not int or not 2 <= frame_count <= 100000:
        raise ValueError('Invalid source frame count')
    if not math.isfinite(duration) or duration <= 0 or fps not in (None, 25, 30, 60):
        raise ValueError('Invalid duration or output sample rate')
    # SequenceLength is serialized as float32. Allow its rounding, not a time stretch.
    tolerance = max(1e-6, duration * 1.2e-7)
    rates = [rate for rate in (25, 30, 60)
             if abs((frame_count - 1) / rate - duration) <= tolerance]
    if len(rates) != 1:
        raise ValueError('Source is not on a verified 25/30/60 Hz timeline')
    source_rate = rates[0]
    if fps is None:
        fps = source_rate
    if source_rate > fps:
        raise ValueError('Source downsampling requires a separate review')
    numerator = (frame_count - 1) * fps
    if numerator % source_rate:
        raise ValueError('Output sample rate cannot preserve the exact endpoint')
    intervals = numerator // source_rate
    return SampleTimeline(frame_count, intervals + 1, fps)
