"""Stage, verify or cook private shared facial meshes using the audited UE runner."""
import argparse
import json
import os
import re
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path)
    parser.add_argument("--mode", choices=("create", "verify", "cook", "map", "import", "inspect"), required=True)
    parser.add_argument("--revision", default="")
    parser.add_argument("--source-work", type=Path,
                        help="Verified facial export used by create/verify")
    parser.add_argument("--staging-work", type=Path,
                        help="Verified shared meshes used by map")
    args = parser.parse_args()
    if args.revision and (args.mode not in ("import", "inspect") or
                          not re.fullmatch(r"[A-Za-z0-9]{1,24}", args.revision)):
        raise ValueError("A candidate revision is only valid for import/inspect")
    work = args.work.resolve()
    if not work.is_relative_to(ROOT / "work/cine-animation-port"):
        raise ValueError("Use the private animation workspace")
    source_work = (args.source_work or work.parent / "facial-export-v1").resolve(strict=True)
    if (not source_work.is_relative_to(ROOT / "work/cine-animation-port") or
            (args.source_work and args.mode not in ("create", "verify"))):
        raise ValueError("Use a private verified facial export for create/verify")
    staging_work = (args.staging_work or work.parent / "shared-faces-v1").resolve(strict=True)
    if (not staging_work.is_relative_to(ROOT / "work/cine-animation-port") or
            (args.staging_work and args.mode != "map")):
        raise ValueError("Use a private verified staging workspace for map")
    template = ROOT / "work/cine-animation-port/shared-v1"
    command = json.loads((template / "cook-command.json").read_text())
    command = [value.replace(str(template), str(work)) for value in command
               if value != "-Cook" and not value.startswith(("-run=", "-List=", "-Output=", "-abslog=", "-WindowsRoot="))]
    if command[0] != "bwrap" or "-NullRHI" not in command or "-NoShaderCompile" not in command:
        raise ValueError("Expected the audited headless authoring command")
    work.mkdir(parents=True, exist_ok=True)
    (work / "tmp").mkdir(exist_ok=True)
    if args.mode == "cook":
        report = json.loads((work / "facial-stage-verify.json").read_text())
        if not report.get("complete") or len(report["results"]) != 6:
            raise ValueError("Fresh-process facial verification must pass before cooking")
        listing = work / "cook.txt"
        if listing.exists() or (work / "cooked").exists():
            raise FileExistsError("Facial cook output already exists")
        listing.write_text("\n".join(row["target"] for row in report["results"]) + "\n")
        command += ["-run=CSSCookAssets", "-Cook", "-WindowsRoot=C:/CSS", "-List=" + str(listing), "-Output=" + str(work / "cooked")]
    else:
        script = {"map": "export_facial_vertex_maps.py", "import": "import_facial_shapes.py",
                  "inspect": "verify_facial_shapes.py"}.get(args.mode, "stage_facial_library.py")
        command += ["-run=pythonscript", "-script=" + str(Path(__file__).with_name(script))]
    command += ["-abslog=" + str(work / (args.mode + "-engine.log"))]
    environment = os.environ.copy()
    environment["CSS_ANIM_WORK"] = str(work)
    environment["CSS_FACE_STAGE_MODE"] = args.mode
    environment["CSS_FACE_REVISION"] = args.revision
    environment["CSS_FACE_INPUT_WORK"] = str(source_work)
    environment["CSS_FACE_MAP_WORK"] = str(staging_work)
    environment.pop("CSS_FACE_SOURCE", None)
    with (work / (args.mode + "-command.json")).open("x") as file:
        json.dump(command, file, indent=2)
    with (work / (args.mode + "-console.log")).open("x") as log:
        result = subprocess.run(["nice", "-n", "15", *command], env=environment, stdout=log, stderr=subprocess.STDOUT)
    with (work / (args.mode + "-exit.json")).open("x") as file:
        json.dump({"exit_code": result.returncode}, file)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
