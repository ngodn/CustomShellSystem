"""Stage unchanged garment sources in an isolated UE 5.6.1 shader experiment."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_content", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--render-fixtures", action="store_true",
                        help="Link the source mesh and existing CSSAuthoring editor module read-only")
    parser.add_argument("--eve-hair", type=Path,
                        help="Eve hair-v14-material2 directory containing authored.json")
    parser.add_argument("--commander-hair", type=Path,
                        help="Commander White runtime5 directory containing staged.json")
    args = parser.parse_args()
    source = args.source_content.resolve()
    target = args.destination.resolve()
    if target == source or source in target.parents:
        parser.error("The experiment must be outside the source content")
    target.mkdir(parents=True, exist_ok=False)
    rows = []
    parents = ["/Game/CSS/UnholyGenessa/" + name for name in (
        "Mat1/M_Trim", "Mat10/M_Trim", "Mat12/M_Metal",
        "Fabric02/M_Fabric", "Fabric02/M_Silk")]

    def copy_package(path, relative, expected=None):
        output = target / "Content" / relative
        if output.exists():
            raise FileExistsError(output)
        output.parent.mkdir(parents=True, exist_ok=True)
        before = digest(path)
        if expected is not None and before != expected:
            raise RuntimeError(f"Source differs from authoring receipt: {path}")
        shutil.copyfile(path, output)
        if digest(output) != before or digest(path) != before:
            raise RuntimeError(f"Source copy changed: {relative}")
        rows.append({"source": str(path), "relative": str(relative), "sha256": before})

    for directory in ("Mat1", "Mat10", "Mat12", "Fabric02"):
        files = sorted((source / "CSS/UnholyGenessa" / directory).glob("*.uasset"))
        if not files:
            raise FileNotFoundError(directory)
        for path in files:
            copy_package(path, path.relative_to(source))
    if args.eve_hair:
        hair = args.eve_hair.resolve()
        packages = json.loads((hair / "authored.json").read_text())["packages"]
        textures = json.loads((hair / "inputs.json").read_text())["textures"]
        packages.update({key: value["sha256"] for key, value in textures.items()})
        for package, expected in packages.items():
            if not package.startswith("/Game/CSS/") or ".." in package.split("/"):
                raise ValueError(f"Invalid source package: {package}")
            relative = Path(package.removeprefix("/Game/") + ".uasset")
            copy_package(hair / "ue/Content" / relative, relative, expected)
        parents.append("/Game/CSS/EveHair/M_Hair1")
    if args.commander_hair:
        hair = args.commander_hair.resolve()
        packages = json.loads((hair / "staged.json").read_text())["sources"]
        for package, expected in packages.items():
            if not package.startswith("/Game/CSS/") or ".." in package.split("/"):
                raise ValueError(f"Invalid source package: {package}")
            relative = Path(package.removeprefix("/Game/") + ".uasset")
            copy_package(hair / "windows/Content" / relative, relative, expected)
        parents.append("/Game/CSS/CommanderWhite/MaterialY/M_Hair")
    project = {"FileVersion": 3, "EngineAssociation": "5.6", "Plugins": [
        {"Name": "PythonScriptPlugin", "Enabled": True},
        {"Name": "EditorScriptingUtilities", "Enabled": True}]}
    if args.render_fixtures:
        authoring = source.parent
        original = json.loads((authoring / "CSSAuthoring.uproject").read_text())
        module = authoring / "Binaries/Linux/libUnrealEditor-CSSAuthoring.so"
        mesh = source / "CSS/UnholyGenessa/SK_EveW3.uasset"
        if not module.is_file() or not mesh.is_file():
            raise FileNotFoundError("The compiled authoring module and SK_EveW3 are required")
        for path in source.rglob("*"):
            if not path.is_file():
                continue
            output = target / "Content" / path.relative_to(source)
            if not output.exists():
                output.parent.mkdir(parents=True, exist_ok=True)
                output.symlink_to(path)
        (target / "Binaries").symlink_to(authoring / "Binaries", target_is_directory=True)
        project["Modules"] = original["Modules"]
        plugins = {p["Name"]: p for p in project["Plugins"]}
        plugins.update({p["Name"]: p for p in original["Plugins"]})
        project["Plugins"] = list(plugins.values())
        protected = {row["source"]: row["sha256"] for row in rows}
        protected.update({str(path): digest(path) for path in (module, mesh)})
        (target / "render-sources.json").write_text(json.dumps(protected, indent=2) + "\n")
    (target / "AstralMaterials.uproject").write_text(json.dumps(project, indent=2) + "\n")
    config = target / "Config"
    config.mkdir()
    (config / "DefaultEngine.ini").write_text(
        "[/Script/Engine.RendererSettings]\n"
        "r.SkinCache.CompileShaders=True\n"
        "r.RayTracing=False\n"
        "[/Script/WindowsTargetPlatform.WindowsTargetSettings]\n"
        "DefaultGraphicsRHI=DefaultGraphicsRHI_DX12\n"
        "!D3D12TargetedShaderFormats=ClearArray\n"
        "+D3D12TargetedShaderFormats=PCD3D_SM6\n"
        "!D3D11TargetedShaderFormats=ClearArray\n"
        "+D3D11TargetedShaderFormats=PCD3D_SM5\n")
    (config / "DefaultGame.ini").write_text(
        "[/Script/UnrealEd.ProjectPackagingSettings]\n"
        "bShareMaterialShaderCode=False\nbSharedMaterialNativeLibraries=False\n")
    (target / "source-manifest.json").write_text(json.dumps(rows, indent=2) + "\n")
    (target / "material-parents.json").write_text(json.dumps(parents, indent=2) + "\n")
    print(f"Staged {len(rows)} source packages in {target}")


if __name__ == "__main__":
    main()
