#!/usr/bin/env python3
"""Build and verify one CINE pack. Host Python 3.14; editor uses its bundled Python.

Usage: python3 assets/packs/build_pack.py PhotoOrbit
Every invocation uses fresh work/packs/<Pack>/run-* stages. Never installs.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import wave

HERE = Path(__file__).resolve().parent
CINE = HERE.parents[1]
REPO = CINE.parents[1]
WORKSPACE = REPO.parent
WORK = CINE / "work/packs"
LINUX = WORKSPACE / "CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine"
WINE_TOOLS = WORKSPACE / "CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P"
WINDOWS = WINE_TOOLS / "work/ue56"
RETOC = REPO / "build/retoc-css-target/release/retoc"
DOTNET_ROOT = Path.home() / ".local/share/mise/installs/dotnet/10.0.401"
DUMP = REPO / "work/research/GameDump-tool/bin/Release/net10.0/GameDump.dll"
GAME = Path("/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2")
USMAP = GAME / "Binaries/Win64/ue4ss/MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241+1339-d7e7826d.usmap"
PACKS = {
    "IrisOut": ("Iris Out", "IRIS OUT", ["iris-out", "iris-out-2", "iris-out-9x16", "iris-out-static", "iris-out-static-1", "iris-out-eve-rig", "iris-out-unholy-rig", "iris-out-white-rig"]),
    "SomethingAintRight": ("Something Ain't Right", "XG - SOMETHING AIN'T RIGHT", ["showreel-124", "showreel-finale", "commander-white-release", "commander-white-release-9x16"]),
    "TheyKnowNotWhatTheyDo": ("They Know Not What They Do", "Polyphia - THEY KNOW NOT WHAT THEY DO", ["profane-proxima"]),
    "MarkOfTheBeast": ("Mark of the Beast", "Polyphia - Mark of the Beast", [f"beast-{shell}{i}" for shell in ("p", "g") for i in range(5)]),
    "PoseGlide": ("Pose Glide", "", [f"pose-glide-{n}" for n in (3, 5, 7, 10)]),
    "Turntable": ("Turntable", "", ["turntable", "breakdown-turntable"]),
    "Walks": ("Walks", "", ["fashion-walk-360", "commander-white-walk"]),
    "PhotoOrbit": ("Photo Orbit", "", ["photo-orbit"]),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def record(path):
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest(path)}


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def strict_json(text):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, "Duplicate JSON key: " + key)
            result[key] = value
        return result
    def constant(value):
        raise ValueError("Nonfinite JSON number: " + value)
    return json.loads(text, object_pairs_hook=pairs, parse_constant=constant)


def prepare(pack):
    title, song, presets = PACKS[pack]
    rows, audio, inputs = [], {}, {}
    for preset_id in presets:
        path = CINE / "presets" / (preset_id + ".cine.json")
        raw = path.read_bytes().decode("utf-8")
        preset = strict_json(raw)
        require(preset.get("schema") == 1 and preset.get("id") == preset_id, "Bad preset identity: " + str(path))
        require(preset.get("name") and preset.get("kind"), "Missing preset name/kind")
        inputs[str(path)] = record(path)
        track = preset.get("audio", "")
        if track:
            require(isinstance(track, str) and re.fullmatch(r"audio/[A-Za-z0-9_-]+\.wav", track), "Bad audio path")
            audio_id = Path(track).stem
            source = CINE / track
            inputs[str(source)] = record(source)
            with wave.open(str(source), "rb") as wav:
                require(wav.getnchannels() == 2 and wav.getcomptype() == "NONE", "Expected stereo PCM WAV")
                frames, rate, channels, width = wav.getnframes(), wav.getframerate(), wav.getnchannels(), wav.getsampwidth()
                require(frames > 0 and rate > 0 and width in (2, 3, 4), "Invalid WAV metadata")
                actual_bytes = 0
                while block := wav.readframes(65536):
                    actual_bytes += len(block)
                require(actual_bytes == frames * channels * width, "Truncated WAV: " + str(source))
            name = "SW_" + audio_id.replace("-", "_")
            asset = f"/Game/CSSX/CINE/{pack}/Audio/{name}.{name}"
            require(all(a["asset"] != asset or key == audio_id for key, a in audio.items()), "Audio asset name collision")
            audio[audio_id] = {"asset": asset, "seconds": frames / rate, "rate": rate, "channels": channels,
                               "frames": frames, "sample_width": width, "source": str(source)}
            pattern = r'("audio"\s*:\s*)' + re.escape(json.dumps(track))
            raw, replacements = re.subn(pattern, lambda match: match[1] + json.dumps(audio_id), raw)
            require(replacements == 1, "Audio replacement must affect exactly one field")
            expected = dict(preset, audio=audio_id)
            require(strict_json(raw) == expected, "Preset changed beyond audio id")
        rows.append({"Name": "preset." + preset_id, "Tag": "None", "DevComment": raw})
    extras = HERE / "data" / pack
    for path in sorted(extras.glob("*.json")):
        require(re.fullmatch(r"(?:recipe|style)\.[A-Za-z0-9_.-]+", path.stem), "Extra rows must be recipe.* or style.*")
        raw = path.read_bytes().decode("utf-8")
        require(isinstance(strict_json(raw), dict), "Extra row must contain a JSON object")
        rows.append({"Name": path.stem, "Tag": "None", "DevComment": raw})
        inputs[str(path)] = record(path)
    require(len({r["Name"].casefold() for r in rows}) == len(rows), "Case-insensitive row collision")
    manifest = {"pack_schema": 1, "pack": pack, "name": title, "version": "1.0.0", "song": song,
                "presets": presets, "audio": {key: {k: a[k] for k in ("asset", "seconds", "rate", "channels")} for key, a in audio.items()},
                "requires": {"cine": "2.0.0"}}
    rows.insert(0, {"Name": "manifest", "Tag": "None", "DevComment": json.dumps(manifest, ensure_ascii=False, separators=(",", ":"))})
    return {"pack": pack, "audio": audio, "inputs": list(inputs.values())}, rows


def run(command, log, env=None, cwd=None):
    print("Running", log.name, flush=True)
    write_json(log.with_suffix(log.suffix + ".command.json"), {"argv": list(map(str, command)), "cwd": str(cwd) if cwd else None})
    with log.open("w") as stream:
        result = subprocess.run(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT, env=env, cwd=cwd)
    require(result.returncode == 0, f"Command exited {result.returncode}: {log}")
    return log.read_text(errors="replace")


def editor(stage, mode):
    env = dict(os.environ, CINE_PACK_MODE=mode, PYTHONDONTWRITEBYTECODE="1",
               XDG_CONFIG_HOME=str(stage / "config-home"), XDG_CACHE_HOME=str(stage / "cache"))
    run(["bwrap", "--ro-bind", "/", "/", "--bind", stage, stage, "--bind", stage / "scratch", "/tmp",
         "--bind", stage / "config-home", Path.home() / ".config/Epic",
         "--dev-bind", "/dev", "/dev", "--proc", "/proc", "--unshare-net", "--die-with-parent", "--",
         LINUX / "Binaries/Linux/UnrealEditor-Cmd", stage / "CinePack.uproject",
         "-unattended", "-nosplash", "-NullRHI", "-NoSound", "-NoShaderCompile", "-ddc=NoZenLocalFallback", "-numcores=6",
         "-ini:EditorSettings:/Script/UnrealEd.AnalyticsPrivacySettings:bSendUsageData=False",
         f"-LocalDataCachePath={stage}/ddc", f"-UserDir={stage}/user/", f"-abslog={stage}/{mode}.log",
         "-run=pythonscript", f"-script={HERE}/editor_pack.py"], stage / (mode + "-console.log"), env=env)
    require((stage / (mode + "-verified.json")).is_file(), f"Editor {mode} did not finish")


def cook(stage, target, logs):
    require(str(target).startswith("/home/"), "Cook must be under /home")
    shutil.copytree(stage / "Content", target / "project/Content")
    shutil.copytree(stage / "Config", target / "project/Config")
    write_json(target / "project/CinePack.uproject", {"FileVersion": 3, "EngineAssociation": "5.6"})
    (target / "out").mkdir()
    socket_dir = WINE_TOOLS / "work/docker1"
    # The short socket path also works in the private daemon's mount namespace.
    docker = ["docker", "--host", f"unix:///proc/{os.getpid()}/cwd/docker.sock"]
    old = Path.cwd()
    os.chdir(socket_dir)
    try:
        env = dict(os.environ, DOCKER_CONFIG=str(target / "client"), TMPDIR=str(target / "out"))
        image = run([*docker, "image", "inspect", "unholy-wine:11.7", "--format", "{{.Id}}"], logs / "image.log", env).strip()
        run([*docker, "run", "--rm", "--init", "--user", "0:0", "--name", f"cine-pack-{os.getpid()}",
             "--network", "none", "--shm-size", "1g",
             "--mount", f"type=bind,src={WINDOWS},dst=/ue,readonly",
             "--mount", f"type=bind,src={target}/project,dst=/project",
             "--mount", f"type=bind,src={target}/out,dst=/out",
             "--mount", f"type=bind,src={HERE}/cook-inner.sh,dst=/cook.sh,readonly",
             image, "bash", "/cook.sh"], logs / "cook.log", env)
    finally:
        os.chdir(old)
    return image


def verify_exports(dump_dir, spec, rows):
    pack = spec["pack"]
    exports = {}
    for path in dump_dir.glob("*.json"):
        for obj in json.loads(path.read_text()):
            require(obj["Name"] not in exports, "Duplicate export")
            exports[obj["Name"]] = obj
    table = exports.pop("DT_CINE_Pack")
    require(table["Type"] == "DataTable", "Wrong table class")
    row_struct = table["Properties"]["RowStruct"]
    require(row_struct.get("ObjectName") == "Class'GameplayTagTableRow'" and
            row_struct.get("ObjectPath") == "/Script/GameplayTags", "Wrong cooked row struct")
    require(set(table["Rows"]) == {r["Name"] for r in rows}, "Cooked row set differs")
    for row in rows:
        actual = table["Rows"][row["Name"]]
        require(actual.get("Tag", "None") == "None", "Cooked Tag is not None")
        require(actual["DevComment"].encode() == row["DevComment"].encode(), "Cooked row bytes differ: " + row["Name"])
    audio_result = {}
    for audio_id, expected in spec["audio"].items():
        name = expected["asset"].rsplit(".", 1)[1]
        obj = exports.pop(name)
        require(obj["Type"] == "SoundWave", "Wrong audio class")
        props = obj["Properties"]
        require(props["NumChannels"] == expected["channels"] and props["SampleRate"] == expected["rate"], "Cooked audio metadata differs")
        require(abs(props["Duration"] - expected["seconds"]) <= max(1 / expected["rate"], 0.00002), "Cooked duration differs")
        require(props["LoadingBehavior"] == "ESoundWaveLoadingBehavior::ForceInline", "Cooked sound is not ForceInline")
        require(not props.get("bLooping", False) and props.get("SoundClassObject") is None, "Unexpected sound routing/loop")
        formats = obj["CompressedFormatData"]
        require(formats and obj["RunningPlatformData"] is None, "Missing inline compressed data")
        require(len(formats) == 1 and next(iter(formats)).startswith("BINKA"), "Expected cooked Bink audio")
        require(all(item["ElementCount"] > 0 and item["SizeOnDisk"] > 0 for item in formats.values()), "Empty cooked audio payload")
        audio_result[audio_id] = {"asset": expected["asset"], "properties": props, "formats": formats,
                                  "compressed_bytes": sum(item["SizeOnDisk"] for item in formats.values())}
    require(not exports, "Unexpected exports: " + str(list(exports)))
    return audio_result


def clean_caches(run_dir):
    removed = []
    for relative in ("stage/scratch", "stage/ddc", "stage/cache", "stage/Intermediate",
                     "cook/out/prefix", "cook/out/ddc", "cook/out/tmp", "cook/project/Intermediate",
                     "cook/out/user/Intermediate", "cook/out/user/Saved/ShaderDebugInfo", "cook/project/Content",
                     "cook/project/Saved/Cooked/Windows/Engine"):
        path = run_dir / relative
        if path.is_dir():
            size = sum(p.stat().st_size for p in path.rglob("*") if p.is_file() and not p.is_symlink())
            shutil.rmtree(path)
            removed.append({"path": str(path), "bytes": size})
    return removed


def build(pack):
    spec, rows = prepare(pack)
    target = WORK / pack
    target.mkdir(parents=True, exist_ok=True)
    require(not (target / "receipt.json").exists(), "Pack already has a receipt; preserve or move the previous build before rebuilding")
    run_dir = target / datetime.now(timezone.utc).strftime("run-%Y%m%dT%H%M%S%fZ")
    stage, logs = run_dir / "stage", run_dir / "logs"
    logs.mkdir(parents=True)
    for folder in ("scratch", "ddc", "user", "Content", "Config", "config-home", "cache"):
        (stage / folder).mkdir(parents=True)
    write_json(stage / "input.json", spec)
    write_json(stage / "rows.json", rows)
    write_json(stage / "CinePack.uproject", {"FileVersion": 3, "EngineAssociation": "5.6", "Plugins": [
        {"Name": "PythonScriptPlugin", "Enabled": True}, {"Name": "EditorScriptingUtilities", "Enabled": True},
        {"Name": "AndroidFileServer", "Enabled": False}]})
    (stage / "Config/DefaultEngine.ini").write_text("[/Script/WindowsTargetPlatform.WindowsTargetSettings]\nbResampleForDevice=False\n")
    (stage / "Config/DefaultGame.ini").write_text("[/Script/UnrealEd.ProjectPackagingSettings]\nbCookAll=True\nbCookMapsOnly=False\nbShareMaterialShaderCode=False\nbSharedMaterialNativeLibraries=False\nbUseIoStore=False\nUsePakFile=False\nbUseUnversionedPropertySerialization=False\n")
    started = time.monotonic()
    try:
        editor(stage, "author")
        editor(stage, "reload")
        image = cook(stage, run_dir / "cook", logs)
        source = run_dir / f"cook/project/Saved/Cooked/Windows/CinePack/Content/CSSX/CINE/{pack}"
        files = sorted(p for p in source.rglob("*") if p.is_file())
        require(files and all(p.suffix in (".uasset", ".uexp", ".ubulk") for p in files), "Unexpected cooked file set")
        expected = {"DT_CINE_Pack"} | {"Audio/" + a["asset"].rsplit(".", 1)[1] for a in spec["audio"].values()}
        actual = {str(p.relative_to(source).with_suffix("")) for p in files if p.suffix == ".uasset"}
        require(actual == expected, "Cooked package set differs")
        require(all((source / (name + ".uexp")).is_file() for name in expected), "Missing export payload")
        legacy = run_dir / "legacy"
        shutil.copytree(source, legacy / f"MortalShell2/Content/CSSX/CINE/{pack}")
        container = f"CINE_{pack}_eins0fx_P"
        output = run_dir / container
        output.mkdir()
        utoc = output / (container + ".utoc")
        run([RETOC, "to-zen", "--version", "UE5_6", legacy, utoc], logs / "retoc-pack.log")
        run([RETOC, "verify", utoc], logs / "retoc-verify.log")
        run([RETOC, "list", utoc], logs / "retoc-list.log")
        trio = sorted(output.iterdir())
        require({p.suffix for p in trio} == {".pak", ".ucas", ".utoc"} and len(trio) == 3, "Invalid output trio")
        mount = run_dir / "readback-mount"
        mount.mkdir()
        for path in trio:
            (mount / path.name).symlink_to(path)
        for suffix in (".utoc", ".ucas"):
            (mount / ("global" + suffix)).symlink_to(GAME / "Content/Paks" / ("global" + suffix))
        dump_dir = run_dir / "dump"
        env = dict(os.environ, DOTNET_ROOT=str(DOTNET_ROOT))
        dump_cmd = [DOTNET_ROOT / "dotnet", DUMP, mount, USMAP, dump_dir]
        run([*dump_cmd, "list"], logs / "dump-list.log", env)
        listed = (dump_dir / "files.txt").read_text().splitlines()
        require({p.casefold() for p in listed} == {f"mortalshell2/content/cssx/cine/{pack}/{name}.uasset".casefold() for name in expected}, "Container paths differ: " + repr(listed))
        result = run([*dump_cmd, "export", f"/CSSX/CINE/{pack}/"], logs / "dump-export.log", env)
        require(f"exported {len(expected)}, failed 0" in result, "GameDump did not export every package")
        audio = verify_exports(dump_dir, spec, rows)
        roundtrip = run_dir / "roundtrip"
        run([RETOC, "to-legacy", "--version", "UE5_6", "--no-script-objects", mount, roundtrip], logs / "retoc-roundtrip.log")
        bulk_checks = []
        for path in sorted(p for p in legacy.rglob("*") if p.suffix in (".uexp", ".ubulk")):
            recovered = roundtrip / path.relative_to(legacy)
            require(recovered.is_file() and digest(recovered) == digest(path), "Packed payload differs: " + str(path))
            bulk_checks.append({"path": str(path.relative_to(legacy)), "sha256": digest(path), "bytes": path.stat().st_size})
        for item in spec["inputs"]:
            require(record(Path(item["path"])) == item, "Input changed during build: " + item["path"])
        receipt = {"status": "passed", "pack": pack, "container": container, "installed": False,
                   "run": str(run_dir), "elapsed_seconds": time.monotonic() - started,
                   "inputs": spec["inputs"], "source_bytes": sum(i["bytes"] for i in spec["inputs"]),
                   "canonical_rows": record(stage / "rows.json"), "row_count": len(rows),
                   "trio": [record(p) for p in trio], "trio_bytes": sum(p.stat().st_size for p in trio),
                   "packages": sorted(f"/Game/CSSX/CINE/{pack}/{name}" for name in expected),
                   "cooked_files": [record(p) for p in files], "audio": audio, "payload_roundtrip": bulk_checks,
                   "tools": {"host_python": sys.version, "linux_engine": json.loads((LINUX / "Build/Build.version").read_text()),
                             "windows_engine": json.loads((WINDOWS / "Engine/Build/Build.version").read_text()),
                             "wine_image": image, "retoc_version": run([RETOC, "--version"], logs / "retoc-version.log").strip(),
                             "dotnet_version": run([DOTNET_ROOT / "dotnet", "--version"], logs / "dotnet-version.log", env).strip(),
                             "retoc": record(RETOC), "gamedump": record(DUMP),
                             "cue4parse": record(DUMP.parent / "CUE4Parse.dll"), "old_build_usmap": record(USMAP)},
                   "scripts": [record(p) for p in (HERE / "build_pack.py", HERE / "editor_pack.py", HERE / "cook-inner.sh")],
                   "logs": [str(p) for p in sorted(run_dir.rglob("*.log"))]}
        receipt["deleted_caches"] = clean_caches(run_dir)
        receipt["logs"] = [path for path in receipt["logs"] if Path(path).is_file()]
        write_json(target / "receipt.json", receipt)
        print(f"PASS {pack}: {len(rows)} rows, {len(audio)} SoundWaves, {receipt['trio_bytes']:,} trio bytes", flush=True)
    except Exception as error:
        write_json(run_dir / "failure.json", {"error": str(error), "elapsed_seconds": time.monotonic() - started})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", choices=PACKS)
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / "build.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        build(args.pack)


if __name__ == "__main__":
    main()
