"""Wait for an existing CSS performance phase without controlling playback."""
import math
import time


def wait_phase(engine, seconds, expected=None):
    def status():
        return engine(dict(op='service.call', service='css.customize', version=1,
                           request={'action': 'performance', 'op': 'status'}))

    first = status()
    identity = {key: first.get(key) for key in ('token', 'id', 'duration', 'rate')}
    if expected is not None and identity != expected:
        raise RuntimeError('Performance changed before phase capture')
    duration, rate = first.get('duration', 0), first.get('rate', 0)
    if (not math.isfinite(seconds) or not 0 <= seconds < duration or
            not first.get('active') or first.get('paused') or rate <= 0 or
            duration / rate > 30):
        raise RuntimeError('Need a playing performance with a loop of at most 30 seconds')
    deadline = time.monotonic() + 2 * duration / rate + 3
    current = first
    while time.monotonic() < deadline:
        if ({key: current.get(key) for key in identity} != identity or
                not current.get('active') or current.get('paused')):
            raise RuntimeError('Performance changed while waiting for capture')
        delta = current['seconds'] - seconds
        if 0 <= delta <= .1:
            return dict(identity=identity, target_seconds=seconds, observed=current,
                        limitation='Phase sampled before screenshot request, not the exact rendered frame timestamp.')
        until = (seconds - current['seconds']) % duration
        time.sleep(min(.2, max(.01, until / rate / 2)))
        current = status()
    raise RuntimeError('Performance did not reach the requested phase')
