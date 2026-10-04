"""Capture owned astral actors through an installed MS2AttackProbe, without game input.

Python 3.14. Temporarily supplies the probe's on-demand inspection function, then
restores it. Does not reload a mod or change the player's appearance or abilities.
"""
import argparse
import fcntl
from pathlib import Path
import signal
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--seconds", type=int, default=1)
    parser.add_argument("--mode", choices=("actors", "materials"), default="actors")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 180:
        parser.error("seconds must be 1..180")
    script = args.probe / "Scripts/RepairPrologue.lua"
    request = args.probe / "request.txt"
    source = "capture.lua" if args.mode == "actors" else "source-materials.lua"
    payload = Path(__file__).with_name(source).read_bytes()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    def interrupted(signum, frame):
        raise InterruptedError(f"Capture interrupted by signal {signum}")
    signal.signal(signal.SIGTERM, interrupted)
    with (args.probe / "astral-capture.lock").open("a") as lock, args.output.open("x") as output:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        original = script.read_bytes()
        script.write_bytes(payload)
        rid = ""
        try:
            until = time.monotonic() + args.seconds
            while time.monotonic() < until:
                rid = f"repair_astral_{time.time_ns()}"
                response = args.probe / f"repair-{rid}.log"
                request.write_text(rid + "\n")
                deadline = time.monotonic() + 8
                while time.monotonic() < deadline:
                    text = response.read_text() if response.exists() else ""
                    if "ASTRAL_END" in text or "ERROR " in text or "REFUSED " in text:
                        output.write(f"REQUEST {rid}\n{text}\n")
                        output.flush()
                        if "ASTRAL_END" not in text:
                            raise RuntimeError(f"Probe failed; see {args.output}")
                        break
                    time.sleep(.1)
                else:
                    raise TimeoutError("No probe response; capture stopped")
                time.sleep(.25)
        finally:
            if request.exists() and request.read_text().strip() == rid:
                request.write_text("")
            if script.read_bytes() == payload:
                script.write_bytes(original)
            else:
                raise RuntimeError("Probe entry changed concurrently; not overwriting it")
    print(args.output)


if __name__ == "__main__":
    main()
