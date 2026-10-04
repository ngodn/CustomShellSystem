"""Stage the CSS shared editor module with read-only source asset links."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source_content", type=Path)
parser.add_argument("destination", type=Path)
args = parser.parse_args()
source = args.source_content.resolve(strict=True)
destination = args.destination.resolve()
if not source.is_dir() or source == destination or source in destination.parents:
    parser.error("Use a new destination outside the source content")
destination.mkdir(parents=True, exist_ok=False)
shutil.copytree(Path(__file__).with_name("authoring"), destination, dirs_exist_ok=True)
for path in source.rglob("*"):
    if path.is_file():
        output = destination / "Content" / path.relative_to(source)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.symlink_to(path.resolve(strict=True))
for directory in ("scratch", "cache", "ddc", "user", "Config"):
    (destination / directory).mkdir()
print(destination)
