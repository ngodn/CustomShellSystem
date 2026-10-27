"""Exercise the readback gate against a real build and deliberately damaged copies.

Usage: python3 -B assets/packs/check_readback.py work/packs/<Pack>/run-<UTC>
"""
import argparse
import copy
import json
from pathlib import Path
import tempfile

from build_pack import WORK, require, verify_exports, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    args = parser.parse_args()
    source = args.run.resolve()
    spec = json.loads((source / "stage/input.json").read_text())
    rows = json.loads((source / "stage/rows.json").read_text())
    original = {p.name: json.loads(p.read_text()) for p in (source / "dump").glob("*.json")}
    table_file = next(name for name, objects in original.items() if objects[0]["Type"] == "DataTable")
    tests = []

    def trial(label, mutate):
        changed = copy.deepcopy(original)
        mutate(changed)
        with tempfile.TemporaryDirectory(dir=WORK, prefix="readback-control-") as temp:
            root = Path(temp)
            for name, objects in changed.items():
                write_json(root / name, objects)
            try:
                verify_exports(root, spec, rows)
            except (ValueError, KeyError):
                tests.append(label)
            else:
                raise AssertionError("Damaged readback accepted: " + label)

    verify_exports(source / "dump", spec, rows)
    trial("changed DevComment rejected", lambda data: data[table_file][0]["Rows"]["manifest"].update(DevComment="{}"))
    trial("missing row rejected", lambda data: data[table_file][0]["Rows"].pop("manifest"))
    trial("nonempty Tag rejected", lambda data: data[table_file][0]["Rows"]["manifest"].update(Tag="Unexpected.Tag"))
    trial("wrong row struct rejected", lambda data: data[table_file][0]["Properties"].update(RowStruct={"ObjectName": "Wrong"}))
    trial("wrong row struct module rejected", lambda data: data[table_file][0]["Properties"]["RowStruct"].update(ObjectPath="/Script/Wrong"))
    if spec["audio"]:
        sound_file = next(name for name, objects in original.items() if objects[0]["Type"] == "SoundWave")
        trial("resampled audio rejected", lambda data: data[sound_file][0]["Properties"].update(SampleRate=16000))
        trial("missing platform data rejected", lambda data: data[sound_file][0].update(CompressedFormatData=None))
        trial("empty compressed audio rejected", lambda data: next(iter(data[sound_file][0]["CompressedFormatData"].values())).update(SizeOnDisk=0))
    require(tests, "No negative controls ran")
    write_json(source / "readback-controls.json", {"undamaged": "passed", "rejected": tests})
    print(f"PASS: undamaged {spec['pack']} accepted; {len(tests)} damaged copies rejected")


if __name__ == "__main__":
    main()
