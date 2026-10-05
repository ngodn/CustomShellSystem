"""Stage, verify or cook private shared facial meshes using the audited UE runner."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("work", type=Path)
    parser.add_argument("--mode", choices=("create", "verify", "cook", "map"), required=True)
    args = parser.parse_args()
    work = args.work.resolve()
    if not work.is_relative_to(ROOT / "work/cine-animation-port"):
        raise ValueError("Use the private animation workspace")
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
        script = "export_facial_vertex_maps.py" if args.mode == "map" else "stage_facial_library.py"
        command += ["-run=pythonscript", "-script=" + str(Path(__file__).with_name(script))]
    command += ["-abslog=" + str(work / (args.mode + "-engine.log"))]
    environment = os.environ.copy()
    environment["CSS_ANIM_WORK"] = str(work)
    environment["CSS_FACE_STAGE_MODE"] = args.mode
    with (work / (args.mode + "-command.json")).open("x") as file:
        json.dump(command, file, indent=2)
    with (work / (args.mode + "-console.log")).open("x") as log:
        result = subprocess.run(["nice", "-n", "15", *command], env=environment, stdout=log, stderr=subprocess.STDOUT)
    with (work / (args.mode + "-exit.json")).open("x") as file:
        json.dump({"exit_code": result.returncode}, file)
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
